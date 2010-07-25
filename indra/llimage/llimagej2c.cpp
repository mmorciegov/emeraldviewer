/** 
 * @file llimagej2c.cpp
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
#include "linden_common.h"

#include "apr_pools.h"
#include "apr_dso.h"

#include "lldir.h"
#include "llimagej2c.h"
#include "llmemtype.h"
#include "emkdu.h"

// We need this to get an OS X version number
#ifdef LL_DARWIN
#include <Carbon/Carbon.h>
#endif

typedef LLImageJ2CImpl* (*CreateLLImageJ2CFunction)();
typedef void (*DestroyLLImageJ2CFunction)(LLImageJ2CImpl*);
typedef const char* (*EngineInfoLLImageJ2CFunction)();

//The Emerald variants of the above;
typedef EMKDUImpl* (*EmCreateImageFunction)();
typedef void (*EmDestroyImageFunction)(EMKDUImpl*);
typedef const char* (*EmEngineInfoFunction)();

//some "private static" variables so we only attempt to load
//dynamic libaries once
CreateLLImageJ2CFunction j2cimpl_create_func;
DestroyLLImageJ2CFunction j2cimpl_destroy_func;
EngineInfoLLImageJ2CFunction j2cimpl_engineinfo_func;

EmCreateImageFunction emimpl_create_func;
EmDestroyImageFunction emimpl_destroy_func;
EmEngineInfoFunction emimpl_engineinfo_func;

apr_pool_t *j2cimpl_dso_memory_pool;
apr_dso_handle_t *j2cimpl_dso_handle;

//Declare the prototype for theses functions here, their functionality
//will be implemented in other files which define a derived LLImageJ2CImpl
//but only ONE static library which has the implementation for this
//function should ever be included
LLImageJ2CImpl* fallbackCreateLLImageJ2CImpl();
void fallbackDestroyLLImageJ2CImpl(LLImageJ2CImpl* impl);
const char* fallbackEngineInfoLLImageJ2CImpl();

bool LLImageJ2C::useEMKDU = false;

//static
//Loads the required "create", "destroy" and "engineinfo" functions needed
void LLImageJ2C::openDSO()
{
	// Loading emkdu on Tiger on Intel machines crashes, so don't do that.
	// So does loading it on PPC (despite being supposedly universal).
#ifdef LL_DARWIN
#ifdef __ppc__
	return;
#else
	SInt32 osx_version;
	if(Gestalt(gestaltSystemVersion, &osx_version) != noErr)
		return;
	if(osx_version < 0x1050)
		return;
#endif // __ppc__
#endif // LL_DARWIN
	//attempt to load a DSO and get some functions from it
	std::string dso_name;
	std::string emdso_name;
	std::string dso_path;

	bool all_functions_loaded = false;
	apr_status_t rv;

#if LL_WINDOWS
	emdso_name = "emkdu.dll";
	dso_name = "llkdu.dll";
#elif LL_DARWIN
	emdso_name = "libemkdu.dylib";
	dso_name = "libllkdu.dylib";
#else
	emdso_name = "libemkdu.so";
	dso_name = "libllkdu.so";
#endif

	dso_path = gDirUtilp->findFile(emdso_name,
				       gDirUtilp->getAppRODataDir(),
				       gDirUtilp->getExecutableDir());

	j2cimpl_dso_handle      = NULL;
	j2cimpl_dso_memory_pool = NULL;

	apr_pool_create(&j2cimpl_dso_memory_pool, NULL);
	rv = apr_dso_load(&j2cimpl_dso_handle,
					  dso_path.c_str(),
					  j2cimpl_dso_memory_pool);

	//now, check for success
	if ( rv == APR_SUCCESS )
	{
		EmCreateImageFunction create_func = NULL;
		EmDestroyImageFunction dest_func = NULL;
		EmEngineInfoFunction engineinfo_func = NULL;

		rv = apr_dso_sym((apr_dso_handle_sym_t*)&create_func,
						 j2cimpl_dso_handle,
						 "EmCreateImage");
		if ( rv == APR_SUCCESS )
		{
			//we've loaded the create function ok
			//we need to delete via the DSO too
			//so lets check for a destruction function
			rv = apr_dso_sym((apr_dso_handle_sym_t*)&dest_func,
							 j2cimpl_dso_handle,
						       "EmDestroyImage");
			if ( rv == APR_SUCCESS )
			{
				//we've loaded the destroy function ok
				rv = apr_dso_sym((apr_dso_handle_sym_t*)&engineinfo_func,
						 j2cimpl_dso_handle,
						 "EmEngineInfo");
				if ( rv == APR_SUCCESS )
				{
					//ok, everything is loaded alright
					emimpl_create_func = create_func;
					emimpl_destroy_func = dest_func;
					emimpl_engineinfo_func = engineinfo_func;
					all_functions_loaded = true;
					useEMKDU = true;
				}
			}
		}
	}

	if(!all_functions_loaded)
	{
		dso_path = gDirUtilp->findFile(dso_name,
				       gDirUtilp->getAppRODataDir(),
				       gDirUtilp->getExecutableDir());
		rv = apr_dso_load(&j2cimpl_dso_handle,
						  dso_path.c_str(),
						  j2cimpl_dso_memory_pool);

		//now, check for success
		if ( rv == APR_SUCCESS )
		{
		//found the dynamic library
		//now we want to load the functions we're interested in
		CreateLLImageJ2CFunction  create_func = NULL;
		DestroyLLImageJ2CFunction dest_func = NULL;
		EngineInfoLLImageJ2CFunction engineinfo_func = NULL;

		rv = apr_dso_sym((apr_dso_handle_sym_t*)&create_func,
						 j2cimpl_dso_handle,
						 "createLLImageJ2CKDU");
		if ( rv == APR_SUCCESS )
		{
			//we've loaded the create function ok
			//we need to delete via the DSO too
			//so lets check for a destruction function
			rv = apr_dso_sym((apr_dso_handle_sym_t*)&dest_func,
							 j2cimpl_dso_handle,
						       "destroyLLImageJ2CKDU");
			if ( rv == APR_SUCCESS )
			{
				//we've loaded the destroy function ok
				rv = apr_dso_sym((apr_dso_handle_sym_t*)&engineinfo_func,
						 j2cimpl_dso_handle,
						 "engineInfoLLImageJ2CKDU");
				if ( rv == APR_SUCCESS )
				{
					//ok, everything is loaded alright
					j2cimpl_create_func  = create_func;
					j2cimpl_destroy_func = dest_func;
					j2cimpl_engineinfo_func = engineinfo_func;
					all_functions_loaded = true;
				}
			}
		}
	}
	}
	if ( !all_functions_loaded )
	{
		//something went wrong with the DSO or function loading..
		//fall back onto our safety impl creation function

#if 0
		// precious verbose debugging, sadly we can't use our
		// 'llinfos' stream etc. this early in the initialisation seq.
		char errbuf[256];
		fprintf(stderr, "failed to load syms from DSO %s (%s)\n",
			dso_name.c_str(), dso_path.c_str());
		apr_strerror(rv, errbuf, sizeof(errbuf));
		fprintf(stderr, "error: %d, %s\n", rv, errbuf);
		apr_dso_error(j2cimpl_dso_handle, errbuf, sizeof(errbuf));
		fprintf(stderr, "dso-error: %d, %s\n", rv, errbuf);
#endif

		if ( j2cimpl_dso_handle )
		{
			apr_dso_unload(j2cimpl_dso_handle);
			j2cimpl_dso_handle = NULL;
		}

		if ( j2cimpl_dso_memory_pool )
		{
			apr_pool_destroy(j2cimpl_dso_memory_pool);
			j2cimpl_dso_memory_pool = NULL;
		}
	}
}

//static
void LLImageJ2C::closeDSO()
{
	if ( j2cimpl_dso_handle ) apr_dso_unload(j2cimpl_dso_handle);
	if (j2cimpl_dso_memory_pool) apr_pool_destroy(j2cimpl_dso_memory_pool);
}

//static
std::string LLImageJ2C::getEngineInfo()
{
	if(useEMKDU)
		return emimpl_engineinfo_func();

	if (!j2cimpl_engineinfo_func)
		j2cimpl_engineinfo_func = fallbackEngineInfoLLImageJ2CImpl;

	return j2cimpl_engineinfo_func();
}

LLImageJ2C::LLImageJ2C() : 	LLImageFormatted(IMG_CODEC_J2C),
							mMaxBytes(0),
							mRawDiscardLevel(-1),
							mRate(0.0f),
							mReversible(FALSE)
	
{
	if(useEMKDU)
	{
		emImpl = emimpl_create_func();
		return;
	}
	
	//We assume here that if we wanted to create via
	//a dynamic library that the approriate open calls were made
	//before any calls to this constructor.

	//Therefore, a NULL creation function pointer here means
	//we either did not want to create using functions from the dynamic
	//library or there were issues loading it, either way
	//use our fall back
	if ( !j2cimpl_create_func )
	{
		j2cimpl_create_func = fallbackCreateLLImageJ2CImpl;
	}

	mImpl = j2cimpl_create_func();
}

// virtual
LLImageJ2C::~LLImageJ2C()
{

	if(useEMKDU)
	{
		if(emImpl)
		{
			emimpl_destroy_func(emImpl);
			return;
		}
		return;
	}
	//We assume here that if we wanted to destroy via
	//a dynamic library that the approriate open calls were made
	//before any calls to this destructor.

	//Therefore, a NULL creation function pointer here means
	//we either did not want to destroy using functions from the dynamic
	//library or there were issues loading it, either way
	//use our fall back
	if ( !j2cimpl_destroy_func )
	{
		j2cimpl_destroy_func = fallbackDestroyLLImageJ2CImpl;
	}

	if ( mImpl )
	{
		j2cimpl_destroy_func(mImpl);
	}
}

// virtual
void LLImageJ2C::resetLastError()
{
	mLastError.clear();
}

//virtual
void LLImageJ2C::setLastError(const std::string& message, const std::string& filename)
{
	mLastError = message;
	if (!filename.empty())
		mLastError += std::string(" FILE: ") + filename;
}

// virtual
S8  LLImageJ2C::getRawDiscardLevel()
{
	return mRawDiscardLevel;
}

BOOL LLImageJ2C::updateData()
{
	BOOL res = TRUE;
	resetLastError();

	// Check to make sure that this instance has been initialized with data
	if (!getData() || (getDataSize() < 16))
	{
		setLastError("LLImageJ2C uninitialized");
		res = FALSE;
	}
	else 
	{
		if(useEMKDU)
		{
			updateRawDiscardLevel();
			EMImageData img_data;
			img_data.mData = getData();
			img_data.mLength = (U32)getDataSize();
			EMImageDims dims = emImpl->getMetadata(&img_data);
			if(dims.mHeight == -1 || dims.mWidth == -1 || dims.mComponents == -1)
			{
				res = 0;
			}
			else
			{
				setSize(dims.mWidth, dims.mHeight, dims.mComponents);
			}
		}
		else
		{
		res = mImpl->getMetadata(*this);
	}
	}

	if (res)
	{
		// SJB: override discard based on mMaxBytes elsewhere
		S32 max_bytes = getDataSize(); // mMaxBytes ? mMaxBytes : getDataSize();
		S32 discard = calcDiscardLevelBytes(max_bytes);
		setDiscardLevel(discard);
	}

	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}
	return res;
}


BOOL LLImageJ2C::decode(LLImageRaw *raw_imagep, F32 decode_time)
{
	return decodeChannels(raw_imagep, decode_time, 0, 4);
}


// Returns TRUE to mean done, whether successful or not.
BOOL LLImageJ2C::decodeChannels(LLImageRaw *raw_imagep, F32 decode_time, S32 first_channel, S32 max_channel_count )
{
	LLMemType mt1((LLMemType::EMemType)mMemType);

	BOOL res = TRUE;
	
	resetLastError();

	// Check to make sure that this instance has been initialized with data
	if (!getData() || (getDataSize() < 16))
	{
		setLastError("LLImageJ2C uninitialized");
		res = TRUE; // done
	}
	else
	{
		// Update the raw discard level
		updateRawDiscardLevel();
		mDecoding = TRUE;
		if(useEMKDU)
		{
			EMImageData img_data;
			img_data.mComponents = max_channel_count;
			img_data.mFirstComp = first_channel;
			img_data.mData = getData();
			img_data.mLength = (U32)getDataSize();
			img_data.mDiscard = getRawDiscardLevel();
			img_data.mHeight = getHeight();
			img_data.mWidth = getWidth();
			if(emImpl->getMetaComment(&img_data))
			{
				
				char wat [67];
				memcpy(&wat[0],img_data.mData,img_data.mLength);
				raw_imagep->decodedImageComment = wat;
				img_data.mComponents = max_channel_count;
				img_data.mFirstComp = first_channel;
				img_data.mData = getData();
				img_data.mLength = (U32)getDataSize();
				img_data.mDiscard = getRawDiscardLevel();
				img_data.mHeight = getHeight();
				img_data.mWidth = getWidth();
			}
			res = emImpl->decodeData(&img_data);
			if((img_data.mData != getData()) &&
				(img_data.mWidth > 0) && (img_data.mHeight > 0)
				&& (img_data.mComponents > 0))
			{
				raw_imagep->resize(img_data.mWidth, img_data.mHeight, img_data.mComponents);
				memcpy(raw_imagep->getData(), img_data.mData, img_data.mLength);
				img_data.mData = 0;
			}
			else
			{
				mDecoding = false;
			}
		}
		else
		{
		res = mImpl->decodeImpl(*this, *raw_imagep, decode_time, first_channel, max_channel_count);
	}
	}
	
	if (res)
	{
		if (!mDecoding)
		{
			// Failed
			raw_imagep->deleteData();
		}
		else
		{
			mDecoding = FALSE;
		}
	}

	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}
	
	return res;
}


BOOL LLImageJ2C::encode(const LLImageRaw *raw_imagep, F32 encode_time)
{
	return encode(raw_imagep, NULL, encode_time);
}


BOOL LLImageJ2C::encode(const LLImageRaw *raw_imagep, const char* comment_text, F32 encode_time)
{
	LLMemType mt1((LLMemType::EMemType)mMemType);
	resetLastError();
	BOOL res = FALSE;
	if(useEMKDU)
	{
		EMImageData img_data;
		img_data.mComponents = raw_imagep->getComponents();
		img_data.mData = (U8*)raw_imagep->getData();
		img_data.mLength = raw_imagep->getDataSize();
		img_data.mDiscard = (mReversible == 0);
		img_data.mFirstComp = 0;
		img_data.mHeight = raw_imagep->getHeight();
		img_data.mWidth = raw_imagep->getWidth();
		res = emImpl->encodeData(&img_data);
		if(img_data.mHeight == -1 || img_data.mWidth == -1)
		{
			setLastError("Failed to encode image using EMKDU.");
		}
		else
		{
			copyData(img_data.mData, (S32)img_data.mLength);
			updateData();
			img_data.mData = 0;
		}

	}
	else
	{
		res = mImpl->encodeImpl(*this, *raw_imagep, comment_text, encode_time, mReversible);
	}

	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}
	return res;
}

//static
S32 LLImageJ2C::calcHeaderSizeJ2C()
{
	//Zwag: This change appears to fix a lot of problems with llkdu and emkdu for image decoding.
	//Zwag: rolled back for now. Caused problems with some smaller textures :S
	return FIRST_PACKET_SIZE; //Zwag:600 Hack. just needs to be >= actual header size...
}

//static
S32 LLImageJ2C::calcDataSizeJ2C(S32 w, S32 h, S32 comp, S32 discard_level, F32 rate)
{
	if (rate <= 0.f) rate = .125f;
	while (discard_level > 0)
	{
		if (w < 1 || h < 1)
			break;
		w >>= 1;
		h >>= 1;
		discard_level--;
	}
	S32 bytes = (S32)((F32)(w*h*comp)*rate);
	bytes = llmax(bytes, calcHeaderSizeJ2C());
	return bytes;
}

S32 LLImageJ2C::calcHeaderSize()
{
	return calcHeaderSizeJ2C();
}

S32 LLImageJ2C::calcDataSize(S32 discard_level)
{
	return calcDataSizeJ2C(getWidth(), getHeight(), getComponents(), discard_level, mRate);
}

S32 LLImageJ2C::calcDiscardLevelBytes(S32 bytes)
{
	llassert(bytes >= 0);
	S32 discard_level = 0;
	if (bytes == 0)
	{
		return MAX_DISCARD_LEVEL;
	}
	while (1)
	{
		S32 bytes_needed = calcDataSize(discard_level); // virtual
		if (bytes >= bytes_needed - (bytes_needed>>2)) // For J2c, up the res at 75% of the optimal number of bytes
		{
			break;
		}
		discard_level++;
		if (discard_level >= MAX_DISCARD_LEVEL)
		{
			break;
		}
	}
	return discard_level;
}

void LLImageJ2C::setRate(F32 rate)
{
	mRate = rate;
}

void LLImageJ2C::setMaxBytes(S32 max_bytes)
{
	mMaxBytes = max_bytes;
}

void LLImageJ2C::setReversible(const BOOL reversible)
{
 	mReversible = reversible;
}


BOOL LLImageJ2C::loadAndValidate(const std::string &filename)
{
	BOOL res = TRUE;
	
	resetLastError();

	S32 file_size = 0;
	LLAPRFile infile ;
	infile.open(filename, LL_APR_RB, LLAPRFile::global, &file_size);
	apr_file_t* apr_file = infile.getFileHandle() ;
	if (!apr_file)
	{
		setLastError("Unable to open file for reading", filename);
		res = FALSE;
	}
	else if (file_size == 0)
	{
		setLastError("File is empty",filename);
		res = FALSE;
	}
	else
	{
		U8 *data = new U8[file_size];
		apr_size_t bytes_read = file_size;
		apr_status_t s = apr_file_read(apr_file, data, &bytes_read); // modifies bytes_read	
		infile.close() ;

		if (s != APR_SUCCESS || (S32)bytes_read != file_size)
		{
			delete[] data;
			setLastError("Unable to read entire file");
			res = FALSE;
		}
		else
		{
			res = validate(data, file_size);
		}
	}
	
	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}
	
	return res;
}


BOOL LLImageJ2C::validate(U8 *data, U32 file_size)
{
	LLMemType mt1((LLMemType::EMemType)mMemType);

	resetLastError();
	
	setData(data, file_size);

	BOOL res = updateData();
	if ( res )
	{
		// Check to make sure that this instance has been initialized with data
		if (!getData() || (0 == getDataSize()))
		{
			setLastError("LLImageJ2C uninitialized");
			res = FALSE;
		}
		else
		{
			if(useEMKDU)
			{
				EMImageData img_data;
				img_data.mData = getData();
				img_data.mLength = (U32)getDataSize();
				EMImageDims dims = emImpl->getMetadata(&img_data);
				if(dims.mHeight == -1 || dims.mWidth == -1 || dims.mComponents == -1)
				{
					res = 0;
				}
				else
				{
					res = 1;
				}
			}
			else
			{
			res = mImpl->getMetadata(*this);
			}
		}
	}
	
	if (!mLastError.empty())
	{
		LLImage::setLastError(mLastError);
	}
	return res;
}

void LLImageJ2C::decodeFailed()
{
	mDecoding = FALSE;
}

void LLImageJ2C::updateRawDiscardLevel()
{
	mRawDiscardLevel = mMaxBytes ? calcDiscardLevelBytes(mMaxBytes) : mDiscardLevel;
}

LLImageJ2CImpl::~LLImageJ2CImpl()
{
}
