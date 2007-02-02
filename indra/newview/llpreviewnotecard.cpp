/** 
 * @file llpreviewnotecard.cpp
 * @brief Implementation of the notecard editor
 *
 * Copyright (c) 2002-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#include "llviewerprecompiledheaders.h"

#include "llpreviewnotecard.h"

#include "llinventory.h"

#include "llagent.h"
#include "llviewerwindow.h"
#include "llbutton.h"
#include "llinventorymodel.h"
#include "lllineeditor.h"
#include "llnotify.h"
#include "llresmgr.h"
#include "roles_constants.h"
#include "llscrollbar.h"
#include "llselectmgr.h"
#include "llviewertexteditor.h"
#include "llvfile.h"
#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "lldir.h"
//#include "llfloaterchat.h"
#include "llviewerstats.h"
#include "viewer.h"		// app_abort_quit()
#include "lllineeditor.h"
#include "llvieweruictrlfactory.h"

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

const S32 PREVIEW_MIN_WIDTH =
	2 * PREVIEW_BORDER +
	2 * PREVIEW_BUTTON_WIDTH + 
	PREVIEW_PAD + RESIZE_HANDLE_WIDTH +
	PREVIEW_PAD;
const S32 PREVIEW_MIN_HEIGHT = 
	2 * PREVIEW_BORDER +
	3*(20 + PREVIEW_PAD) +
	2 * SCROLLBAR_SIZE + 128;

///----------------------------------------------------------------------------
/// Class LLPreviewNotecard
///----------------------------------------------------------------------------

// Default constructor
LLPreviewNotecard::LLPreviewNotecard(const std::string& name,
									 const LLRect& rect,
									 const std::string& title,
									 const LLUUID& item_id, 
									 const LLUUID& object_id,
									 const LLUUID& asset_id,
									 BOOL show_keep_discard) :
	LLPreview(name, rect, title, item_id, object_id, TRUE,
			  PREVIEW_MIN_WIDTH,
			  PREVIEW_MIN_HEIGHT),
	mAssetID( asset_id ),
	mNotecardItemID(item_id),
	mObjectID(object_id)
{
	LLRect curRect = rect;

	if (show_keep_discard)
	{
		gUICtrlFactory->buildFloater(this,"floater_preview_notecard_keep_discard.xml");
		childSetAction("Keep",onKeepBtn,this);
		childSetAction("Discard",onDiscardBtn,this);
	}
	else
	{
		gUICtrlFactory->buildFloater(this,"floater_preview_notecard.xml");
		childSetAction("Save",onClickSave,this);
		
		if( mAssetID.isNull() )
		{
			LLInventoryItem* item = getItem();
			if( item )
			{
				mAssetID = item->getAssetUUID();
			}
		}
	}	

	reshape(curRect.getWidth(), curRect.getHeight(), TRUE);
	setRect(curRect);
			
	childSetVisible("lock", FALSE);	
	
	LLInventoryItem* item = getItem();
	
	childSetCommitCallback("desc", LLPreview::onText, this);
	if (item)
		childSetText("desc", item->getDescription());
	childSetPrevalidate("desc", &LLLineEditor::prevalidatePrintableNotPipe);

	setTitle(title);
	
	LLViewerTextEditor* editor = LLViewerUICtrlFactory::getViewerTextEditorByName(this, "Notecard Editor");

	if (editor)
	{
		editor->setWordWrap(TRUE);
		editor->setSourceID(item_id);
		editor->setHandleEditKeysDirectly(TRUE);
	}

	gAgent.changeCameraToDefault();
}

BOOL LLPreviewNotecard::postBuild()
{
	LLViewerTextEditor *ed = (LLViewerTextEditor *)gUICtrlFactory->getTextEditorByName(this, "Notecard Editor");
	if (ed)
	{
		ed->setNotecardInfo(mNotecardItemID, mObjectID);
	}
	return TRUE;
}

bool LLPreviewNotecard::saveItem(LLPointer<LLInventoryItem>* itemptr)
{
	LLInventoryItem* item = NULL;
	if (itemptr && itemptr->notNull())
	{
		item = (LLInventoryItem*)(*itemptr);
	}
	bool res = saveIfNeeded(item);
	if (res)
	{
		delete itemptr;
	}
	return res;
}

void LLPreviewNotecard::setEnabled( BOOL enabled )
{

	LLViewerTextEditor* editor = LLViewerUICtrlFactory::getViewerTextEditorByName(this, "Notecard Editor");

	childSetEnabled("Notecard Editor", enabled);
	childSetVisible("lock", !enabled);
	childSetEnabled("desc", enabled);
	childSetEnabled("Save", enabled && editor && (!editor->isPristine()));

}


void LLPreviewNotecard::draw()
{
	

	//childSetFocus("Save", FALSE);

	LLViewerTextEditor* editor = LLViewerUICtrlFactory::getViewerTextEditorByName(this, "Notecard Editor");
	BOOL script_changed = !editor->isPristine();
	
	childSetEnabled("Save", script_changed && getEnabled());
	
	LLPreview::draw();
}

// virtual
BOOL LLPreviewNotecard::handleKeyHere(KEY key, MASK mask,
									  BOOL called_from_parent)
{
	if(getVisible() && getEnabled())
	{
		if(('S' == key) && (MASK_CONTROL == (mask & MASK_CONTROL)))
		{
			saveIfNeeded();
			return TRUE;
		}
	}
	return FALSE;
}

// virtual
BOOL LLPreviewNotecard::canClose()
{
	LLViewerTextEditor* editor = LLViewerUICtrlFactory::getViewerTextEditorByName(this, "Notecard Editor");

	if(mForceClose || editor->isPristine())
	{
		return TRUE;
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		gViewerWindow->alertXml("SaveChanges",
								  &LLPreviewNotecard::handleSaveChangesDialog,
								  this);
								  
		return FALSE;
	}
}

const LLInventoryItem* LLPreviewNotecard::getDragItem()
{
	LLViewerTextEditor* editor = LLViewerUICtrlFactory::getViewerTextEditorByName(this, "Notecard Editor");

	if(editor)
	{
		return editor->getDragItem();
	}
	return NULL;
}

void LLPreviewNotecard::loadAsset()
{
	// request the asset.
	LLInventoryItem* item = getItem();
	LLViewerTextEditor* editor = LLViewerUICtrlFactory::getViewerTextEditorByName(this, "Notecard Editor");

	if (!editor)
		return;


	if(item)
	{
		if (gAgent.allowOperation(PERM_COPY, item->getPermissions(),
									GP_OBJECT_MANIPULATE)
			|| gAgent.isGodlike())
		{
			mAssetID = item->getAssetUUID();
			if(mAssetID.isNull())
			{
				editor->setText("");
				editor->makePristine();
				editor->setEnabled(TRUE);
				mAssetStatus = PREVIEW_ASSET_LOADED;
			}
			else
			{
				LLUUID* new_uuid = new LLUUID(mItemUUID);
				LLHost source_sim = LLHost::invalid;
				if (mObjectUUID.notNull())
				{
					LLViewerObject *objectp = gObjectList.findObject(mObjectUUID);
					if (objectp && objectp->getRegion())
					{
						source_sim = objectp->getRegion()->getHost();
					}
					else
					{
						// The object that we're trying to look at disappeared, bail.
						llwarns << "Can't find object " << mObjectUUID << " associated with notecard." << llendl;
			            mAssetID.setNull();
			            editor->setText("Unable to find object containing this note.");
			            editor->makePristine();
			            editor->setEnabled(FALSE);
			            mAssetStatus = PREVIEW_ASSET_LOADED;
						return;
					}
				}
				gAssetStorage->getInvItemAsset(source_sim,
												gAgent.getID(),
												gAgent.getSessionID(),
												item->getPermissions().getOwner(),
												mObjectUUID,
												item->getUUID(),
												item->getAssetUUID(),
												item->getType(),
												&onLoadComplete,
												(void*)new_uuid,
												TRUE);
				mAssetStatus = PREVIEW_ASSET_LOADING;
			}
		}
		else
		{
			mAssetID.setNull();
			editor->setText("You are not allowed to view this note.");
			editor->makePristine();
			editor->setEnabled(FALSE);
			mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		if(!gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
								GP_OBJECT_MANIPULATE))
		{
			editor->setEnabled(FALSE);
			childSetVisible("lock", TRUE);
		}
	}
	else
	{
		editor->setText("");
		editor->makePristine();
		editor->setEnabled(TRUE);
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
}

// static
void LLPreviewNotecard::onLoadComplete(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status)
{
	llinfos << "LLPreviewNotecard::onLoadComplete()" << llendl;
	LLUUID* item_id = (LLUUID*)user_data;
	LLPreviewNotecard* preview = LLPreviewNotecard::getInstance(*item_id);
	if( preview )
	{
		if(0 == status)
		{
			LLVFile file(vfs, asset_uuid, type, LLVFile::READ);

			S32 file_length = file.getSize();

			char* buffer = new char[file_length+1];
			file.read((U8*)buffer, file_length);		/*Flawfinder: ignore*/

			// put a EOS at the end
			buffer[file_length] = 0;

			
			LLViewerTextEditor* previewEditor = LLViewerUICtrlFactory::getViewerTextEditorByName(preview, "Notecard Editor");

			if( (file_length > 19) && !strncmp( buffer, "Linden text version", 19 ) )
			{
				if( !previewEditor->importBuffer( buffer ) )
				{
					llwarns << "Problem importing notecard" << llendl;
				}
			}
			else
			{
				// Version 0 (just text, doesn't include version number)
				previewEditor->setText(buffer);
			}

			previewEditor->makePristine();

			LLInventoryItem* item = preview->getItem();
			BOOL modifiable = item && gAgent.allowOperation(PERM_MODIFY,
								item->getPermissions(), GP_OBJECT_MANIPULATE);
			previewEditor->setEnabled(modifiable);
			delete[] buffer;
			preview->mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		else
		{
			if( gViewerStats )
			{
				gViewerStats->incStat( LLViewerStats::ST_DOWNLOAD_FAILED );
			}

			if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
				LL_ERR_FILE_EMPTY == status)
			{
				LLNotifyBox::showXml("NotecardMissing");
			}
			else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
			{
				LLNotifyBox::showXml("NotecardNoPermissions");
			}
			else
			{
				LLNotifyBox::showXml("UnableToLoadNotecard");
			}

			llwarns << "Problem loading notecard: " << status << llendl;
			preview->mAssetStatus = PREVIEW_ASSET_ERROR;
		}
	}
	delete item_id;
}

