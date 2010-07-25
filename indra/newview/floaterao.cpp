/** 
 * @file llfloaterao.cpp
 * @brief clientside animation overrider
 * by Skills Hak
 */

#include "llviewerprecompiledheaders.h"

#include "floaterao.h"

#include "llagent.h"
#include "llvoavatar.h"
#include "llanimationstates.h"
#include "lluictrlfactory.h"
#include "llinventoryview.h"
#include "llstartup.h"
#include "llpreviewnotecard.h"
#include "llviewertexteditor.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llspinctrl.h"
#include "chatbar_as_cmdline.h"

#include "llinventory.h"
#include "llinventoryview.h"
#include "roles_constants.h"
#include "llviewerregion.h"

#include "llpanelinventory.h"
#include "llinventorybridge.h"

#include "jc_lslviewerbridge.h"
#include "floaterexploreanimations.h"
#include "llboost.h"
#include <boost/regex.hpp>

void cmdline_printchat(std::string message);

// TIMERS -------------------------------------------------------

AOStandTimer* mAOStandTimer;

AOStandTimer::AOStandTimer() : LLEventTimer( gSavedPerAccountSettings.getF32("EmeraldAOStandInterval") )
{
	AOStandTimer::tick();
}
AOStandTimer::~AOStandTimer()
{
}
void AOStandTimer::reset()
{
	mPeriod = gSavedPerAccountSettings.getF32("EmeraldAOStandInterval");
	mEventTimer.reset();
}
BOOL AOStandTimer::tick()
{
	LLFloaterAO::stand_iterator++;
	LLFloaterAO::ChangeStand();
	return FALSE;
}

AOInvTimer::AOInvTimer() : LLEventTimer( (F32)1.0 )
{
}
AOInvTimer::~AOInvTimer()
{
}
BOOL AOInvTimer::tick()
{
	if (!(gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled"))) return TRUE;
	if(LLStartUp::getStartupState() >= STATE_INVENTORY_SEND)
	{
		LLUUID emerald_category = JCLSLBridge::findCategoryByNameOrCreate(emerald_category_name);
		if(gInventory.isCategoryComplete(emerald_category))
		{
			return LLFloaterAO::init();
		}
		else
		{
			gInventory.fetchDescendentsOf(emerald_category);
		}
	}
	return FALSE;
}
// NC DROP -------------------------------------------------------

class AONoteCardDropTarget : public LLView
{
public:
	AONoteCardDropTarget(const std::string& name, const LLRect& rect, void (*callback)(LLViewerInventoryItem*));
	~AONoteCardDropTarget();

	void doDrop(EDragAndDropType cargo_type, void* cargo_data);

	//
	// LLView functionality
	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);
protected:
	void	(*mDownCallback)(LLViewerInventoryItem*);
};


AONoteCardDropTarget::AONoteCardDropTarget(const std::string& name, const LLRect& rect,
						  void (*callback)(LLViewerInventoryItem*)) :
	LLView(name, rect, NOT_MOUSE_OPAQUE, FOLLOWS_ALL),
	mDownCallback(callback)
{
}

AONoteCardDropTarget::~AONoteCardDropTarget()
{
}

void AONoteCardDropTarget::doDrop(EDragAndDropType cargo_type, void* cargo_data)
{
}

BOOL AONoteCardDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data,
									 EAcceptance* accept,
									 std::string& tooltip_msg)
{
	BOOL handled = FALSE;
	if(getParent())
	{
		handled = TRUE;
		LLViewerInventoryItem* inv_item = (LLViewerInventoryItem*)cargo_data;
		if(gInventory.getItem(inv_item->getUUID()))
		{
			*accept = ACCEPT_YES_COPY_SINGLE;
			if(drop)
			{
				mDownCallback(inv_item);
			}
		}
		else
		{
			*accept = ACCEPT_NO;
		}
	}
	return handled;
}

AONoteCardDropTarget * LLFloaterAO::mAOItemDropTarget;

// -------------------------------------------------------

struct struct_anim_types
{
	std::string name;
	int state;
};

std::vector<struct_anim_types> mAnimTypes;

struct struct_combobox
{
	LLComboBox* combobox;
	std::string name;
	int state;
};

std::vector<struct_combobox> mComboBoxes;

struct struct_default_anims
{
	LLUUID orig_id;
	LLUUID ao_id;
	int state;
};
std::vector<struct_default_anims> mAODefaultAnims;

struct struct_all_anims
{
	LLUUID ao_id;
	std::string anim_name;
	int state;
};
std::vector<struct_all_anims> mAOAllAnims;

struct struct_stands
{
	LLUUID ao_id;
	std::string anim_name;
};
std::vector<struct_stands> mAOStands;

struct struct_tokens
{
	std::string token;
	int state;
};
std::vector<struct_tokens> mAOTokens;

struct AOAssetInfo
{
	std::string path;
	std::string name;
};

//BOOL LLFloaterAO::fullfetch = FALSE;

int LLFloaterAO::mAnimationState = 0;
int LLFloaterAO::stand_iterator = 0;
LLComboBox* mcomboBox_stands;

LLUUID LLFloaterAO::invfolderid = LLUUID::null;
LLUUID LLFloaterAO::mCurrentStandId = LLUUID::null;

LLTabContainer* mTabContainer;
LLFloaterAO* LLFloaterAO::sInstance = NULL;

LLFloaterAO::LLFloaterAO():LLFloater(std::string("floater_ao"))
{
	llassert_always(sInstance == NULL);
	sInstance = this;
	LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_ao.xml");
	sInstance->setVisible(FALSE);

	mComboBoxes.clear();
	struct_combobox loader_combobox;
	for (std::vector<struct_anim_types>::iterator iter = mAnimTypes.begin(); iter != mAnimTypes.end(); ++iter)
	{
		loader_combobox.combobox = getChild<LLComboBox>(iter->name);
		loader_combobox.name = iter->name;
		loader_combobox.state = iter->state;
		mComboBoxes.push_back(loader_combobox);
	}

	mTabContainer = getChild<LLTabContainer>("tabcontainer");
	mcomboBox_stands = (LLComboBox*) getComboBox("stand");

	for (std::vector<struct_combobox>::iterator iter = mComboBoxes.begin(); iter != mComboBoxes.end(); ++iter)
	{
		getChild<LLComboBox>(iter->name)->setCommitCallback(onComboBoxCommit);
		childSetCommitCallback("EmeraldAORandomize"+iter->name,onCheckBoxCommit);
		childSetValue("EmeraldAORandomize"+iter->name, gSavedPerAccountSettings.getBOOL("EmeraldAORandomize"+iter->name));
	}

	loadComboBoxes();
	updateLayout(sInstance);
}

