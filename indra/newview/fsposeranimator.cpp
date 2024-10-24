/**
 * @file fsposeranimator.cpp
 * @brief business-layer for posing your (and other) avatar(s) and animeshes.
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

#include <deque>
#include <boost/algorithm/string.hpp>
#include "fsposeranimator.h"
#include "llcharacter.h"
#include "llagent.h"
#include "fsposingmotion.h"

FSPoserAnimator::FSPoserAnimator() {}
FSPoserAnimator::~FSPoserAnimator() {}
std::map<LLUUID, LLAssetID> FSPoserAnimator::_avatarIdToRegisteredAnimationId;

bool FSPoserAnimator::isPosingAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    if (posingMotion->isStopped())
        return false;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    return posingMotion->currentlyPosingJoint(jointPose);
}

void FSPoserAnimator::setPosingAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint, bool shouldPose)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    bool arePosing = isPosingAvatarJoint(avatar, joint);
    if ( (arePosing && shouldPose) || (!arePosing && !shouldPose) ) // could !XOR, but this is readable
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    if (shouldPose)
        posingMotion->addJointToState(jointPose);
    else
        posingMotion->removeJointFromState(jointPose);
}

void FSPoserAnimator::resetAvatarJoint(LLVOAvatar *avatar, FSPoserJoint joint)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->setTargetPosition(jointPose->getBeginningPosition());
    jointPose->setTargetRotation(jointPose->getBeginningRotation());
}

void FSPoserAnimator::undoLastJointRotation(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->undoLastRotationSet();

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->undoLastRotationSet();
}

void FSPoserAnimator::undoLastJointPosition(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->undoLastPositionSet();

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->undoLastPositionSet();
}

void FSPoserAnimator::undoLastJointScale(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->undoLastScaleSet();

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->undoLastScaleSet();
}

void FSPoserAnimator::resetJointPosition(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->setTargetPosition(jointPose->getBeginningPosition());

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->setTargetPosition(oppositeJointPose->getBeginningPosition());
}

void FSPoserAnimator::resetJointScale(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->setTargetScale(jointPose->getBeginningScale());

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->setTargetScale(oppositeJointPose->getBeginningScale());
}

bool FSPoserAnimator::canRedoJointRotation(LLVOAvatar* avatar, FSPoserJoint joint)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    if (posingMotion->isStopped())
        return false;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return false;

    return jointPose->canRedoRotation();
}

void FSPoserAnimator::redoLastJointRotation(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->redoLastRotationSet();

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->redoLastRotationSet();
}

void FSPoserAnimator::redoLastJointPosition(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->redoLastPositionSet();

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->redoLastPositionSet();
}

void FSPoserAnimator::redoLastJointScale(LLVOAvatar* avatar, FSPoserJoint joint, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    if (posingMotion->isStopped())
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return;

    jointPose->redoLastScaleSet();

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint.mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->redoLastScaleSet();
}

LLVector3 FSPoserAnimator::getJointPosition(LLVOAvatar* avatar, FSPoserJoint joint, bool forRecapture)
{
    LLVector3 pos;
    if (!isAvatarSafeToUse(avatar))
        return pos;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return pos;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return pos;

    if (forRecapture)
        pos = jointPose->getCurrentPosition();
    else
        pos = jointPose->getTargetPosition();

    return pos;
}

void FSPoserAnimator::setJointPosition(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 position, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;
    if (!joint)
        return;

    std::string jn = joint->jointName();
    if (jn.empty())
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(jn);
    if (!jointPose)
        return;

    if (style == NONE)
    {
        jointPose->setTargetPosition(position);
        return;
    }

    LLVector3 positionDelta = jointPose->getTargetPosition() - position;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    LLVector3 oppositeJointPosition = oppositeJointPose->getTargetPosition();
    switch (style)
    {
        case SYMPATHETIC:
            jointPose->setTargetPosition(position);
            oppositeJointPose->setTargetPosition(oppositeJointPosition - positionDelta);
            break;

        case MIRROR:
            jointPose->setTargetPosition(position);
            oppositeJointPose->setTargetPosition(oppositeJointPosition + positionDelta);
            break;

        default:
            break;
    }
}

LLVector3 FSPoserAnimator::getJointRotation(LLVOAvatar *avatar, FSPoserJoint joint, E_BoneAxisTranslation translation, S32 negation, bool forRecapture)
{
    LLVector3 vec3;
    if (!isAvatarSafeToUse(avatar))
        return vec3;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return vec3;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return vec3;

    LLQuaternion rot;
    if (forRecapture)
        rot = jointPose->getCurrentRotation();
    else
        rot = jointPose->getTargetRotation();
    
    return translateRotationFromQuaternion(translation, negation, rot);
}

void FSPoserAnimator::setJointRotation(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 rotation, E_BoneDeflectionStyles style,
                                       E_BoneAxisTranslation translation, S32 negation)
{
    if (!isAvatarSafeToUse(avatar))
        return;
    if (!joint)
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    LLQuaternion rot_quat = translateRotationToQuaternion(translation, negation, rotation);
    switch (style)
    {
        case SYMPATHETIC:
        case MIRROR:
            jointPose->setTargetRotation(rot_quat);
            break;

        case DELTAMODE:
            jointPose->applyDeltaRotation(rot_quat);
            return;
            
        case NONE:
        default:
            jointPose->setTargetRotation(rot_quat);
            return;
    }

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    LLQuaternion inv_quat;
    switch (style)
    {
        case SYMPATHETIC:
            oppositeJointPose->setTargetRotation(rot_quat);
            break;

        case MIRROR:
            inv_quat = LLQuaternion(-rot_quat.mQ[VX], rot_quat.mQ[VY], -rot_quat.mQ[VZ], rot_quat.mQ[VW]);
            oppositeJointPose->setTargetRotation(inv_quat);
            break;

        default:
            break;
    }
}

void FSPoserAnimator::reflectJoint(LLVOAvatar *avatar, const FSPoserJoint *joint)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    if (!joint)
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint->jointName());
    if (!jointPose)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
    {
        LLQuaternion rot_quat = jointPose->getTargetRotation();
        LLQuaternion inv_quat = LLQuaternion(-rot_quat.mQ[VX], rot_quat.mQ[VY], -rot_quat.mQ[VZ], rot_quat.mQ[VW]);

        jointPose->setTargetRotation(inv_quat);
        return;
    }

    LLQuaternion first_quat  = jointPose->getTargetRotation();
    LLQuaternion first_inv   = LLQuaternion(-first_quat.mQ[VX], first_quat.mQ[VY], -first_quat.mQ[VZ], first_quat.mQ[VW]);
    LLQuaternion second_quat = oppositeJointPose->getTargetRotation();
    LLQuaternion second_inv  = LLQuaternion(-second_quat.mQ[VX], second_quat.mQ[VY], -second_quat.mQ[VZ], second_quat.mQ[VW]);
    jointPose->setTargetRotation(second_inv);
    oppositeJointPose->setTargetRotation(first_inv);
}

void FSPoserAnimator::flipEntirePose(LLVOAvatar *avatar)
{
    if (!isAvatarSafeToUse(avatar))
        return;

    for (size_t index = 0; index != PoserJoints.size(); ++index)
    {
        if (PoserJoints[index].dontFlipOnMirror()) // we only flip one side.
            continue;

        bool currentlyPosing = isPosingAvatarJoint(avatar, PoserJoints[index]);
        if (!currentlyPosing)
            continue;

        auto oppositeJoint = getPoserJointByName(PoserJoints[index].mirrorJointName());
        if (oppositeJoint)
        {
            bool currentlyPosingOppositeJoint = isPosingAvatarJoint(avatar, *oppositeJoint);
            if (!currentlyPosingOppositeJoint)
                continue;
        }

        reflectJoint(avatar, &PoserJoints[index]);
    }
}

// from the UI to the bone, the inverse translation, the un-swap, the backwards
LLQuaternion FSPoserAnimator::translateRotationToQuaternion(E_BoneAxisTranslation translation, S32 negation,
                                                            LLVector3 rotation)
{
    if (negation & NEGATE_ALL)
    {
        rotation.mV[VX] *= -1;
        rotation.mV[VY] *= -1;
        rotation.mV[VZ] *= -1;
    }
    else
    {
        if (negation & NEGATE_YAW)
            rotation.mV[VX] *= -1;
        if (negation & NEGATE_PITCH)
            rotation.mV[VY] *= -1;
        if (negation & NEGATE_ROLL)
            rotation.mV[VZ] *= -1;
    }

    LLMatrix3    rot_mat;
    switch (translation)
    {
        case SWAP_YAW_AND_ROLL:
            rot_mat = LLMatrix3(rotation.mV[VZ], rotation.mV[VY], rotation.mV[VX]);
            break;

        case SWAP_YAW_AND_PITCH:
            rot_mat = LLMatrix3(rotation.mV[VY], rotation.mV[VX], rotation.mV[VZ]);
            break;

        case SWAP_ROLL_AND_PITCH:
            rot_mat = LLMatrix3(rotation.mV[VX], rotation.mV[VZ], rotation.mV[VY]);
            break;

        case SWAP_X2Y_Y2Z_Z2X:
            rot_mat = LLMatrix3(rotation.mV[VZ], rotation.mV[VX], rotation.mV[VY]);
            break;

        case SWAP_X2Z_Y2X_Z2Y:
            rot_mat = LLMatrix3(rotation.mV[VY], rotation.mV[VZ], rotation.mV[VX]);
            break;

        case SWAP_NOTHING:
        default:
            rot_mat = LLMatrix3(rotation.mV[VX], rotation.mV[VY], rotation.mV[VZ]);
            break;
    }

    LLQuaternion rot_quat;
    rot_quat = LLQuaternion(rot_mat) * rot_quat;

    return rot_quat;
}

// from the bone to the UI; this is the 'forwards' use of the enum
LLVector3 FSPoserAnimator::translateRotationFromQuaternion(E_BoneAxisTranslation translation, S32 negation, LLQuaternion rotation)
{
    LLVector3 vec3;

    switch (translation)
    {
        case SWAP_YAW_AND_ROLL:
            rotation.getEulerAngles(&vec3.mV[VZ], &vec3.mV[VY], &vec3.mV[VX]);
            break;

        case SWAP_YAW_AND_PITCH:
            rotation.getEulerAngles(&vec3.mV[VY], &vec3.mV[VX], &vec3.mV[VZ]);
            break;

        case SWAP_ROLL_AND_PITCH:
            rotation.getEulerAngles(&vec3.mV[VX], &vec3.mV[VZ], &vec3.mV[VY]);
            break;

        case SWAP_X2Y_Y2Z_Z2X:
            rotation.getEulerAngles(&vec3.mV[VZ], &vec3.mV[VX], &vec3.mV[VY]);
            break;

        case SWAP_X2Z_Y2X_Z2Y:
            rotation.getEulerAngles(&vec3.mV[VY], &vec3.mV[VZ], &vec3.mV[VX]);
            break;

        case SWAP_NOTHING:
        default:
            rotation.getEulerAngles(&vec3.mV[VX], &vec3.mV[VY], &vec3.mV[VZ]);
            break;
    }

    if (negation & NEGATE_ALL)
    {
        vec3.mV[VX] *= -1;
        vec3.mV[VY] *= -1;
        vec3.mV[VZ] *= -1;
    }
    else
    {
        if (negation & NEGATE_YAW)
            vec3.mV[VX] *= -1;
        if (negation & NEGATE_PITCH)
            vec3.mV[VY] *= -1;
        if (negation & NEGATE_ROLL)
            vec3.mV[VZ] *= -1;
    }

    return vec3;
}

LLVector3 FSPoserAnimator::getJointScale(LLVOAvatar* avatar, FSPoserJoint joint, bool forRecapture)
{
    LLVector3 scale;
    if (!isAvatarSafeToUse(avatar))
        return scale;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return scale;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(joint.jointName());
    if (!jointPose)
        return scale;

    if (forRecapture)
        scale = jointPose->getCurrentScale();
    else
        scale = jointPose->getTargetScale();

    return scale;
}

void FSPoserAnimator::setJointScale(LLVOAvatar *avatar, const FSPoserJoint *joint, LLVector3 scale, E_BoneDeflectionStyles style)
{
    if (!isAvatarSafeToUse(avatar))
        return;
    if (!joint)
        return;

    std::string jn = joint->jointName();
    if (jn.empty())
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    FSPosingMotion::FSJointPose* jointPose = posingMotion->getJointPoseByJointName(jn);
    if (!jointPose)
        return;

    jointPose->setTargetScale(scale);

    if (style == NONE)
        return;

    FSPosingMotion::FSJointPose* oppositeJointPose = posingMotion->getJointPoseByJointName(joint->mirrorJointName());
    if (!oppositeJointPose)
        return;

    oppositeJointPose->setTargetScale(scale);
}

const FSPoserAnimator::FSPoserJoint* FSPoserAnimator::getPoserJointByName(std::string jointName)
{
    for (size_t index = 0; index != PoserJoints.size(); ++index)
    {
        if (boost::iequals(PoserJoints[index].jointName(), jointName))
            return &PoserJoints[index];
    }

    return nullptr;
}

bool FSPoserAnimator::tryPosingAvatar(LLVOAvatar *avatar)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = findOrCreatePosingMotion(avatar);
    if (!posingMotion)
        return false;

    if (posingMotion->isStopped())
    {
        if (avatar->isSelf())
            gAgent.stopFidget();

        avatar->startDefaultMotions();
        avatar->startMotion(posingMotion->motionId());

        return true;
    }

    return false;
}

void FSPoserAnimator::stopPosingAvatar(LLVOAvatar *avatar)
{
    if (!avatar || avatar->isDead())
        return;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return;

    avatar->stopMotion(posingMotion->motionId());
}

bool FSPoserAnimator::isPosingAvatar(LLVOAvatar* avatar)
{
    if (!isAvatarSafeToUse(avatar))
        return false;

    FSPosingMotion* posingMotion = getPosingMotion(avatar);
    if (!posingMotion)
        return false;

    return !posingMotion->isStopped();
}

FSPosingMotion* FSPoserAnimator::getPosingMotion(LLVOAvatar* avatar)
{
    if (!isAvatarSafeToUse(avatar))
        return nullptr;

    if (_avatarIdToRegisteredAnimationId.find(avatar->getID()) == _avatarIdToRegisteredAnimationId.end())
        return nullptr;

    return dynamic_cast<FSPosingMotion*>(avatar->findMotion(_avatarIdToRegisteredAnimationId[avatar->getID()]));
}

FSPosingMotion* FSPoserAnimator::findOrCreatePosingMotion(LLVOAvatar* avatar)
{
    FSPosingMotion* motion = getPosingMotion(avatar);

    if (!motion)
    {
        LLTransactionID mTransactionID;
        mTransactionID.generate();
        LLAssetID animationAssetId = mTransactionID.makeAssetID(gAgent.getSecureSessionID());

        if (avatar->registerMotion(animationAssetId, FSPosingMotion::create))
            _avatarIdToRegisteredAnimationId[avatar->getID()] = animationAssetId;

        return dynamic_cast<FSPosingMotion*>(avatar->createMotion(animationAssetId));
    }

    return motion;
}

bool FSPoserAnimator::isAvatarSafeToUse(LLVOAvatar *avatar)
{
    if (!avatar)
        return false;
    if (avatar->isDead())
        return false;
    if (avatar->getRegion() != gAgent.getRegion())
        return false;

    return true;
}

bool FSPoserAnimator::writePoseAsBvh(llofstream* fileStream, LLVOAvatar* avatar)
{
    if (!fileStream || !avatar)
        return false;

    *fileStream << "HIERARCHY" << std::endl;
    auto startingJoint = getPoserJointByName("mPelvis");
    writeBvhFragment(fileStream, avatar, startingJoint, 0);
    *fileStream << "MOTION" << std::endl;
    *fileStream << "Frames:    1" << std::endl;
    *fileStream << "Frame Time: 1" << std::endl;
    writeBvhMotion(fileStream, avatar, startingJoint);
    *fileStream << std::endl;

    return true;
}

bool FSPoserAnimator::writeBvhFragment(llofstream* fileStream, LLVOAvatar* avatar, const FSPoserJoint* joint, int tabStops)
{
    if (!joint)
        return false;

    auto position = getJointPosition(avatar, *joint);

    switch (joint->boneType())
    {
        case WHOLEAVATAR:
            *fileStream << "ROOT " + joint->jointName() << std::endl;
            *fileStream << "{" << std::endl;
            *fileStream << getTabs(tabStops + 1) + "OFFSET " + vec3ToXYZString(position) << std::endl;
            *fileStream << getTabs(tabStops + 1) + "CHANNELS 6 Xposition Yposition Zposition Xrotation Yrotation Zrotation" << std::endl;
            break;

        default:
            *fileStream << getTabs(tabStops) + "JOINT " + joint->jointName() << std::endl;
            *fileStream << getTabs(tabStops) + "{" << std::endl;
            *fileStream << getTabs(tabStops + 1) + "OFFSET " + vec3ToXYZString(position) << std::endl;
            *fileStream << getTabs(tabStops + 1) + "CHANNELS 3 Xrotation Yrotation Zrotation" << std::endl;
            break;
    }

    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    if (numberOfBvhChildNodes > 0)
    {
        for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
        {
            auto nextJoint = getPoserJointByName(joint->bvhChildren()[index]);
            writeBvhFragment(fileStream, avatar, nextJoint, tabStops + 1);
        }
    }
    else
    {
        *fileStream << getTabs(tabStops + 1) + "End Site" << std::endl;
        *fileStream << getTabs(tabStops + 1) + "{" << std::endl;
        *fileStream << getTabs(tabStops + 2) + "OFFSET " + vec3ToXYZString(position) << std::endl; // I don't understand this node
        *fileStream << getTabs(tabStops + 1) + "}" << std::endl;
    }

    *fileStream << getTabs(tabStops) + "}" << std::endl;
    return true;
}

bool FSPoserAnimator::writeBvhMotion(llofstream* fileStream, LLVOAvatar* avatar, const FSPoserJoint* joint)
{
    if (!joint)
        return false;

    auto rotation = getJointRotation(avatar, *joint, SWAP_NOTHING, NEGATE_NOTHING, true);
    auto position = getJointPosition(avatar, *joint, true);

    switch (joint->boneType())
    {
        case WHOLEAVATAR:
            *fileStream << vec3ToXYZString(position) + " " + rotationToYZXString(rotation);
            break;

        default:
            *fileStream << " " + rotationToYZXString(rotation);
            break;
    }

    size_t numberOfBvhChildNodes = joint->bvhChildren().size();
    for (size_t index = 0; index != numberOfBvhChildNodes; ++index)
    {
        auto nextJoint = getPoserJointByName(joint->bvhChildren()[index]);
        writeBvhMotion(fileStream, avatar, nextJoint);
    }

    return true;
}

std::string FSPoserAnimator::vec3ToXYZString(LLVector3 val)
{
    return f32ToString(val[VX]) + " " + f32ToString(val[VY]) + " " + f32ToString(val[VZ]);
}

std::string FSPoserAnimator::rotationToYZXString(LLVector3 val)
{
    return f32ToString(val[VY] * RAD_TO_DEG) + " " + f32ToString(val[VZ] * RAD_TO_DEG) + " " + f32ToString(val[VX] * RAD_TO_DEG);
}

std::string FSPoserAnimator::getTabs(int numOfTabstops)
{
    std::string tabSpaces;
    for (int i = 0; i < numOfTabstops; i++)
        tabSpaces += "\t";

    return tabSpaces;
}

std::string FSPoserAnimator::f32ToString(F32 val)
{
    std::string str;
    char        buf[20];
    snprintf(buf, 20, "%f", val);
    str = buf;
    return str;
}
