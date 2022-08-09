/**
 * @file vjfloaterlocalmesh.h
 * @author Vaalith Jinn
 * @brief Local Mesh Floater header
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Local Mesh contribution source code
 * Copyright (C) 2022, Vaalith Jinn.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $/LicenseInfo$
 */

#pragma once

#include "llfloater.h"
class LLObjectSelection;


class LLFloaterLocalMesh : public LLFloater
{
	public:
		LLFloaterLocalMesh(const LLSD& key);
		virtual ~LLFloaterLocalMesh(void);

	public:
		void onOpen(const LLSD& key);
		void onClose(bool app_quitting);
		void onSelectionChangedCallback();
		virtual BOOL postBuild();
		virtual void draw();

		/* add    - loads a new file, adds it to the list and reads it.
		   reload - re-loads a selected file, reapplies it to viewer objects.
		   remove - clears all affected viewer objects and unloads selected file
		   apply  - applies selected file onto a selected viewer object
		   clear  - reverts a selected viewer object to it's normal state
		   show log/show list - toggles between loaded file list, and log.
		*/
		static void onBtnAdd(void* userdata);
		void onBtnAddCallback(std::string filename);

		static void onBtnReload(void* userdata);
		static void onBtnRemove(void* userdata);
		static void onBtnApply(void* userdata);
		static void onBtnClear(void* userdata);
		static void onBtnShowLog(void* userdata);

		void reloadFileList(bool keep_selection);
		void onFileListCommitCallback();
		void reloadLowerUI();	
		void toggleSelectTool(bool toggle);
		LLUUID getCurrentSelectionIfValid();

	private:
		// llsafehandle is deprecated.
		LLPointer<LLObjectSelection> mObjectSelection;

		// since we use this to check if selection changed,
		// and since uuid seems like a safer way to check rather than
		// comparing llvovolumes, we might as well refer to this
		// when querying what is actually selected.
		LLUUID mLastSelectedObject;

};