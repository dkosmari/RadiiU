/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "ConfirmExitPopup.hpp"

#include "App.hpp"
#include "ButtonHBox.hpp"
#include "IconsFontAwesome4.h"
#include "tracer.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


namespace ConfirmExitPopup {

    namespace {

        // Types

        enum class State {
            hidden,
            queued,
            visible,
        };


        // Constants
        const std::string popup_id = "Confirm Exit";

        // Variables

        State state = State::hidden;

    } // namespace


    void
    open()
    {
        TRACE_FUNC;

        state = State::queued;
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        if (state == State::hidden)
            return;

        if (state == State::queued) {
            ImGui::OpenPopup(popup_id);
            state = State::visible;
        }

        PopupModal popup{popup_id,
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings};
        if (!popup) {
            state = State::hidden;
            return;
        }

        ImGui::Text("Are you sure you want to quit " PACKAGE_NAME "?");

        ImGui::Separator();

        ButtonHBox buttons;
        buttons.expand = true;
        buttons.add(
            ICON_FA_TIMES " Cancel",
            ImGui::CloseCurrentPopup
        );
        buttons.add(
            ICON_FA_SIGN_OUT " Quit",
            true,
            App::quit
        );
        buttons.show();
    }

} // namespace ConfirmExitPopup
