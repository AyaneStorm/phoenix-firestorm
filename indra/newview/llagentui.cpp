/**
 * @file llagentui.cpp
 * @brief Utility methods to process agent's data as slurl's etc. before displaying
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llagentui.h"

// Library includes
#include "llparcel.h"

// Viewer includes
#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewerparcelmgr.h"
#include "llvoavatarself.h"
#include "llslurl.h"
// [RLVa:KB] - Checked: 2010-04-04 (RLVa-1.2.0d)
#include "rlvhandler.h"
// [/RLVa:KB]

// <FS:Ansariel> FIRE-1874: Show server channel in statusbar
#include "llappviewer.h"
// <FS:Beq> FIRE-10549 etc, incorrect grid URL on HG back links
#include "fsgridhandler.h"
#include "lfsimfeaturehandler.h"
// </FS:Beq>

//static
void LLAgentUI::buildFullname(std::string& name)
{
    if (isAgentAvatarValid())
        name = gAgentAvatarp->getFullname();
}

//static
void LLAgentUI::buildSLURL(LLSLURL& slurl, const bool escaped /*= true*/)
{
    LLSLURL return_slurl;
    LLViewerRegion *regionp = gAgent.getRegion();
    if (regionp)
    {
// <FS:CR> Aurora-sim var region teleports
#ifdef OPENSIM
        if (LLGridManager::instance().isInOpenSim())
        {
            return_slurl = LLSLURL(LFSimFeatureHandler::getInstance()->hyperGridURL(), regionp->getName(), gAgent.getPositionAgent());
        }
        else
#endif
// </FS:CR>
        // <FS:Oren> FIRE-30768: SLURL's don't work in VarRegions
          //// Make sure coordinates are within current region
          //LLVector3d global_pos = gAgent.getPositionGlobal();
          //LLVector3d region_origin = regionp->getOriginGlobal();
          //// -1 otherwise slurl will fmod 256 to 0.
          //// And valid slurl range is supposed to be 0..255
          //F64 max_val = REGION_WIDTH_METERS - 1;
          //global_pos.mdV[VX] = llclamp(global_pos[VX], region_origin[VX], region_origin[VX] + max_val);
          //global_pos.mdV[VY] = llclamp(global_pos[VY], region_origin[VY], region_origin[VY] + max_val);

          //return_slurl = LLSLURL(regionp->getName(), global_pos);
        return_slurl = LLSLURL(regionp->getName(), regionp->getOriginGlobal(), gAgent.getPositionGlobal());
        // </FS:Oren>
    }
    slurl = return_slurl;
}

