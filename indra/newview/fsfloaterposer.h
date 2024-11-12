/**
 * @file fsfloaterposer.cpp
 * @brief View Model for posing your (and other) avatar(s).
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2024 Angeldark Raymaker @ Second Life
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


#ifndef FS_FLOATER_POSER_H
#define FS_FLOATER_POSER_H

#include "llfloater.h"
#include "fsposeranimator.h"

class FSVirtualTrackpad;
class LLButton;
class LLCheckBoxCtrl;
class LLLineEditor;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLTabContainer;

/// <summary>
/// Describes how to load a pose file.
/// </summary>
typedef enum E_LoadPoseMethods
{
    ROTATIONS               = 1,
    POSITIONS               = 2,
    SCALES                  = 3,
    ROTATIONS_AND_POSITIONS = 4,
    ROTATIONS_AND_SCALES    = 5,
    POSITIONS_AND_SCALES    = 6,
    ROT_POS_AND_SCALES      = 7,
    HAND_RIGHT              = 8,
    HAND_LEFT               = 9,
    FACE_ONLY               = 10,
} E_LoadPoseMethods;

/// <summary>
/// Describes the columns of the avatars scroll-list.
/// </summary>
typedef enum E_Columns
{
    COL_ICON = 0,
    COL_NAME = 1,
    COL_UUID = 2,
    COL_SAVE = 3,
} E_Columns;

/// <summary>
/// A class containing the UI fiddling for the Poser Floater.
/// Please don't do LLJoint stuff here, fsposingmotion (the LLMotion derivative) is the class for that.
/// </summary>
class FSFloaterPoser : public LLFloater
{
    friend class LLFloaterReg;
    FSFloaterPoser(const LLSD &key);

 private:
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;

    /// <summary>
    /// Refreshes the supplied pose list from the supplued subdirectory.
    /// </summary>
    void refreshPoseScroll(LLScrollListCtrl* posesScrollList, std::optional<std::string_view> subDirectory = std::nullopt);

    /// <summary>
    /// (Dis)Enables all of the posing controls; such as when you can't pose for reasons.
    /// </summary>
    /// <param name="enable">Whether to enable the pose controls.</param>
    void poseControlsEnable(bool enable);

    /// <summary>
    /// Refreshes all of the 'joint/bones/thingos' lists (lists-zah, all of them).
    /// </summary>
    void refreshJointScrollListMembers();

    /// <summary>
    /// Adds a 'header' menu item to the supplied scroll list, handy for demarking clusters of joints on the UI.
    /// </summary>
    /// <param name="jointName">
    /// The well-known name of the joint, eg: mChest
    /// This does a lookup into the poser XML for a friendly header title by joint name, if it exists.
    /// </param>
    /// <param name="bodyJointsScrollList">The scroll list to add the header-row to.</param>
    void addHeaderRowToScrollList(const std::string& jointName, LLScrollListCtrl* bodyJointsScrollList);

    /// <summary>
    /// Generates the data for a row to add to a scroll-list.
    /// The supplied joint name is looked up in the UI XML to find a friendly name.
    /// </summary>
    /// <param name="jointName">The well-known joint name of the joint to add the row for, eg: mChest.</param>
    /// <param name="isHeaderRow">Whether the joint is one which should come immediately after a header.</param>
    /// <returns>The data required to make the row.</returns>
    LLSD createRowForJoint(const std::string& jointName, bool isHeaderRow);

    /// <summary>
    /// Gets the collection of poser joints currently selected on the active bones-tab of the UI.
    /// </summary>
    /// <returns>The selected joints</returns>
    std::vector<FSPoserAnimator::FSPoserJoint*> getUiSelectedPoserJoints() const;

    /// <summary>
    /// Gets a detectable avatar by its UUID.
    /// </summary>
    /// <param name="avatarToFind">The ID of the avatar to find.</param>
    /// <returns>The avatar, if found, otherwise nullptr.</returns>
    LLVOAvatar* getAvatarByUuid(const LLUUID& avatarToFind) const;

    /// <summary>
    /// Gets the currently selected avatar or animesh.
    /// </summary>
    /// <returns>The currently selected avatar or animesh.</returns>
    LLVOAvatar* getUiSelectedAvatar() const;

    /// <summary>
    /// Sets the UI selection for avatar or animesh.
    /// </summary>
    /// <param name="avatarToSelect">The ID of the avatar to select, if found.</param>
    void setUiSelectedAvatar(const LLUUID& avatarToSelect);

    /// <summary>
    /// Gets the current bone-deflection style: encapsulates 'anything else you want to do' while you're manipulating a joint.
    /// Such as: fiddle the opposite joint too.
    /// </summary>
    /// <returns>A E_BoneDeflectionStyles member.</returns>
    E_BoneDeflectionStyles getUiSelectedBoneDeflectionStyle() const;

    /// <summary>
    /// Gets the collection of UUIDs for nearby avatars.
    /// </summary>
    /// <returns>A the collection of UUIDs for nearby avatars.</returns>
    uuid_vec_t getNearbyAvatarsAndAnimeshes() const;

    /// <summary>
    /// Gets a collection of UUIDs for avatars currently being presented on the UI.
    /// </summary>
    /// <returns>A the collection of UUIDs.</returns>
    uuid_vec_t getCurrentlyListedAvatarsAndAnimeshes() const;

    /// <summary>
    /// Gets the scroll-list index of the supplied avatar.
    /// </summary>
    /// <param name="toFind">The avatar UUID to find on the avatars scroll list.</param>
    /// <returns>The scroll-list index for the supplied avatar, if found, otherwise -1.</returns>
    S32 getAvatarListIndexForUuid(const LLUUID& toFind) const;

    /// <summary>
    /// There are several control-callbacks manipulating rotations etc, they all devolve to these.
    /// In these are the appeals to the posing business layer.
    /// </summary>
    /// <remarks>
    /// Using a set, then a get does not guarantee the value you just set.
    /// There may be +/- PI difference two axes, because harmonics.
    /// Thus keep your UI synced with less gets.
    /// </remarks>
    void setSelectedJointsRotation(F32 yawInRadians, F32 pitchInRadians, F32 rollInRadians);
    void setSelectedJointsPosition(F32 x, F32 y, F32 z);
    void setSelectedJointsScale(F32 x, F32 y, F32 z);

    /// <summary>
    /// Yeilds the rotation of the first selected joint (one may multi-select).
    /// </summary>
    /// <remarks>
    /// Using a set, then a get does not guarantee the value you just set.
    /// There may be +/- PI difference two axes, because harmonics.
    /// Thus keep your UI synced with less gets.
    /// </remarks>
    LLVector3 getRotationOfFirstSelectedJoint() const;
    LLVector3 getPositionOfFirstSelectedJoint() const;
    LLVector3 getScaleOfFirstSelectedJoint() const;

    // Pose load/save
    void onToggleLoadSavePanel();
    void onClickPoseSave();
    void onPoseFileSelect();
    bool savePoseToXml(LLVOAvatar* avatar, const std::string& posePath);
    void onClickBrowsePoseCache();
    void onPoseMenuAction(const LLSD& param);
    void loadPoseFromXml(LLVOAvatar* avatar, const std::string& poseFileName, E_LoadPoseMethods loadMethod);
    bool poseFileStartsFromTeePose(const std::string& poseFileName);
    void setPoseSaveFileTextBoxToUiSelectedAvatarSaveFileName();
    void setUiSelectedAvatarSaveFileName(const std::string& saveFileName);

    // UI Event Handlers:
    void onAvatarsRefresh();
    void onAvatarSelect();
    void onJointTabSelect();
    void onToggleAdvancedPanel();
    void onToggleMirrorChange();
    void onToggleSympatheticChange();
    void onToggleDeltaModeChange();
    void setRotationChangeButtons(bool mirror, bool sympathetic, bool togglingDelta);
    void onUndoLastRotation();
    void onRedoLastRotation();
    void onUndoLastPosition();
    void onRedoLastPosition();
    void onUndoLastScale();
    void onRedoLastScale();
    void onResetPosition();
    void onResetScale();
    void onSetAvatarToTpose();
    void enableOrDisableRedoButton();
    void onPoseStartStop();
    void startPosingSelf();
    void stopPosingSelf();
    void onLimbTrackballChanged();
    void onLimbYawPitchRollChanged();
    void onAvatarPositionSet();
    void onAdvancedPositionSet();
    void onAdvancedScaleSet();
    void onClickToggleSelectedBoneEnabled();
    void onClickRecaptureSelectedBones();
    void onClickFlipPose();
    void onClickFlipSelectedJoints();
    void onPoseJointsReset();
    void onOpenSetAdvancedPanel();
    void onAdjustTrackpadSensitivity();
    void onClickLoadLeftHandPose();
    void onClickLoadRightHandPose();
    void onClickLoadHandPose(bool isRightHand);

    // UI Refreshments
    void refreshRotationSliders();
    void refreshAvatarPositionSliders();
    void refreshTrackpadCursor();
    void refreshAdvancedPositionSliders();
    void refreshAdvancedScaleSliders();

    /// <summary>
    /// Determines if we have permission to animate the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to animate.</param>
    /// <returns>True if we have permission to animate, otherwise false.</returns>
    bool havePermissionToAnimateAvatar(LLVOAvatar* avatar) const;

    /// <summary>
    /// Determines if we could animate the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to animate.</param>
    /// <returns>True if the avatar is non-null, not dead, in the same region as self, otherwise false.</returns>
    bool couldAnimateAvatar(LLVOAvatar* avatar) const;

    /// <summary>
    /// Our instance of the class which lets us do the business of manipulating the avatar.
    /// This separates that business from the code-behind the UI.
    /// </summary>
    FSPoserAnimator mPoserAnimator;

    /// <summary>
    /// The supplied Joint name has a quaternion describing its rotation.
    /// This gets the kind of axial transformation required for 'easy' consumption of the joint's Euler angles on our UI.
    /// This facilitates 'conceptual' conversion of Euler frame to up/down, left/right and roll and is rather subjective.
    /// Thus, many of these 'conversions' are backed by values in the XML.
    /// </summary>
    /// <param name="jointName">The well-known name of the joint, eg: mChest.</param>
    /// <returns>The axial translation so the oily angles make better sense in terms of up/down/left/right/roll.</returns>
    /// <remarks>
    /// Euler angles aren't cartesian; they're one of 12 possible orderings or something, yes, yes.
    /// No the translation isn't untangling all of that, it's not needed until it is.
    /// We're not landing on Mars with this code, just offering a user reasonable thumb-twiddlings.
    /// </remarks>
    E_BoneAxisTranslation getJointTranslation(const std::string& jointName) const;

    /// <summary>
    /// Gets the collection of E_BoneAxisNegation values for the supplied joint.
    /// </summary>
    /// <param name="jointName">The name of the joind to get the axis transformation for.</param>
    /// <returns>The kind of axis transformation to perform.</returns>
    S32 getJointNegation(const std::string& jointName) const;

    /// <summary>
    /// Refreshes the text on the avatars scroll list based on their state.
    /// </summary>
    void refreshTextHighlightingOnAvatarScrollList();

    /// <summary>
    /// Refreshes the text on all joints scroll lists based on their state.
    /// </summary>
    void refreshTextHighlightingOnJointScrollLists();

    /// <summary>
    /// Sets the text of the save pose button.
    /// </summary>
    /// <param name="setAsSaveDiff">Whether to indicate a diff will be saved, instead of a pose.</param>
    void setSavePosesButtonText(bool setAsSaveDiff);

    /// <summary>
    /// Gets whether any avatar know by the UI is being posed.
    /// </summary>
    bool posingAnyoneOnScrollList();

    /// <summary>
    /// Applies the appropriate font-face (such as bold) to the text of the supplied list, to indicate use.
    /// </summary>
    /// <param name="listName">The name of the list to adjust text-face for.</param>
    /// <param name="avatar">The avatar to whom the list is relevant.</param>
    void addBoldToScrollList(LLScrollListCtrl* list, LLVOAvatar* avatar);

    /// <summary>
    /// Determines if the user has run this method twice within mDoubleClickInterval.
    /// </summary>
    /// <returns>true if this method has executed since mDoubleClickInterval seconds ago, otherwise false.</returns>
    bool notDoubleClicked();

    /// <summary>
    /// Gets whether the user wishes to reset the base-rotation to zero when they start editing a joint.
    /// </summary>
    /// <remarks>
    /// If a joint has a base-rotation of zero, the rotation then appears to be the user's work and qualifies to save to a re-importable format.
    /// </remarks>
    bool getWhetherToResetBaseRotationOnEdit();

    /// <summary>
    /// The time when the last click of a button was made.
    /// Utilized for controls needing a 'double click do' function.
    /// </summary>
    std::chrono::system_clock::time_point mTimeLastExecutedDoubleClickMethod = std::chrono::system_clock::now();

    /// <summary>
    /// The constant time interval, in seconds, a user must execute the notDoubleClicked twice to successfully 'double-click' a button.
    /// </summary>
    std::chrono::duration<double> const mDoubleClickInterval = std::chrono::duration<double>(0.3);

    /// <summary>
    /// Unwraps a normalized value from the trackball to a slider value.
    /// </summary>
    /// <param name="scale">The scale value from the trackball.</param>
    /// <returns>A value appropriate for fitting a slider.</returns>
    /// <remarks>
    /// If the trackpad is in 'infinite scroll' mode, it can produce normalized-values outside the range of the sliders.
    /// This method ensures whatever value the trackpad produces, they work with the sliders.
    /// </remarks>
    static F32 unWrapScale(F32 scale);

    FSVirtualTrackpad* mAvatarTrackball{ nullptr };

    LLSliderCtrl* mTrackpadSensitivitySlider{ nullptr };
    LLSliderCtrl* mLimbYawSlider{ nullptr };
    LLSliderCtrl* mLimbPitchSlider{ nullptr }; // pointing your nose up or down
    LLSliderCtrl* mLimbRollSlider{ nullptr }; // your ear touches your shoulder
    LLSliderCtrl* mPosXSlider{ nullptr };
    LLSliderCtrl* mPosYSlider{ nullptr };
    LLSliderCtrl* mPosZSlider{ nullptr };
    LLSliderCtrl* mAdvPosXSlider{ nullptr };
    LLSliderCtrl* mAdvPosYSlider{ nullptr };
    LLSliderCtrl* mAdvPosZSlider{ nullptr };
    LLSliderCtrl* mAdvScaleXSlider{ nullptr };
    LLSliderCtrl* mAdvScaleYSlider{ nullptr };
    LLSliderCtrl* mAdvScaleZSlider{ nullptr };

    LLTabContainer* mJointsTabs{ nullptr };
    LLTabContainer* mHandsTabs{ nullptr };

    LLScrollListCtrl* mAvatarSelectionScrollList{ nullptr };
    LLScrollListCtrl* mBodyJointsScrollList{ nullptr };
    LLScrollListCtrl* mFaceJointsScrollList{ nullptr };
    LLScrollListCtrl* mHandJointsScrollList{ nullptr };
    LLScrollListCtrl* mMiscJointsScrollList{ nullptr };
    LLScrollListCtrl* mCollisionVolumesScrollList{ nullptr };
    LLScrollListCtrl* mEntireAvJointScroll{ nullptr };
    LLScrollListCtrl* mPosesScrollList{ nullptr };
    LLScrollListCtrl* mHandPresetsScrollList{ nullptr };

    LLButton* mToggleAdvancedPanelBtn{ nullptr };
    LLButton* mStartStopPosingBtn{ nullptr };
    LLButton* mToggleLoadSavePanelBtn{ nullptr };
    LLButton* mBrowserFolderBtn{ nullptr };
    LLButton* mLoadPosesBtn{ nullptr };
    LLButton* mSavePosesBtn{ nullptr };
    LLButton* mFlipPoseBtn{ nullptr };
    LLButton* mFlipJointBtn{ nullptr };
    LLButton* mRecaptureBtn{ nullptr };
    LLButton* mTogglePosingBonesBtn{ nullptr };
    LLButton* mToggleMirrorRotationBtn{ nullptr };
    LLButton* mToggleSympatheticRotationBtn{ nullptr };
    LLButton* mToggleDeltaModeBtn{ nullptr };
    LLButton* mRedoChangeBtn{ nullptr };
    LLButton* mSetToTposeButton{ nullptr };

    LLLineEditor* mPoseSaveNameEditor{ nullptr };

    LLPanel* mAdvancedParentPnl{ nullptr };
    LLPanel* mJointsParentPnl{ nullptr };
    LLPanel* mTrackballPnl{ nullptr };
    LLPanel* mPositionRotationPnl{ nullptr };
    LLPanel* mBodyJointsPnl{ nullptr };
    LLPanel* mFaceJointsPnl{ nullptr };
    LLPanel* mHandsJointsPnl{ nullptr };
    LLPanel* mMiscJointsPnl{ nullptr };
    LLPanel* mCollisionVolumesPnl{ nullptr };
    LLPanel* mPosesLoadSavePnl{ nullptr };
    LLCheckBoxCtrl* mStopPosingOnCloseCbx{ nullptr };
    LLCheckBoxCtrl* mResetBaseRotOnEditCbx{ nullptr };
};

#endif