LLFloaterAO::~LLFloaterAO()
{
    sInstance=NULL;
	for (std::vector<struct_combobox>::iterator iter = mComboBoxes.begin(); iter != mComboBoxes.end(); ++iter)
	{
		//if (iter->combobox) 
		iter->combobox = 0;
	}
	delete mAOItemDropTarget;
	mAOItemDropTarget = NULL;
}

void LLFloaterAO::toggle(void*)
{
	if (sInstance)
	{
		if(!sInstance->getVisible())
		{
			sInstance->open();
		}
		else
		{
			sInstance->close();
		}
	}
	else
	{
		sInstance = new LLFloaterAO();
		sInstance->setVisible(TRUE);
	}
}

void LLFloaterAO::onOpen()
{
	sInstance->setVisible(TRUE);
	sInstance->setFocus(TRUE);
}

void LLFloaterAO::onClose()
{
	sInstance->setVisible(FALSE);
	sInstance->setFocus(FALSE);
}

BOOL LLFloaterAO::postBuild()
{
	LLView *target_view = getChild<LLView>("ao_notecard");
	if(target_view)
	{
		if (mAOItemDropTarget)
		{
			delete mAOItemDropTarget;
		}
		mAOItemDropTarget = new AONoteCardDropTarget("drop target", target_view->getRect(), AOItemDrop);//, mAvatarID);
		addChild(mAOItemDropTarget);
	}
	if(LLStartUp::getStartupState() == STATE_STARTED)
	{
		LLUUID itemidimport = (LLUUID)gSavedPerAccountSettings.getString("EmeraldAOConfigNotecardID");
		LLViewerInventoryItem* itemimport = gInventory.getItem(itemidimport);
		if(itemimport)
		{
			childSetValue("ao_nc_text","Currently set to: "+itemimport->getName());
		}
		else if(itemidimport.isNull())
		{
			childSetValue("ao_nc_text","Currently not set");
		}
		else
		{
			childSetValue("ao_nc_text","Currently set to a item not on this account");
		}
	}
	else
	{
		childSetValue("ao_nc_text","Not logged in");
	}
	childSetAction("more_btn", onClickMore, this);
	childSetAction("less_btn", onClickLess, this);

	childSetAction("reloadcard",onClickReloadCard,this);
	childSetAction("opencard",onClickOpenCard,this);
	childSetAction("prevstand",onClickPrevStand,this);
	childSetAction("nextstand",onClickNextStand,this);
	childSetCommitCallback("EmeraldAOEnabled",onClickToggleAO);
	childSetCommitCallback("EmeraldAOSitsEnabled",onClickToggleSits);
	childSetCommitCallback("EmeraldAONoStandsInMouselook",onClickNoMouselookStands);
	childSetCommitCallback("standtime",onSpinnerCommit);
	
	// Set relevant per-account stuff.
	childSetValue("EmeraldAOEnabled", gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled"));
	childSetValue("EmeraldAOSitsEnabled", gSavedPerAccountSettings.getBOOL("EmeraldAOSitsEnabled"));
	childSetValue("EmeraldAONoStandsInMouselook", gSavedPerAccountSettings.getBOOL("EmeraldAONoStandsInMouselook"));
	childSetValue("standtime", gSavedPerAccountSettings.getF32("EmeraldAOStandInterval"));

	return TRUE;
}

void LLFloaterAO::updateLayout(LLFloaterAO* floater)
{
	if (floater)
	{
		BOOL advanced = gSavedSettings.getBOOL("EmeraldAOAdvanced");
		if (advanced)
		{
			floater->reshape(610,380);
		}
		else
		{
			floater->reshape(200,380);
		}
		
		floater->childSetVisible("more_btn", !advanced);
		floater->childSetVisible("less_btn", advanced);

		floater->childSetVisible("tabcontainer", advanced);
		floater->childSetVisible("tabdefaultanims", advanced);

		for (std::vector<struct_combobox>::iterator iter = mComboBoxes.begin(); iter != mComboBoxes.end(); ++iter)
		{
			if (iter->name == "stand") continue;
			floater->childSetVisible("textdefault"+iter->name, advanced);
			floater->childSetVisible(iter->name, advanced);
		}
		mTabContainer->selectFirstTab();
	}
}

BOOL LLFloaterAO::fullfetch = FALSE;

BOOL LLFloaterAO::init()
{
	BOOL success = FALSE;

	mAnimTypes.clear();
	struct_anim_types loader_anim_types;
	loader_anim_types.name = "stand";			loader_anim_types.state = STATE_AGENT_STAND;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "walk";				loader_anim_types.state = STATE_AGENT_WALK;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "run";				loader_anim_types.state = STATE_AGENT_RUN;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "jump";				loader_anim_types.state = STATE_AGENT_JUMP;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "sit";				loader_anim_types.state = STATE_AGENT_SIT;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "gsit";				loader_anim_types.state = STATE_AGENT_GROUNDSIT;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "crouch";			loader_anim_types.state = STATE_AGENT_CROUCH;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "cwalk";			loader_anim_types.state = STATE_AGENT_CROUCHWALK;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "fall";				loader_anim_types.state = STATE_AGENT_FALLDOWN;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "hover";			loader_anim_types.state = STATE_AGENT_HOVER;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "fly";				loader_anim_types.state = STATE_AGENT_FLY;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "flyslow";			loader_anim_types.state = STATE_AGENT_FLYSLOW;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "flyup";			loader_anim_types.state = STATE_AGENT_HOVER_UP;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "flydown";			loader_anim_types.state = STATE_AGENT_HOVER_DOWN;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "land";				loader_anim_types.state = STATE_AGENT_LAND;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "standup";			loader_anim_types.state = STATE_AGENT_STANDUP;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "prejump";			loader_anim_types.state = STATE_AGENT_PRE_JUMP;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "turnleft";			loader_anim_types.state = STATE_AGENT_TURNLEFT;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "turnright";		loader_anim_types.state = STATE_AGENT_TURNRIGHT;			mAnimTypes.push_back(loader_anim_types);
	loader_anim_types.name = "typing";			loader_anim_types.state = STATE_AGENT_TYPING;			mAnimTypes.push_back(loader_anim_types);
//	loader_anim_types.name = "floating";			loader_anim_types.state = STATE_AGENT_FLOATING;			mAnimTypes.push_back(loader_anim_types);
//	loader_anim_types.name = "swimmingforward";	loader_anim_types.state = STATE_AGENT_SWIMMINGFORWARD;			mAnimTypes.push_back(loader_anim_types);
//	loader_anim_types.name = "swimmingup";		loader_anim_types.state = STATE_AGENT_SWIMMINGUP;			mAnimTypes.push_back(loader_anim_types);
//	loader_anim_types.name = "swimmingdown";		loader_anim_types.state = STATE_AGENT_SWIMMINGDOWN;			mAnimTypes.push_back(loader_anim_types);

    if (!sInstance)
	sInstance = new LLFloaterAO();

	LLUUID configncitem = (LLUUID)gSavedPerAccountSettings.getString("EmeraldAOConfigNotecardID");
	if(LLStartUp::getStartupState() >= STATE_INVENTORY_SEND)
	{
		LLUUID emerald_category = JCLSLBridge::findCategoryByNameOrCreate(emerald_category_name);
		if(gInventory.isCategoryComplete(emerald_category))
		{
			if (configncitem.notNull())
			{
				success = FALSE;
				const LLInventoryItem* item = gInventory.getItem(configncitem);
				if(item)
				{
					BOOL in_emcat = FALSE;
					LLUUID parent_id = item->getParentUUID();
					LLViewerInventoryCategory* parent = gInventory.getCategory(item->getParentUUID());
					if(parent_id != emerald_category)
					{
						while(parent)
						{
							parent_id = parent->getParentUUID();
							if(parent_id == emerald_category)
							{
								in_emcat = TRUE;
								break;
							}else if(parent_id.isNull())break;
							parent = gInventory.getCategory(parent_id);
						}
					}else in_emcat = TRUE;
					
					if(in_emcat == FALSE)
					{
						cmdline_printchat("Your AO notecard and animations must be in the #Emerald folder in order to function correctly.");
						/*LLViewerInventoryCategory* parent = gInventory.getCategory(item->getParentUUID());
						if(parent->getPreferredType() == LLAssetType::AT_NONE)
						{
							LLNotifications::instance().add("NotifyAOMove", LLSD(),LLSD());
							move_inventory_item(gAgent.getID(),gAgent.getSessionID(),parent->getUUID(),emerald_category,parent->getName(), NULL);
						}else
						{
							cmdline_printchat("Unable to move AO, as its containing folder is a category.");
						}
						*/
					}

					if (gAgent.allowOperation(PERM_COPY, item->getPermissions(),GP_OBJECT_MANIPULATE) || gAgent.isGodlike())
					{
						if(!item->getAssetUUID().isNull())
						{
							///////////////////////////
							mAOStands.clear();
							mAOTokens.clear();
							mAODefaultAnims.clear();

							struct_tokens tokenloader;
							tokenloader.token = 
							tokenloader.token = "[ Sitting On Ground ]";	tokenloader.state = STATE_AGENT_GROUNDSIT; mAOTokens.push_back(tokenloader);    // 0
							tokenloader.token = "[ Sitting ]";				tokenloader.state = STATE_AGENT_SIT; mAOTokens.push_back(tokenloader);              // 1
							tokenloader.token = "[ Crouching ]";			tokenloader.state = STATE_AGENT_CROUCH; mAOTokens.push_back(tokenloader);            // 3
							tokenloader.token = "[ Crouch Walking ]";		tokenloader.state = STATE_AGENT_CROUCHWALK; mAOTokens.push_back(tokenloader);       // 4
							tokenloader.token = "[ Standing Up ]";			tokenloader.state = STATE_AGENT_STANDUP; mAOTokens.push_back(tokenloader);          // 6
							tokenloader.token = "[ Falling ]";				tokenloader.state = STATE_AGENT_FALLDOWN; mAOTokens.push_back(tokenloader);              // 7
							tokenloader.token = "[ Flying Down ]";			tokenloader.state = STATE_AGENT_HOVER_DOWN; mAOTokens.push_back(tokenloader);          // 8
							tokenloader.token = "[ Flying Up ]";			tokenloader.state = STATE_AGENT_HOVER_UP; mAOTokens.push_back(tokenloader);            // 9
							tokenloader.token = "[ Flying Slow ]";			tokenloader.state = STATE_AGENT_FLYSLOW; mAOTokens.push_back(tokenloader);          // 10
							tokenloader.token = "[ Flying ]";				tokenloader.state = STATE_AGENT_FLY; mAOTokens.push_back(tokenloader);               // 11
							tokenloader.token = "[ Hovering ]";				tokenloader.state = STATE_AGENT_HOVER; mAOTokens.push_back(tokenloader);             // 12
							tokenloader.token = "[ Jumping ]";				tokenloader.state = STATE_AGENT_JUMP; mAOTokens.push_back(tokenloader);              // 13
							tokenloader.token = "[ Pre Jumping ]";			tokenloader.state = STATE_AGENT_PRE_JUMP; mAOTokens.push_back(tokenloader);          // 14
							tokenloader.token = "[ Running ]";				tokenloader.state = STATE_AGENT_RUN; mAOTokens.push_back(tokenloader);              // 15
							tokenloader.token = "[ Turning Right ]";		tokenloader.state = STATE_AGENT_TURNRIGHT; mAOTokens.push_back(tokenloader);        // 16
							tokenloader.token = "[ Turning Left ]";			tokenloader.state = STATE_AGENT_TURNLEFT; mAOTokens.push_back(tokenloader);         // 17
							tokenloader.token = "[ Walking ]";				tokenloader.state = STATE_AGENT_WALK; mAOTokens.push_back(tokenloader);              // 18
							tokenloader.token = "[ Landing ]";				tokenloader.state = STATE_AGENT_LAND; mAOTokens.push_back(tokenloader);              // 19
							tokenloader.token = "[ Standing ]";				tokenloader.state = STATE_AGENT_STAND; mAOTokens.push_back(tokenloader);             // 20
							tokenloader.token = "[ Swimming Down ]";		tokenloader.state = STATE_AGENT_SWIMMINGDOWN; mAOTokens.push_back(tokenloader);        // 21
							tokenloader.token = "[ Swimming Up ]";			tokenloader.state = STATE_AGENT_SWIMMINGUP; mAOTokens.push_back(tokenloader);          // 22
							tokenloader.token = "[ Swimming Forward ]";		tokenloader.state = STATE_AGENT_SWIMMINGFORWARD; mAOTokens.push_back(tokenloader);     // 23
							tokenloader.token = "[ Floating ]";				tokenloader.state = STATE_AGENT_FLOATING; mAOTokens.push_back(tokenloader);             // 24
							tokenloader.token = "[ Typing ]";				tokenloader.state = STATE_AGENT_TYPING; mAOTokens.push_back(tokenloader); //25
 
							struct_default_anims default_anims_loader;
							default_anims_loader.orig_id = ANIM_AGENT_WALK;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_WALK;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_RUN;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_RUN;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_PRE_JUMP;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_PRE_JUMP;		mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_JUMP;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_JUMP;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_TURNLEFT;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_TURNLEFT;		mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_TURNRIGHT;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_TURNRIGHT;		mAODefaultAnims.push_back(default_anims_loader);

							default_anims_loader.orig_id = ANIM_AGENT_SIT;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_SIT;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_SIT_FEMALE;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_SIT;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_SIT_GENERIC;			default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_SIT;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_SIT_GROUND;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_GROUNDSIT;		mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_SIT_GROUND_CONSTRAINED;	default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_GROUNDSIT;		mAODefaultAnims.push_back(default_anims_loader);

							default_anims_loader.orig_id = ANIM_AGENT_HOVER;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_HOVER;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_HOVER_DOWN;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_HOVER_DOWN;		mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_HOVER_UP;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_HOVER_UP;		mAODefaultAnims.push_back(default_anims_loader);

							default_anims_loader.orig_id = ANIM_AGENT_CROUCH;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_CROUCH;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_CROUCHWALK;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_CROUCHWALK;		mAODefaultAnims.push_back(default_anims_loader);

							default_anims_loader.orig_id = ANIM_AGENT_FALLDOWN;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_FALLDOWN;		mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_STANDUP;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_STANDUP;		mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_LAND;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_LAND;			mAODefaultAnims.push_back(default_anims_loader);

							default_anims_loader.orig_id = ANIM_AGENT_FLY;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_FLY;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = ANIM_AGENT_FLYSLOW;				default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_FLYSLOW;		mAODefaultAnims.push_back(default_anims_loader);

							default_anims_loader.orig_id = ANIM_AGENT_TYPE;					default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_TYPING;			mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = LLUUID("159258dc-f57c-4662-8afd-c55b81d13849");	default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_FLOATING;		mAODefaultAnims.push_back(default_anims_loader);

							default_anims_loader.orig_id = LLUUID("159258dc-f57c-4662-8afd-c55b81d13849");	default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_SWIMMINGFORWARD;		mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = LLUUID("159258dc-f57c-4662-8afd-c55b81d13849");	default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_SWIMMINGUP;		mAODefaultAnims.push_back(default_anims_loader);
							default_anims_loader.orig_id = LLUUID("159258dc-f57c-4662-8afd-c55b81d13849");	default_anims_loader.ao_id = LLUUID::null; default_anims_loader.state = STATE_AGENT_SWIMMINGDOWN;	mAODefaultAnims.push_back(default_anims_loader);
							///////////////////////////
							LLUUID* new_uuid = new LLUUID(configncitem);
							LLHost source_sim = LLHost::invalid;
							invfolderid = item->getParentUUID();
							gAssetStorage->getInvItemAsset(source_sim,
															gAgent.getID(),
															gAgent.getSessionID(),
															item->getPermissions().getOwner(),
															LLUUID::null,
															item->getUUID(),
															item->getAssetUUID(),
															item->getType(),
															&onNotecardLoadComplete,
															(void*)new_uuid,
															TRUE);
							success = TRUE;
						}
					}
				}else
				{
					//static BOOL startedfetch = FALSE;
					if(fullfetch == FALSE)
					{
						fullfetch = TRUE;
						//no choice, can't move the AO till we find it, should only have to happen once
						gInventory.startBackgroundFetch();
						return FALSE;
					}
				}
			}
		}
	}

	if (!success)
	{
		if(configncitem.notNull())
		{
			cmdline_printchat("Could not read the specified Config Notecard");
			cmdline_printchat("If your specified notecard is not in the #emerald folder, please move it there.");
		}
		gSavedPerAccountSettings.setBOOL("EmeraldAOEnabled",FALSE);
	}
	return success;
}

// AO LOGIC -------------------------------------------------------

void LLFloaterAO::run()
{
	setAnimationState(STATE_AGENT_IDLE); // reset state
	int state = getAnimationState(); // check if sitting or hovering
	if ((state == STATE_AGENT_IDLE) || (state == STATE_AGENT_STAND))
	{
		if (gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled"))
		{
			if (mAOStandTimer)
			{
				mAOStandTimer->reset();
				ChangeStand();
			}
			else
			{
				mAOStandTimer =	new AOStandTimer();
			}
		}
		else
		{
			stopAOMotion(getCurrentStandId(),TRUE); //stop stand first then set state
			setAnimationState(STATE_AGENT_IDLE);
		}
	}
	else
	{
		if (state == STATE_AGENT_SIT) gAgent.sendAnimationRequest(GetAnimIDFromState(state), (gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled") && gSavedPerAccountSettings.getBOOL("EmeraldAOSitsEnabled")) ? ANIM_REQUEST_START : ANIM_REQUEST_STOP);
		else gAgent.sendAnimationRequest(GetAnimIDFromState(state), gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled") ? ANIM_REQUEST_START : ANIM_REQUEST_STOP);
	}
}

void LLFloaterAO::startAOMotion(const LLUUID& id, BOOL stand)
{
	int startmotionstate = STATE_AGENT_IDLE;

	if (!stand) // it's a default linden anim uuid
	{
		startmotionstate = GetStateFromOrigAnimID(id);
		if (startmotionstate == STATE_AGENT_IDLE) return;
	}
	else // it's an override anim uuid, probably a stand
	{
		startmotionstate = GetStateFromAOAnimID(id);
	}
//	cmdline_printchat(llformat("stand %s // startmotionstate: %d // %s // %s",stand?"1":"0",startmotionstate,GetAnimTypeFromState(startmotionstate).c_str(),id.asString().c_str()));
	if (stand)
	{
		if (id.notNull())
		{
			BOOL sitting = FALSE;
			if (gAgent.getAvatarObject())
			{
				sitting = gAgent.getAvatarObject()->mIsSitting;
			}
			if (sitting) return;
			gAgent.sendAnimationRequest(id, ANIM_REQUEST_START);
		}
	}
	else
	{
		if (GetAnimID(id).notNull() && gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled"))
		{
			stopAOMotion(getCurrentStandId(), TRUE); //stop stand first then set state 
			setAnimationState(GetStateFromOrigAnimID(id));
		
			if ((GetStateFromOrigAnimID(id) == STATE_AGENT_SIT) && !(gSavedPerAccountSettings.getBOOL("EmeraldAOSitsEnabled"))) return;
			gAgent.sendAnimationRequest(GetAnimID(id), ANIM_REQUEST_START);
    
			if(mAODelayTimer.getElapsedTimeF32() >= F32(1.f))
			{
					    // checking for duplicate or stuck anims (packet loss/lag) 
					    std::list<LLAnimHistoryItem*> history = LLFloaterExploreAnimations::animHistory[gAgent.getID()];
					    for (std::list<LLAnimHistoryItem*>::iterator iter = history.begin(); iter != history.end(); ++iter)
					    {
						    LLAnimHistoryItem* item = (*iter);
						    int state = GetStateFromAOAnimID(item->mAssetID);
						    if (!state) continue;
						    if  ( (item->mPlaying) && (state != getAnimationState()) )
						    {
	    //						cmdline_printchat(llformat("stopping duplicate anim: %d // %s // %s",getAnimationState(),GetAnimTypeFromState(getAnimationState()).c_str(),item->mAssetID.asString().c_str()));
							    gAgent.sendAnimationRequest(item->mAssetID, ANIM_REQUEST_STOP);
						    }
					    }
			    mAODelayTimer.reset();
			}
		}
	}
}

void LLFloaterAO::stopAOMotion(const LLUUID& id, BOOL stand)
{
	int stopmotionstate = STATE_AGENT_IDLE;

	if (!stand)
	{
		stopmotionstate = GetStateFromOrigAnimID(id);
		if (stopmotionstate == STATE_AGENT_IDLE) return;
	}
	else
	{
		stopmotionstate = GetStateFromAOAnimID(id);
	}

//	cmdline_printchat(llformat("stand %s // stopmotionstate: %d // %s // %s",stand?"1":"0",stopmotionstate,GetAnimTypeFromState(stopmotionstate).c_str(),id.asString().c_str()));

	if (stand)
	{
		setAnimationState(STATE_AGENT_IDLE);
		gAgent.sendAnimationRequest(id, ANIM_REQUEST_STOP);
	}
	else
	{
		if (GetAnimID(id).notNull() && gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled"))
		{
			if (getAnimationState() == GetStateFromOrigAnimID(id))
			{
				setAnimationState(STATE_AGENT_IDLE);
			}
			if(!mAODelayTimer.getStarted())
			{
			    mAODelayTimer.start();
			}
			else
			{
			    mAODelayTimer.reset();
			}
			ChangeStand();
			gAgent.sendAnimationRequest(GetAnimID(id), ANIM_REQUEST_STOP);
		}
	}
}

void LLFloaterAO::ChangeStand()
{
	if (gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled"))
	{
		if (gAgent.getAvatarObject())
		{
			if (gSavedPerAccountSettings.getBOOL("EmeraldAONoStandsInMouselook") && gAgent.cameraMouselook()) return;

			if (gAgent.getAvatarObject()->mIsSitting)
			{
//				stopAOMotion(getCurrentStandId(), FALSE, TRUE); //stop stand first then set state
//				if (getAnimationState() != STATE_AGENT_GROUNDSIT) setAnimationState(STATE_AGENT_SIT);
//				setCurrentStandId(LLUUID::null);
				return;
			}
		}
		if ((getAnimationState() == STATE_AGENT_IDLE) || (getAnimationState() == STATE_AGENT_STAND))// stands have lowest priority
		{
			if (!(mAOStands.size() > 0)) return;
			if (gSavedPerAccountSettings.getBOOL("EmeraldAORandomizestand"))
			{
				stand_iterator = ll_rand(mAOStands.size()-1);
			}
			if (stand_iterator < 0) stand_iterator = int( mAOStands.size()-stand_iterator);
			if (stand_iterator > int( mAOStands.size()-1)) stand_iterator = 0;

			int stand_iterator_previous = stand_iterator -1;

			if (stand_iterator_previous < 0) stand_iterator_previous = int( mAOStands.size()-1);
			
			if (mAOStands[stand_iterator].ao_id.notNull())
			{
				stopAOMotion(getCurrentStandId(), TRUE); //stop stand first then set state
				startAOMotion(mAOStands[stand_iterator].ao_id, TRUE);

				setAnimationState(STATE_AGENT_STAND);
				setCurrentStandId(mAOStands[stand_iterator].ao_id);
				if ((sInstance)&&(mcomboBox_stands)) mcomboBox_stands->selectNthItem(stand_iterator);
				return;
			}
		}
	} 
	else
	{
		stopAOMotion(getCurrentStandId(), TRUE);
		return; //stop if ao is off
	}
	return;
}

int LLFloaterAO::getAnimationState()
{
	if (gAgent.getAvatarObject())
	{
		if (gAgent.getAvatarObject()->mIsSitting) setAnimationState(STATE_AGENT_SIT);
		else if (gAgent.getFlying()) setAnimationState(STATE_AGENT_HOVER);
	}
	return mAnimationState;
}

void LLFloaterAO::setAnimationState(const int state)
{
	mAnimationState = state;
}

LLUUID LLFloaterAO::getCurrentStandId()
{
	return mCurrentStandId;
}

void LLFloaterAO::setCurrentStandId(const LLUUID& id)
{
	mCurrentStandId = id;
}

void LLFloaterAO::AOItemDrop(LLViewerInventoryItem* item)
{
	gSavedPerAccountSettings.setString("EmeraldAOConfigNotecardID", item->getUUID().asString());
	sInstance->childSetValue("ao_nc_text","Currently set to: "+item->getName());
}

LLUUID LLFloaterAO::GetAnimID(const LLUUID& id)
{
	for (std::vector<struct_default_anims>::iterator iter = mAODefaultAnims.begin(); iter != mAODefaultAnims.end(); ++iter)
	{
		if (iter->orig_id == id) return iter->ao_id;
	}
	return LLUUID::null;
}

LLUUID LLFloaterAO::GetAnimIDFromState(const int state)
{
	for (std::vector<struct_default_anims>::iterator iter = mAODefaultAnims.begin(); iter != mAODefaultAnims.end(); ++iter)
	{
		if (iter->state == state) return iter->ao_id;
	}
	return LLUUID::null;
}

LLUUID LLFloaterAO::GetRandomAnimID(const LLUUID& id)
{
	return LLUUID::null;
}

LLUUID LLFloaterAO::GetRandomAnimIDFromState(const int state)
{
	std::vector<struct_all_anims> mAORandomizeAnims;
	struct_all_anims random_anims_loader;
	for (std::vector<struct_all_anims>::iterator iter = mAOAllAnims.begin(); iter != mAOAllAnims.end(); ++iter)
	{
		if (iter->state == state)
		{
			random_anims_loader.ao_id = iter->ao_id; random_anims_loader.state = iter->state; mAORandomizeAnims.push_back(random_anims_loader);
		}
	}
	return mAORandomizeAnims[ll_rand(mAOAllAnims.size()-1)].ao_id;
}

LLUUID LLFloaterAO::GetOrigAnimIDFromState(const int state)
{
	for (std::vector<struct_default_anims>::iterator iter = mAODefaultAnims.begin(); iter != mAODefaultAnims.end(); ++iter)
	{
		if (iter->state == state) return iter->orig_id;
	}
	return LLUUID::null;
}

int LLFloaterAO::GetStateFromOrigAnimID(const LLUUID& id)
{
	for (std::vector<struct_default_anims>::iterator iter = mAODefaultAnims.begin(); iter != mAODefaultAnims.end(); ++iter)
	{
		if (iter->orig_id == id) return iter->state;
	}
	return STATE_AGENT_IDLE;
}

int LLFloaterAO::GetStateFromAOAnimID(const LLUUID& id)
{
	for (std::vector<struct_all_anims>::iterator iter = mAOAllAnims.begin(); iter != mAOAllAnims.end(); ++iter)
	{
		if (iter->ao_id == id) return iter->state;
	}
	return STATE_AGENT_IDLE;
}

int LLFloaterAO::GetStateFromToken(std::string strtoken)
{
	for (std::vector<struct_tokens>::iterator iter = mAOTokens.begin(); iter != mAOTokens.end(); ++iter)
	{
		if (iter->token == strtoken) return iter->state;
	}
	return STATE_AGENT_IDLE;
}

std::string LLFloaterAO::GetAnimTypeFromState(const int state)
{
	for (std::vector<struct_anim_types>::iterator iter = mAnimTypes.begin(); iter != mAnimTypes.end(); ++iter)
	{
		if (iter->state == state) return iter->name;
	}
	return "unknown";
}

// UI -------------------------------------------------------

LLUICtrl* LLFloaterAO::getComboBox(std::string name)
{
	for (std::vector<struct_combobox>::iterator iter = mComboBoxes.begin(); iter != mComboBoxes.end(); ++iter)
	{
		if ((iter->combobox) && (iter->name == name)) return iter->combobox;
	}
	return NULL;
}

void LLFloaterAO::onSpinnerCommit(LLUICtrl* ctrl, void* userdata)
{
	LLSpinCtrl* spin = (LLSpinCtrl*) ctrl;
	if(spin)
	{
		if (spin->getName() == "standtime")
		{
			gSavedPerAccountSettings.setF32("EmeraldAOStandInterval", spin->get());
			if (mAOStandTimer) mAOStandTimer->reset();
		}
	}
}

void LLFloaterAO::onCheckBoxCommit(LLUICtrl* ctrl, void* userdata)
{
	LLSpinCtrl* checkbox = (LLSpinCtrl*) ctrl;
	if(checkbox)
	{
		gSavedPerAccountSettings.setBOOL(checkbox->getName(), checkbox->getValue().asBoolean());
	}
}

void LLFloaterAO::onClickMore(void* data)
{
	gSavedSettings.setBOOL( "EmeraldAOAdvanced", TRUE );
	updateLayout(sInstance);
}
void LLFloaterAO::onClickLess(void* data)
{
	gSavedSettings.setBOOL( "EmeraldAOAdvanced", FALSE );
	updateLayout(sInstance);
}

void LLFloaterAO::onClickToggleAO(LLUICtrl *checkbox, void*)
{
	gSavedPerAccountSettings.setBOOL("EmeraldAOEnabled", checkbox->getValue().asBoolean());
	llinfos << "Save EmeraldAOEnabled: " << checkbox->getValue().asBoolean() << llendl;
	run();
}

void LLFloaterAO::onClickToggleSits(LLUICtrl *checkbox, void*)
{
	gSavedPerAccountSettings.setBOOL("EmeraldAOSitsEnabled", checkbox->getValue().asBoolean());
	run();
}

void LLFloaterAO::onClickNoMouselookStands(LLUICtrl *checkbox, void*)
{
	gSavedPerAccountSettings.setBOOL("EmeraldAONoStandsInMouselook", checkbox->getValue().asBoolean());
}

void LLFloaterAO::onComboBoxCommit(LLUICtrl* ctrl, void* userdata)
{
	LLComboBox* box = (LLComboBox*)ctrl;
	if(box)
	{
		if (box->getName() == "stand")
		{
			stand_iterator = box->getCurrentIndex();
			cmdline_printchat(llformat("Changing stand to %s.",mAOStands[stand_iterator].anim_name.c_str()));
			ChangeStand();
		}
		else
		{
			int state = STATE_AGENT_IDLE;
			std::string stranim = box->getValue().asString();

			if (box->getName() == "sit")
			{
				if (gAgent.getAvatarObject() && (gSavedPerAccountSettings.getBOOL("EmeraldAOEnabled")) && (gSavedPerAccountSettings.getBOOL("EmeraldAOSitsEnabled")))
				{
					if ((gAgent.getAvatarObject()->mIsSitting) && (getAnimationState() == STATE_AGENT_SIT))
					{
						//llinfos << "sitting " << GetAnimID(ANIM_AGENT_SIT) << " " << getAssetIDByName(stranim) << llendl;
						gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_SIT), ANIM_REQUEST_STOP);
						gAgent.sendAnimationRequest(getAssetIDByName(stranim), ANIM_REQUEST_START);
					}
				}
				gSavedPerAccountSettings.setString("EmeraldAODefaultsit",stranim);
				state = STATE_AGENT_SIT;
			}
			else if (box->getName() == "gsit")
			{
				//llinfos << "gsitting " << GetAnimID(ANIM_AGENT_SIT_GROUND) << " " << getAssetIDByName(stranim) << llendl;
				if (gAgent.getAvatarObject())
				{
					if ((gAgent.getAvatarObject()->mIsSitting) && (getAnimationState() == STATE_AGENT_GROUNDSIT))
					{
						//llinfos << "gsitting " << GetAnimID(ANIM_AGENT_SIT_GROUND) << " " << getAssetIDByName(stranim) << llendl;
						gAgent.sendAnimationRequest(GetAnimID(ANIM_AGENT_SIT_GROUND), ANIM_REQUEST_STOP);
						gAgent.sendAnimationRequest(getAssetIDByName(stranim), ANIM_REQUEST_START);
					}
				}
				gSavedPerAccountSettings.setString("EmeraldAODefaultgsit",stranim);
				state = STATE_AGENT_GROUNDSIT;
			}
			else
			{
				for (std::vector<struct_combobox>::iterator iter = mComboBoxes.begin(); iter != mComboBoxes.end(); ++iter)
				{
					if (box->getName() == iter->name)
					{
						gAgent.sendAnimationRequest(GetOrigAnimIDFromState(iter->state), ANIM_REQUEST_STOP);
						gSavedPerAccountSettings.setString("EmeraldAODefault"+iter->name,stranim);
						state = iter->state;
					}
				}
				for (std::vector<struct_default_anims>::iterator iter = mAODefaultAnims.begin(); iter != mAODefaultAnims.end(); ++iter)
				{
					if (state == iter->state)
					{
						iter->ao_id = getAssetIDByName(stranim);
					}
				}
			}
		}
	}
}

void LLFloaterAO::onClickPrevStand(void* user_data)
{
	if (!(mAOStands.size() > 0)) return;
	stand_iterator=stand_iterator-1;
	if (stand_iterator < 0) stand_iterator = int( mAOStands.size()-stand_iterator);
	if (stand_iterator > int( mAOStands.size()-1)) stand_iterator = 0;
	cmdline_printchat(llformat("Changing stand to %s.",mAOStands[stand_iterator].anim_name.c_str()));
	ChangeStand();
}

void LLFloaterAO::onClickNextStand(void* user_data)
{
	if (!(mAOStands.size() > 0)) return;
	stand_iterator=stand_iterator+1;
	if (stand_iterator < 0) stand_iterator = int( mAOStands.size()-stand_iterator);
	if (stand_iterator > int( mAOStands.size()-1)) stand_iterator = 0;
	cmdline_printchat(llformat("Changing stand to %s.",mAOStands[stand_iterator].anim_name.c_str()));
	ChangeStand();
}

void LLFloaterAO::onClickReloadCard(void* user_data)
{
//	if(gInventory.isEverythingFetched())
//	{
		LLFloaterAO::init();
//	}
}

void LLFloaterAO::onClickOpenCard(void* user_data)
{
//	if(gInventory.isEverythingFetched())
//	{
		LLUUID configncitem = (LLUUID)gSavedPerAccountSettings.getString("EmeraldAOConfigNotecardID");
		if (configncitem.notNull())
		{
			const LLInventoryItem* item = gInventory.getItem(configncitem);
			if(item)
			{
				if (gAgent.allowOperation(PERM_COPY, item->getPermissions(),GP_OBJECT_MANIPULATE) || gAgent.isGodlike())
				{
					if(!item->getAssetUUID().isNull())
					open_notecard((LLViewerInventoryItem*)item, std::string("Note: ") + item->getName(), LLUUID::null, FALSE);
				}
			}
		}
//	}
}

void LLFloaterAO::loadComboBoxes()
{
	if (sInstance)
	{
		// empty
		for (std::vector<struct_combobox>::iterator iter = mComboBoxes.begin(); iter != mComboBoxes.end(); ++iter)
		{
			if (iter->combobox)
			{
				iter->combobox->clear();
				iter->combobox->removeall();
			}
		}

		// fill
		for (std::vector<struct_all_anims>::iterator iter = mAOAllAnims.begin(); iter != mAOAllAnims.end(); ++iter)
		{
			/*
			if ( && (mcomboBox_stands) && (iter->state == STATE_AGENT_STAND))
			{
				mcomboBox_stands->add(iter->anim_name.c_str(), ADD_BOTTOM, TRUE);
			}
			*/
			for (std::vector<struct_combobox>::iterator comboiter = mComboBoxes.begin(); comboiter != mComboBoxes.end(); ++comboiter)
			{
				if ((comboiter->combobox) && (comboiter->state == iter->state))
				{
					if (!(comboiter->combobox->selectByValue(iter->anim_name.c_str()))) //check if exists then add
					{
						comboiter->combobox->add(iter->anim_name.c_str(), ADD_BOTTOM, TRUE); 
					}
				}
			}

		}

		// reading default anim settings and updating combobox selections
		for (std::vector<struct_default_anims>::iterator iter = mAODefaultAnims.begin(); iter != mAODefaultAnims.end(); ++iter)
		{
			if (iter->state == STATE_AGENT_STAND) continue; // there is no default stand o.o
			for (std::vector<struct_combobox>::iterator comboiter = mComboBoxes.begin(); comboiter != mComboBoxes.end(); ++comboiter)
			{
				if (iter->state == comboiter->state)
				{
					std::string defaultanim = gSavedPerAccountSettings.getString("EmeraldAODefault"+comboiter->name);
					if (getAssetIDByName(defaultanim) != LLUUID::null) iter->ao_id = getAssetIDByName(defaultanim);
					if (comboiter->combobox) SetDefault(comboiter->combobox,iter->ao_id,defaultanim);
				}
			}
		}
	}
}

void LLFloaterAO::onNotecardLoadComplete(LLVFS *vfs,const LLUUID& asset_uuid,LLAssetType::EType type,void* user_data, S32 status, LLExtStat ext_status)
{
	if(status == LL_ERR_NOERR)
	{
		S32 size = vfs->getSize(asset_uuid, type);
		U8* buffer = new U8[size];
		vfs->getData(asset_uuid, type, buffer, 0, size);

		if(type == LLAssetType::AT_NOTECARD)
		{
			LLViewerTextEditor* edit = new LLViewerTextEditor("",LLRect(0,0,0,0),S32_MAX,"");
			if(edit->importBuffer((char*)buffer, (S32)size))
			{
				llinfos << "ao nc decode success" << llendl;
				std::string card = edit->getText();
				edit->die();

				mAOAllAnims.clear();
				mAOStands.clear();
				//mAODefaultAnims.clear();

				struct_stands standsloader;
				struct_all_anims all_anims_loader;

				typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
				boost::char_separator<char> sep("\n");
				tokenizer tokline(card, sep);

				for (tokenizer::iterator line = tokline.begin(); line != tokline.end(); ++line)
				{
					std::string strline(*line);

					boost::regex type("^(\\s*)(\\[ )(.*)( \\])");
					boost::smatch what; 
					if (boost::regex_search(strline, what, type)) 
					{
//						llinfos << "type: " << what[0] << llendl;
//						llinfos << "anims in type: " << boost::regex_replace(strline, type, "") << llendl;

						boost::char_separator<char> sep("|,");
						std::string stranimnames(boost::regex_replace(strline, type, ""));
						tokenizer tokanimnames(stranimnames, sep);
						for (tokenizer::iterator anim = tokanimnames.begin(); anim != tokanimnames.end(); ++anim)
						{
							std::string strtoken(what[0]);
							std::string stranim(*anim);
							int state = GetStateFromToken(strtoken.c_str());
							LLUUID animid(getAssetIDByName(stranim));

							if (!(animid.notNull()))
							{
								cmdline_printchat(llformat("Warning: animation '%s' could not be found (Section: %s).",stranim.c_str(),strtoken.c_str()));
							}
							else
							{
								all_anims_loader.ao_id = animid; all_anims_loader.anim_name = stranim.c_str(); all_anims_loader.state = state; mAOAllAnims.push_back(all_anims_loader);

								if (state == STATE_AGENT_STAND)
								{
									standsloader.ao_id = animid; standsloader.anim_name = stranim.c_str(); mAOStands.push_back(standsloader);
									continue;
								}

								for (std::vector<struct_default_anims>::iterator iter = mAODefaultAnims.begin(); iter != mAODefaultAnims.end(); ++iter)
								{
									if (state == iter->state)
									{
										iter->ao_id = animid;
									}
								}
							}
						}
					} 
				}
				llinfos << "ao nc read sucess" << llendl;

				loadComboBoxes();

				run();
			}
			else
			{
				llinfos << "ao nc decode error" << llendl;
			}
		}
	}
	else
	{
		llinfos << "ao nc read error" << llendl;
	}
}

BOOL LLFloaterAO::SetDefault(void* userdata, LLUUID ao_id, std::string defaultanim)
{
	if (sInstance && (userdata))
	{
		LLComboBox *box = (LLComboBox *) userdata;
		if (LLUUID::null == ao_id)
		{
			box->clear();
			box->removeall();
		}
		else
		{
			box->selectByValue(defaultanim);
		}
	}
	return TRUE;
}

class ObjectNameMatches : public LLInventoryCollectFunctor
{
public:
	ObjectNameMatches(std::string name)
	{
		sName = name;
	}
	virtual ~ObjectNameMatches() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		if(item)
		{
			if (item->getParentUUID() == LLFloaterAO::invfolderid)
			{
				return (item->getName() == sName);
			}
			return false;
		}
		return false;
	}
private:
	std::string sName;
};

const LLUUID& LLFloaterAO::getAssetIDByName(const std::string& name)
{
	if (name.empty()) return LLUUID::null;

	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	ObjectNameMatches objectnamematches(name);
	gInventory.collectDescendentsIf(LLUUID::null,cats,items,FALSE,objectnamematches);

	if (items.count())
	{
		return items[0]->getAssetUUID();
	}
	return LLUUID::null;
};
