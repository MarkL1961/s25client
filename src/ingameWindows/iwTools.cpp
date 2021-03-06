// Copyright (c) 2005 - 2015 Settlers Freaks (sf-team at siedler25.org)
//
// This file is part of Return To The Roots.
//
// Return To The Roots is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Return To The Roots is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Return To The Roots. If not, see <http://www.gnu.org/licenses/>.

#include "defines.h" // IWYU pragma: keep
#include "iwTools.h"

#include "Loader.h"
#include "GameClient.h"
#include "GamePlayer.h"
#include "controls/ctrlDeepening.h"
#include "controls/ctrlProgress.h"
#include "world/GameWorldViewer.h"
#include "world/GameWorldBase.h"
#include "WindowManager.h"
#include "iwHelp.h"
#include "notifications/NotificationManager.h"
#include "notifications/ToolNote.h"
#include "addons/const_addons.h"
#include "gameData/const_gui_ids.h"
#include "gameData/ToolConsts.h"
#include "helpers/converters.h"
#include "libutil/src/colors.h"
#include <boost/lambda/lambda.hpp>

iwTools::iwTools(const GameWorldViewer& gwv, GameCommandFactory& gcFactory):
    IngameWindow(CGI_TOOLS, IngameWindow::posAtMouse,  166 + (gwv.GetWorld().GetGGS().isEnabled(AddonId::TOOL_ORDERING) ? 46 : 0), 432, _("Tools"), LOADER.GetImageN("io", 5)),
    gwv(gwv), gcFactory(gcFactory),
    settings_changed(false), ordersChanged(false), shouldUpdateTexts(false), isReplay(GAMECLIENT.IsReplayModeOn())
{
    // Einzelne Balken
    for(unsigned i = 0; i < TOOL_COUNT; i++)
        AddToolSettingSlider(i, TOOLS[i]);

    const GlobalGameSettings& settings = gwv.GetWorld().GetGGS();
    if (settings.isEnabled(AddonId::TOOL_ORDERING))
    {
        // qx:tools
        for (unsigned i = 0; i < TOOL_COUNT; ++i)
        {
            AddImageButton(100 + i * 2, 174, 25   + i * 28, 20, 13, TC_GREY, LOADER.GetImageN("io",  33), "+1");
            AddImageButton(101 + i * 2, 174, 25 + 13 + i * 28, 20, 13, TC_GREY, LOADER.GetImageN("io",  34), "-1");
            AddDeepening  (200 + i, 151, 25 + 4 + i * 28, 20, 18, TC_GREY, "", NormalFont, COLOR_YELLOW);
        }
        UpdateTexts();
    }

    // Info
    AddImageButton(12,  18, 384, 30, 32, TC_GREY, LOADER.GetImageN("io",  225), _("Help"));
    if(settings.isEnabled(AddonId::TOOL_ORDERING))
        AddImageButton(15, 130, 384, 30, 32, TC_GREY, LOADER.GetImageN("io", 216), _("Zero all production"));
    // Standard
    AddImageButton(13, 118 + (settings.isEnabled(AddonId::TOOL_ORDERING) ? 46 : 0), 384, 30, 32, TC_GREY, LOADER.GetImageN("io", 191), _("Default"));

    // Einstellungen festlegen
    UpdateSettings();

    // Netzwerk-Übertragungs-Timer
    AddTimer(14, 2000);

    toolSubscription = gwv.GetWorld().GetNotifications().subscribe<ToolNote>(boost::lambda::var(shouldUpdateTexts) = true);
}

void iwTools::AddToolSettingSlider(unsigned id, GoodType ware)
{
    ctrlProgress* el = AddProgress(id, 17, 25 + id * 28, 132, 26, TC_GREY, 140 + id * 2 + 1, 140 + id * 2, 10, _(WARE_NAMES[ware]), 4, 4, 0, _("Less often"), _("More often"));
    if(isReplay)
        el->ActivateControls(false);
}

iwTools::~iwTools()
{
    TransmitSettings();
}