// static
LLPreviewNotecard* LLPreviewNotecard::getInstance(const LLUUID& item_id)
{
	LLPreview* instance = NULL;
	preview_map_t::iterator found_it = LLPreview::sInstances.find(item_id);
	if(found_it != LLPreview::sInstances.end())
	{
		instance = found_it->second;
	}
	return (LLPreviewNotecard*)instance;
}

// static
void LLPreviewNotecard::onClickSave(void* user_data)
{
	//llinfos << "LLPreviewNotecard::onBtnSave()" << llendl;
	LLPreviewNotecard* preview = (LLPreviewNotecard*)user_data;
	if(preview)
	{
		preview->saveIfNeeded();
	}
}

struct LLSaveNotecardInfo
{
	LLPreviewNotecard* mSelf;
	LLUUID mItemUUID;
	LLUUID mObjectUUID;
	LLTransactionID mTransactionID;
	LLPointer<LLInventoryItem> mCopyItem;
	LLSaveNotecardInfo(LLPreviewNotecard* self, const LLUUID& item_id, const LLUUID& object_id,
					   const LLTransactionID& transaction_id, LLInventoryItem* copyitem) :
		mSelf(self), mItemUUID(item_id), mObjectUUID(object_id), mTransactionID(transaction_id), mCopyItem(copyitem)
	{
	}
};