//static
bool LLAgentUI::checkAgentDistance(const LLVector3& pole, F32 radius)
{
    F32 delta_x = gAgent.getPositionAgent().mV[VX] - pole.mV[VX];
    F32 delta_y = gAgent.getPositionAgent().mV[VY] - pole.mV[VY];

    return  sqrt( delta_x* delta_x + delta_y* delta_y ) < radius;
}
bool LLAgentUI::buildLocationString(std::string& str, ELocationFormat fmt,const LLVector3& agent_pos_region)
{
    LLViewerRegion* region = gAgent.getRegion();
    LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

    if (!region || !parcel) return false;

    S32 pos_x = S32(agent_pos_region.mV[VX] + 0.5f);
    S32 pos_y = S32(agent_pos_region.mV[VY] + 0.5f);
    S32 pos_z = S32(agent_pos_region.mV[VZ] + 0.5f);

    // Round the numbers based on the velocity
    F32 velocity_mag_sq = gAgent.getVelocity().magVecSquared();

    const F32 FLY_CUTOFF = 6.f;     // meters/sec
    const F32 FLY_CUTOFF_SQ = FLY_CUTOFF * FLY_CUTOFF;
    const F32 WALK_CUTOFF = 1.5f;   // meters/sec
    const F32 WALK_CUTOFF_SQ = WALK_CUTOFF * WALK_CUTOFF;

    if (velocity_mag_sq > FLY_CUTOFF_SQ)
    {
        pos_x -= pos_x % 4;
        pos_y -= pos_y % 4;
    }
    else if (velocity_mag_sq > WALK_CUTOFF_SQ)
    {
        pos_x -= pos_x % 2;
        pos_y -= pos_y % 2;
    }

    // <FS:Ansariel> FIRE-1874: Show server version in statusbar
    static LLCachedControl<bool> fsStatusbarShowSimulatorVersion(gSavedSettings, "FSStatusbarShowSimulatorVersion");
    std::string simulator_version;

    if (fsStatusbarShowSimulatorVersion)
    {
        std::istringstream simulator_name(gLastVersionChannel);
        std::string version_part;

        // Format is "Second Life Server 2020-03-20T18:40:52.538914"
        if ((simulator_name >> version_part) &&
            (simulator_name >> version_part) &&
            (simulator_name >> version_part) &&
            (simulator_name >> version_part))
        {
            size_t version_start = version_part.rfind('.');
            if (version_start != std::string::npos && version_start != version_part.length() - 1)
            {
                simulator_version = version_part.substr(version_start + 1);
            }
        }
    }
    // </FS:Ansariel> FIRE-1874: Show server version in statusbar

    // create a default name and description for the landmark
    std::string parcel_name = LLViewerParcelMgr::getInstance()->getAgentParcelName();
    std::string region_name = region->getName();
// [RLVa:KB] - Checked: 2010-04-04 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
    // RELEASE-RLVa: [SL-2.0.0] Check ELocationFormat to make sure our switch still makes sense
    if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
    {
        parcel_name = RlvStrings::getString(RlvStringKeys::Hidden::Parcel);
        region_name = RlvStrings::getString(RlvStringKeys::Hidden::Region);
        if (LOCATION_FORMAT_NO_MATURITY == fmt)
            fmt = LOCATION_FORMAT_LANDMARK;
        else if (LOCATION_FORMAT_FULL == fmt || LOCATION_FORMAT_V1 == fmt || LOCATION_FORMAT_V1_NO_COORDS == fmt)
            fmt = LOCATION_FORMAT_NO_COORDS;
    }
// [/RLVa:KB]
    std::string sim_access_string = region->getSimAccessString();
    std::string buffer;
    if( parcel_name.empty() )
    {
        // the parcel doesn't have a name
        switch (fmt)
        {
        case LOCATION_FORMAT_LANDMARK:
            buffer = llformat("%.100s", region_name.c_str());
            break;
        case LOCATION_FORMAT_NORMAL:
            buffer = llformat("%s", region_name.c_str());
            break;
        case LOCATION_FORMAT_NORMAL_COORDS:
            buffer = llformat("%s (%d, %d, %d)",
                region_name.c_str(),
                pos_x, pos_y, pos_z);
            break;
        case LOCATION_FORMAT_NO_COORDS:
            buffer = llformat("%s%s%s",
                region_name.c_str(),
                sim_access_string.empty() ? "" : " - ",
                sim_access_string.c_str());
            break;
        case LOCATION_FORMAT_NO_MATURITY:
            buffer = llformat("%s (%d, %d, %d)",
                region_name.c_str(),
                pos_x, pos_y, pos_z);
            break;
        case LOCATION_FORMAT_FULL:
            buffer = llformat("%s (%d, %d, %d)%s%s",
                region_name.c_str(),
                pos_x, pos_y, pos_z,
                sim_access_string.empty() ? "" : " - ",
                sim_access_string.c_str());
            break;
        // <FS:Ansariel> V1 format statusbar
        case LOCATION_FORMAT_V1:
            if (fsStatusbarShowSimulatorVersion && !simulator_version.empty())
            {
                buffer = llformat("%s - %s - (%d, %d, %d)%s%s",
                    region_name.c_str(),
                    simulator_version.c_str(),
                    pos_x, pos_y, pos_z,
                    sim_access_string.empty() ? "" : " - ",
                    sim_access_string.c_str());
            }
            else
            {
                buffer = llformat("%s (%d, %d, %d)%s%s",
                    region_name.c_str(),
                    pos_x, pos_y, pos_z,
                    sim_access_string.empty() ? "" : " - ",
                    sim_access_string.c_str());
            }
            break;
        case LOCATION_FORMAT_V1_NO_COORDS:
            if (fsStatusbarShowSimulatorVersion && !simulator_version.empty())
            {
                buffer = llformat("%s - %s%s%s",
                    region_name.c_str(),
                    simulator_version.c_str(),
                    sim_access_string.empty() ? "" : " - ",
                    sim_access_string.c_str());
            }
            else
            {
                buffer = llformat("%s%s%s",
                    region_name.c_str(),
                    sim_access_string.empty() ? "" : " - ",
                    sim_access_string.c_str());
            }
            break;
        // </FS:Ansariel> V1 format statusbar
        }
    }
    else
    {
        // the parcel has a name, so include it in the landmark name
        switch (fmt)
        {
        case LOCATION_FORMAT_LANDMARK:
            buffer = llformat("%.100s", parcel_name.c_str());
            break;
        case LOCATION_FORMAT_NORMAL:
            buffer = llformat("%s, %s", parcel_name.c_str(), region_name.c_str());
            break;
        case LOCATION_FORMAT_NORMAL_COORDS:
            buffer = llformat("%s (%d, %d, %d)",
                parcel_name.c_str(),
                pos_x, pos_y, pos_z);
            break;
        case LOCATION_FORMAT_NO_MATURITY:
            buffer = llformat("%s, %s (%d, %d, %d)",
                parcel_name.c_str(),
                region_name.c_str(),
                pos_x, pos_y, pos_z);
            break;
        case LOCATION_FORMAT_NO_COORDS:
            buffer = llformat("%s, %s%s%s",
                              parcel_name.c_str(),
                              region_name.c_str(),
                              sim_access_string.empty() ? "" : " - ",
                              sim_access_string.c_str());
                break;
        case LOCATION_FORMAT_FULL:
            buffer = llformat("%s, %s (%d, %d, %d)%s%s",
                parcel_name.c_str(),
                region_name.c_str(),
                pos_x, pos_y, pos_z,
                sim_access_string.empty() ? "" : " - ",
                sim_access_string.c_str());
            break;
        // <FS:Ansariel> V1 format statusbar
        case LOCATION_FORMAT_V1:
            if (fsStatusbarShowSimulatorVersion && !simulator_version.empty())
            {
                buffer = llformat("%s - %s - (%d, %d, %d)%s%s - %s",
                    region_name.c_str(),
                    simulator_version.c_str(),
                    pos_x, pos_y, pos_z,
                    sim_access_string.empty() ? "" : " - ",
                    sim_access_string.c_str(),
                    parcel_name.c_str());
            }
            else
            {
                buffer = llformat("%s (%d, %d, %d)%s%s - %s",
                    region_name.c_str(),
                    pos_x, pos_y, pos_z,
                    sim_access_string.empty() ? "" : " - ",
                    sim_access_string.c_str(),
                    parcel_name.c_str());
            }
            break;
        case LOCATION_FORMAT_V1_NO_COORDS:
            if (fsStatusbarShowSimulatorVersion && !simulator_version.empty())
            {
                buffer = llformat("%s - %s%s%s - %s",
                    region_name.c_str(),
                    simulator_version.c_str(),
                    sim_access_string.empty() ? "" : " - ",
                    sim_access_string.c_str(),
                    parcel_name.c_str());
            }
            else
            {
                buffer = llformat("%s%s%s - %s",
                    region_name.c_str(),
                    sim_access_string.empty() ? "" : " - ",
                    sim_access_string.c_str(),
                    parcel_name.c_str());
            }
            break;
        // </FS:Ansariel> V1 format statusbar
        }
    }
    str = buffer;
    return true;
}
bool LLAgentUI::buildLocationString(std::string& str, ELocationFormat fmt)
{
    return buildLocationString(str,fmt, gAgent.getPositionAgent());
}
