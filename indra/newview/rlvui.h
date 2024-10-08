/**
 *
 * Copyright (c) 2009-2011, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

#ifndef RLV_UI_H
#define RLV_UI_H

#include "llfloaterreg.h"
#include "llsingleton.h"

#include "rlvdefines.h"

// ============================================================================
// RlvUIEnabler - self-contained class that handles disabling or reenabling certain aspects of the viewer's UI
//

class RlvUIEnabler : public LLSingleton<RlvUIEnabler>
{
    LLSINGLETON(RlvUIEnabler);

protected:
    friend class RlvHandler;

    /*
     * Signal callbacks
     */
public:
    void onBehaviourToggle(ERlvBehaviour eBhvr, ERlvParamType eType);   // RlvHandler::rlv_behaviour_signal_t

    /*
     * Behaviour handlers
     */
protected:
    void onRefreshHoverText();                                          // showloc, shownames, showhovertext(all|world|hud)
    void onToggleMovement();                                            // fly, alwaysrun and temprun
    void onToggleShowLoc();                                             // showloc
    void onToggleShowMinimap();                                         // showminimap
    void onToggleShowWorldMap();                                        // showworldmap
    void onToggleTp();                                                  // tploc and tplm
    void onToggleUnsit();                                               // unsit
    void onToggleViewXXX();                                             // viewnote, viewscript, viewtexture
    void onUpdateLoginLastLocation(bool fQuitting);                     // tploc and unsit

    /*
     * Floater and sidebar validation callbacks
     */
public:
#ifdef CATZNIP_STRINGVIEW
    bool addGenericFloaterFilter(const std::string& strFloaterName, const boost::string_view& strRlvNotification);
#else
    bool addGenericFloaterFilter(const std::string& strFloaterName, const std::string& strRlvNotification);
#endif // CATZNIP_STRINGVIEW
    bool addGenericFloaterFilter(const std::string& strFloaterName, const std::function<void()>& fn = nullptr);
    bool removeGenericFloaterFilter(const std::string& strFloaterName);

protected:
    bool filterFloaterGeneric(std::string_view, const LLSD&);
    boost::signals2::connection m_ConnFloaterGeneric;
    bool filterFloaterShowLoc(std::string_view, const LLSD& );
    boost::signals2::connection m_ConnFloaterShowLoc;                   // showloc
    bool filterFloaterViewXXX(std::string_view, const LLSD&);
    boost::signals2::connection m_ConnFloaterViewXXX;                   // viewnote, viewscript, viewtexture

    bool filterPanelShowLoc(std::string_view, std::string_view, const LLSD& );
    boost::signals2::connection m_ConnPanelShowLoc;                     // showloc

    /*
     * Helper functions
     */
public:
    static bool canViewParcelProperties();                              // showloc
    static bool canViewRegionProperties();                              // showloc
    static bool hasOpenIM(const LLUUID& idAgent);                       // shownames
    static bool hasOpenProfile(const LLUUID& idAgent);                  // shownames

    /*
     * Member variables
     */
protected:
    typedef boost::function<void(bool)> behaviour_handler_t;
    typedef std::multimap<ERlvBehaviour, behaviour_handler_t> behaviour_handler_map_t;
    behaviour_handler_map_t m_Handlers;

    std::map<std::string, std::function<void()>, std::less<>> m_FilteredFloaterMap;
};

// ============================================================================

#endif // RLV_UI_H