bool LLPreviewNotecard::saveIfNeeded(LLInventoryItem* copyitem)
{
	if(!gAssetStorage)
	{
		llwarns << "Not connected to an asset storage system." << llendl;
		return false;
	}

	
	LLViewerTextEditor* editor = LLViewerUICtrlFactory::getViewerTextEditorByName(this, "Notecard Editor");

	if(!editor->isPristine())
	{
		// We need to update the asset information
		LLTransactionID tid;
		LLAssetID asset_id;
		tid.generate();
		asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

		LLVFile file(gVFS, asset_id, LLAssetType::AT_NOTECARD, LLVFile::APPEND);

		LLString buffer;
		if (!editor->exportBuffer(buffer))
		{
			return false;
		}

		editor->makePristine();

		S32 size = buffer.length() + 1;
		file.setMaxSize(size);
		file.write((U8*)buffer.c_str(), size);

		LLInventoryItem* item = getItem();
		// save it out to database
		if (item)
		{
			
			LLSaveNotecardInfo* info = new LLSaveNotecardInfo(this, mItemUUID, mObjectUUID,
															  tid, copyitem);
			gAssetStorage->storeAssetData(tid, LLAssetType::AT_NOTECARD,
											&onSaveComplete,
											(void*)info,
											FALSE);
		}
	}
	return true;
}

