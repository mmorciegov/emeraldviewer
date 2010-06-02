/** 
 * @file llviewercontrol.h
 * @brief references to viewer-specific control files
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLVIEWERCONTROL_H
#define LL_LLVIEWERCONTROL_H

#include <map>
#include "llcontrol.h"

// Enabled this definition to compile a 'hacked' viewer that
// allows a hacked godmode to be toggled on and off.
#define TOGGLE_HACKED_GODLIKE_VIEWER 
#ifdef TOGGLE_HACKED_GODLIKE_VIEWER
extern BOOL gHackGodmode;
#endif

// These functions found in llcontroldef.cpp *TODO: clean this up!
//setting variables are declared in this function
void settings_setup_listeners();

extern std::map<std::string, LLControlGroup*> gSettings;

// for the graphics settings
void create_graphics_group(LLControlGroup& group);

// saved at end of session
extern LLControlGroup gSavedSettings;
extern LLControlGroup gSavedPerAccountSettings;

// Read-only
extern LLControlGroup gColors;

// Saved at end of session
extern LLControlGroup gCrashSettings;

// Set after settings loaded
extern std::string gLastRunVersion;
extern std::string gCurrentVersion;

//! Helper function for LLCachedControl
template <class T> 
eControlType get_control_type(const T& in, LLSD& out)
{
	llerrs << "Usupported control type: " << typeid(T).name() << "." << llendl;
	return TYPE_COUNT;
}

//! Publish/Subscribe object to interact with LLControlGroups.

//! An LLCachedControl instance to connect to a LLControlVariable
//! without have to manually create and bind a listener to a local
//! object.
template <class T>
class LLCachedControl
{
    T mCachedValue;
    LLPointer<LLControlVariable> mControl;
    boost::signals::connection mConnection;

public:
	LLCachedControl(const std::string& name, 
					const T& default_value, 
					const std::string& comment = "Declared In Code")
	{
		mControl = gSavedSettings.getControl(name);
		if(mControl.isNull())
		{
			declareTypedControl(gSavedSettings, name, default_value, comment);
			mControl = gSavedSettings.getControl(name);
			if(mControl.isNull())
			{
				llerrs << "The control could not be created!!!" << llendl;
			}

			mCachedValue = default_value;
		}
		else
		{
			mCachedValue = (const T&)mControl->getValue();
		}

		// Add a listener to the controls signal...
		mControl->getSignal()->connect(
			boost::bind(&LLCachedControl<T>::handleValueChange, this, _1)
			);
	}

	~LLCachedControl()
	{
		if(mConnection.connected())
		{
			mConnection.disconnect();
		}
	}

	LLCachedControl& operator =(const T& newvalue)
	{
	   setTypeValue(*mControl, newvalue);
	}

	operator const T&() { return mCachedValue; }

private:
	void declareTypedControl(LLControlGroup& group, 
							 const std::string& name, 
							 const T& default_value,
							 const std::string& comment)
	{
		LLSD init_value;
		eControlType type = get_control_type<T>(default_value, init_value);
		if(type < TYPE_COUNT)
		{
			group.declareControl(name, type, init_value, comment, FALSE);
		}
	}

	bool handleValueChange(const LLSD& newvalue)
	{
		mCachedValue = (const T &)newvalue;
		return true;
	}

	void setTypeValue(LLControlVariable& c, const T& v)
	{
		// Implicit conversion from T to LLSD...
		c.set(v);
	}
};

template <> eControlType get_control_type<U32>(const U32& in, LLSD& out);
template <> eControlType get_control_type<S32>(const S32& in, LLSD& out);
template <> eControlType get_control_type<F32>(const F32& in, LLSD& out);
template <> eControlType get_control_type<bool> (const bool& in, LLSD& out); 
// Yay BOOL, its really an S32.
//template <> eControlType get_control_type<BOOL> (const BOOL& in, LLSD& out) 
template <> eControlType get_control_type<std::string>(const std::string& in, LLSD& out);
template <> eControlType get_control_type<LLVector3>(const LLVector3& in, LLSD& out);
template <> eControlType get_control_type<LLVector3d>(const LLVector3d& in, LLSD& out); 
template <> eControlType get_control_type<LLRect>(const LLRect& in, LLSD& out);
template <> eControlType get_control_type<LLColor4>(const LLColor4& in, LLSD& out);
template <> eControlType get_control_type<LLColor3>(const LLColor3& in, LLSD& out);
template <> eControlType get_control_type<LLColor4U>(const LLColor4U& in, LLSD& out); 
template <> eControlType get_control_type<LLSD>(const LLSD& in, LLSD& out);

//#define TEST_CACHED_CONTROL 1
#ifdef TEST_CACHED_CONTROL
void test_cached_control();
#endif // TEST_CACHED_CONTROL



///////////////////////
namespace jc_you_suck
{
class jc_rebind
{
	template <typename REC>		static void rebind_callback(const LLSD &data, REC *reciever){ *reciever = data; }

	typedef boost::signal<void(const LLSD&)> signal_t;

public:

//#define binder_debug

	template <typename RBTYPE> static RBTYPE* rebind_llcontrol(std::string name, LLControlGroup* controlgroup, bool init)
	{
		static std::map<LLControlGroup*, std::map<std::string, void*> > references;

#ifdef binder_debug
		llinfos << "rebind_llcontrol" << llendl;
#endif

		RBTYPE* type = NULL;
		if(controlgroup)
		{
			if(references.find(controlgroup) == references.end())
			{
#ifdef binder_debug
				llinfos << "was no map for a group, adding" << llendl;
#endif
				references[controlgroup] = std::map<std::string, void*>();
			}

			if(references[controlgroup].find(name) != references[controlgroup].end())
			{
#ifdef binder_debug
				llinfos << "pulling type from map for " << name << llendl;
#endif
				type = (RBTYPE*)(references[controlgroup][name]);
				if(type == NULL)llerrs << "bad type stored" << llendl;
			}else
			{
#ifdef binder_debug
				llinfos << "creating type in map for " << name << llendl;
#endif
				type = new RBTYPE();
				references[controlgroup][name] = (void*)type;
				LLControlVariable* control = controlgroup->getControl(name);
				if(control)
				{
#ifdef binder_debug
					llinfos << "control there " << name << llendl;
#endif
					signal_t* signal = control->getSignal();
					if(signal)
					{
#ifdef binder_debug
						llinfos << "signal there" << name << llendl;
#endif
						signal->connect(boost::bind(&jc_rebind::rebind_callback<RBTYPE>, _1, type));
						if(init)jc_rebind::rebind_callback<RBTYPE>(control->getValue(),type);
					}else llerrs << "no signal!" << llendl;
				}else llerrs << "no control!" << llendl;
			}
		}
		return type;
	}
};

template <>					void jc_rebind::rebind_callback<S32>(const LLSD &data, S32 *reciever);
template <>					void jc_rebind::rebind_callback<F32>(const LLSD &data, F32 *reciever);
template <>					void jc_rebind::rebind_callback<U32>(const LLSD &data, U32 *reciever);
template <>					void jc_rebind::rebind_callback<std::string>(const LLSD &data, std::string *reciever);

}
using namespace jc_you_suck;
#define rebind_llcontrol jc_rebind::rebind_llcontrol

///////////////////////






#endif // LL_LLVIEWERCONTROL_H
