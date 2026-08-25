/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string>
#include <utility>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "ConfirmDeleteStationPopup.hpp"

#include "ButtonHBox.hpp"
#include "IconsFontAwesome4.h"
#include "tracer.hpp"
#include "UI.hpp"


using namespace std::literals;

namespace ConfirmDeleteStationPopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class State {
            hidden,
            queued,
            visible,
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "Confirm delete favorite station";


        /*-----------*/
        /* Variables */
        /*-----------*/

        State state;
        ConfirmFunction confirm_callback;
        ConstStationPtr station;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        action_confirm();

        void
        reset();


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/


        void
        action_confirm()
        {
            TRACE_FUNC;

            ImGui::CloseCurrentPopup();
            if (confirm_callback)
                confirm_callback();
        }


        void
        reset()
        {
            confirm_callback = {};
            station.reset();
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    open(ConstStationPtr station_,
         ConfirmFunction func)
    {
        TRACE_FUNC;

        if (!station_)
            return;

        state = State::queued;
        confirm_callback = std::move(func);
        station = std::move(station_);
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

        ImGui::SetNextWindowSize({800, 0}, ImGuiCond_Always);
        Popup popup{popup_id,
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings};
        if (!popup) {
            state = State::hidden;
            reset();
            return;
        }

        UI::Title(popup_id);

        if (Child content{"content",
                          {0, 0},
                          ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_NavFlattened}) {

            ImGui::FormatTextWrapped("Are you sure you want to delete {:?}?", station->name);

        }

        ButtonHBox buttons;
        buttons.expand = true;
        buttons.add(
            ICON_FA_TIMES " Cancel",
            ImGui::CloseCurrentPopup
        );
        buttons.add(
            ICON_FA_TRASH_O " Delete",
            true,
            action_confirm);
        buttons.show();
    }


} // namespace ConfirmDeleteStationPopup