// static
void LLPreviewNotecard::onSaveComplete(const LLUUID& asset_uuid, void* user_data, S32 status) // StoreAssetData callback (fixed)
{
	LLSaveNotecardInfo* info = (LLSaveNotecardInfo*)user_data;
	if(info && (0 == status))
	{
		if(info->mObjectUUID.isNull())
		{
			LLViewerInventoryItem* item;
			item = (LLViewerInventoryItem*)gInventory.getItem(info->mItemUUID);
			if(item)
			{
				LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
				new_item->setAssetUUID(asset_uuid);
				new_item->setTransactionID(info->mTransactionID);
				new_item->updateServer(FALSE);
				gInventory.updateItem(new_item);
				gInventory.notifyObservers();
			}
			else
			{
				llwarns << "Inventory item for script " << info->mItemUUID
						<< " is no longer in agent inventory." << llendl;
			}
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(info->mObjectUUID);
			LLViewerInventoryItem* item = NULL;
			if(object)
			{
				item = (LLViewerInventoryItem*)object->getInventoryObject(info->mItemUUID);
			}
			if(object && item)
			{
				item->setAssetUUID(asset_uuid);
				item->setTransactionID(info->mTransactionID);
				object->updateInventory(item, TASK_INVENTORY_ITEM_KEY, false);
				dialog_refresh_all();
			}
			else
			{
				gViewerWindow->alertXml("SaveNotecardFailObjectNotFound");
			}
		}
		// Perform item copy to inventory
		if (info->mCopyItem.notNull())
		{
			LLViewerTextEditor* editor = LLViewerUICtrlFactory::getViewerTextEditorByName(info->mSelf, "Notecard Editor");
			if (editor)
			{
				editor->copyInventory(info->mCopyItem);
			}
		}
		
		// Find our window and close it if requested.
		LLPreviewNotecard* previewp = (LLPreviewNotecard*)LLPreview::find(info->mItemUUID);
		if (previewp && previewp->mCloseAfterSave)
		{
			previewp->close();
		}
	}
	else
	{
		llwarns << "Problem saving notecard: " << status << llendl;
		LLStringBase<char>::format_map_t args;
		args["[REASON]"] = std::string(LLAssetStorage::getErrorString(status));
		gViewerWindow->alertXml("SaveNotecardFailReason",args);
	}

	char uuid_string[UUID_STR_LENGTH];		/*Flawfinder: ignore*/
	asset_uuid.toString(uuid_string);
	char filename[LL_MAX_PATH];		/*Flawfinder: ignore*/
	snprintf(filename, LL_MAX_PATH, "%s.tmp", gDirUtilp->getExpandedFilename(LL_PATH_CACHE,uuid_string).c_str());		/*Flawfinder: ignore*/
	LLFile::remove(filename);
	delete info;
}

// static
void LLPreviewNotecard::handleSaveChangesDialog(S32 option, void* userdata)
{
	LLPreviewNotecard* self = (LLPreviewNotecard*)userdata;
	switch(option)
	{
	case 0:  // "Yes"
		self->mCloseAfterSave = TRUE;
		LLPreviewNotecard::onClickSave((void*)self);
		break;

	case 1:  // "No"
		self->mForceClose = TRUE;
		self->close();
		break;

	case 2: // "Cancel"
	default:
		// If we were quitting, we didn't really mean it.
		app_abort_quit();
		break;
	}
}

void LLPreviewNotecard::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPreview::reshape( width, height, called_from_parent );

	if( !isMinimized() )
	{
		// So that next time you open a script it will have the same height and width 
		// (although not the same position).
		gSavedSettings.setRect("NotecardEditorRect", mRect);
	}
}

// EOF