void iwTools::TransmitSettings()
{
    if(isReplay)
        return;
    // Wurden Einstellungen verändert?
    if(settings_changed || ordersChanged)
    {
        // Einstellungen speichern
        for(unsigned i = 0; i < TOOL_COUNT; ++i)
            GAMECLIENT.visual_settings.tools_settings[i] = (unsigned char)GetCtrl<ctrlProgress>(i)->GetPosition();

        gcFactory.ChangeTools(GAMECLIENT.visual_settings.tools_settings, ordersChanged ? gwv.GetPlayer().GetToolOrderDelta() : NULL);

        settings_changed = false;
        ordersChanged = false;
    }
}

void iwTools::UpdateTexts()
{
    if (gwv.GetWorld().GetGGS().isEnabled(AddonId::TOOL_ORDERING))
    {
        const GamePlayer& localPlayer = gwv.GetPlayer();
        for (unsigned i = 0; i < TOOL_COUNT; ++i)
        {
            ctrlDeepening* field = GetCtrl<ctrlDeepening>(200 + i);
            field->SetText(helpers::toString(isReplay ? localPlayer.GetToolsOrdered(i) : localPlayer.GetToolsOrderedVisual(i)));
        }
    }
}

void iwTools::Msg_PaintBefore()
{
    if (shouldUpdateTexts)
    {
        UpdateTexts();
        shouldUpdateTexts = false;
    }
}

void iwTools::Msg_ButtonClick(const unsigned int ctrl_id)
{
    if(isReplay)
        return;
    // qx:tools
    if ( ctrl_id >= 100 && ctrl_id < (100 + 2 * TOOL_COUNT) )
    {
        unsigned int tool = (ctrl_id - 100) / 2;
        const GamePlayer& me = gwv.GetPlayer();

        if (ctrl_id & 0x1)
            ordersChanged |= me.ChangeToolOrderVisual(tool, -1);
        else
            ordersChanged |= me.ChangeToolOrderVisual(tool, +1);

        ctrlDeepening* field = GetCtrl<ctrlDeepening>(200 + tool);
        field->SetText(helpers::toString(me.GetToolsOrderedVisual(tool)));
    }
    else
        switch(ctrl_id)
        {
            default: return;
            case 12:
                WINDOWMANAGER.Show(new iwHelp(GUI_ID(CGI_HELP), _(
                    "These settings control the tool production of your metalworks. "
                    "The higher the value, the more likely this tool is to be produced.")));
            case 13: // Standard
                GAMECLIENT.visual_settings.tools_settings = GAMECLIENT.default_settings.tools_settings;
                UpdateSettings();
                settings_changed = true;
                break;
            case 15: // Zero all
                std::fill(GAMECLIENT.visual_settings.tools_settings.begin(), GAMECLIENT.visual_settings.tools_settings.end(), 0);
                UpdateSettings();
                settings_changed = true;
                break;
        }
}

void iwTools::Msg_ProgressChange(const unsigned int  /*ctrl_id*/, const unsigned short  /*position*/)
{
    // Einstellungen wurden geändert
    settings_changed = true;
}

void iwTools::Msg_Timer(const unsigned int  /*ctrl_id*/)
{
    if(isReplay)
        // Im Replay aktualisieren wir die Werte
        UpdateSettings();
    else
        // Im normalen Spielmodus schicken wir den ganzen Spaß ab
        TransmitSettings();
}

void iwTools::UpdateSettings()
{
    if(isReplay)
    {
        const GamePlayer& localPlayer = gwv.GetPlayer();
        for(unsigned i = 0; i < TOOL_COUNT; ++i)
            GetCtrl<ctrlProgress>(i)->SetPosition(localPlayer.GetToolPriority(i));
    }else
    {
        for(unsigned i = 0; i < TOOL_COUNT; ++i)
            GetCtrl<ctrlProgress>(i)->SetPosition(GAMECLIENT.visual_settings.tools_settings[i]);
    }
}
