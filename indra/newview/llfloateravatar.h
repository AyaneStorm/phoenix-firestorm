/**
 * @file llfloateravatar.h
 * @author Leyla Farazha
 * @brief floater for the avatar changer
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_FLOATER_AVATAR_H
#define LL_FLOATER_AVATAR_H

#include "llfloater.h"
class LLMediaCtrl;

class LLFloaterAvatar:
    public LLFloater
{
    friend class LLFloaterReg;
private:
    LLFloaterAvatar(const LLSD& key);
    ~LLFloaterAvatar();
    bool postBuild() override;

    LLMediaCtrl* mAvatarPicker;

    // <FS:Ansariel> Avatar chooser does not change between OpenSim grids
    /*virtual*/ void onOpen(const LLSD& key) override;
    void handleUrlChanged(const std::string& url);

    boost::signals2::connection mAvatarPickerUrlChangedSignal;
    // </FS:Ansariel>
};

#endif
