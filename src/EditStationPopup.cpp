/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <regex>
#include <string>
#include <optional>
#include <utility>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "EditStationPopup.hpp"

#include "ButtonHBox.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "string_utils.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using namespace std::literals;

using string_utils::from_csv;
using string_utils::to_csv;


namespace EditStationPopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class Mode {
            create,
            edit,
        };

        enum class State {
            hidden,
            queued,
            visible,
        };


        struct Editable {
            std::string language;
            std::string tags;
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "EditStationPopup";


        /*-----------*/
        /* Variables */
        /*-----------*/

        State state = State::hidden;
        Mode mode = Mode::create;
        ConfirmFunction confirm_callback;
        std::optional<Station> station;
        std::optional<Editable> editable;
        bool busy = false;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        action_confirm();

        void
        reset();

        void
        show_field(const std::string& label,
                   std::string& value);

        void
        update_from_rb();

        void
        update_from_rb_exception(const std::exception& e);

        void
        update_from_rb_result(RadioBrowserAPI::Station rb_station);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        action_confirm()
        {
            TRACE_FUNC;

            ImGui::CloseCurrentPopup();

            // Convert CSV back to a vector
            station->language = from_csv(editable->language);
            station->tags = from_csv(editable->tags);

            if (confirm_callback)
                confirm_callback(std::move(*station));
        }


        bool
        is_valid_uuid(const std::string& uuid)
        {
            static const std::regex uuid_re{
                "[[:xdigit:]]{8}"
                "-"
                "[[:xdigit:]]{4}"
                "-"
                "[[:xdigit:]]{4}"
                "-"
                "[[:xdigit:]]{4}"
                "-"
                "[[:xdigit:]]{12}"
            };
            return std::regex_match(uuid, uuid_re);
        }


        void
        reset()
        {
            confirm_callback = {};
            station.reset();
            editable.reset();
        }


        void
        show_field(const std::string& label,
                   std::string& value)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            UI::Label(label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText("##" + label, value);
        }


        void
        update_from_rb()
        {
            busy = true;
            RadioBrowserAPI::get_station(station->stationuuid,
                                         update_from_rb_result,
                                         update_from_rb_exception);
        }


        void
        update_from_rb_exception(const std::exception& e)
        {
            busy = false;
            LOG_ERROR("Could not fetch station {}: {}",
                      station->stationuuid,
                      e.what());
        }


        void
        update_from_rb_result(RadioBrowserAPI::Station rb_station)
        {
            busy = false;
            LOG_DEBUG("Updating favorite from RadioBrowser");
            station = Station::from_radio_browser(rb_station);
            editable.emplace(
                to_csv(station->language),
                to_csv(station->tags)
            );
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    open(ConfirmFunction func)
    {
        TRACE_FUNC;

        state = State::queued;
        mode = Mode::create;
        confirm_callback = std::move(func);
        station.emplace();
        editable.emplace();
    }


    void
    open(const Station& station_,
         ConfirmFunction func)
    {
        TRACE_FUNC;

        state = State::queued;
        mode = Mode::edit;
        confirm_callback = std::move(func);
        station.emplace(station_);
        editable.emplace(
            to_csv(station->language),
            to_csv(station->tags)
        );
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

        ImGui::SetNextWindowSize({1100, 600}, ImGuiCond_Always);
        PopupModal popup{popup_id,
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings};
        if (!popup) {
            state = State::hidden;
            reset();
            return;
        }

        switch (mode) {
            case Mode::create:
                ImGui::TextAligned(0.5f, -1, "Create new station");
                break;
            case Mode::edit:
                ImGui::TextAligned(0.5f, -1, "Edit station");
                break;
        }

        ImGui::Separator();

        Disabled if_busy{busy};

        ButtonHBox buttons;
        buttons.expand = true;
        buttons.add(
            ICON_FA_TIMES " Cancel",
            ImGui::CloseCurrentPopup
        );

        if (is_valid_uuid(station->stationuuid)) {
            // TODO: perform better UUID validation
            buttons.add(ICON_FA_DOWNLOAD " Update",
                        "Fetch station details from RadioBrowser",
                        false,
                        update_from_rb);
        }

        buttons.add(
            (mode == Mode::create
             ? ICON_FA_CHECK " Create"
             : ICON_FA_CHECK " Apply"),
            true,
            action_confirm
        );

        // TODO: add button for updating from Browser, if uuid is present

        const auto& style = ImGui::GetStyle();
        if (Child content{"content",
                          {0, - (style.ItemSpacing.y + buttons.button_size.y)},
                          ImGuiChildFlags_NavFlattened,
                          ImGuiWindowFlags_NoSavedSettings}) {

            if (Table fields_table{"fields", 2}) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                show_field("name",         station->name);
                show_field("url",          station->url);
                show_field("url_resolved", station->url_resolved);
                show_field("homepage",     station->homepage);
                show_field("favicon",      station->favicon);
                show_field("tags",         editable->tags);
                show_field("countrycode",  station->countrycode);
                show_field("language",     editable->language);
                show_field("stationuuid",  station->stationuuid);
            }

            // Always scroll to the top when the window is first shown.
            if (ImGui::IsWindowAppearing())
                ImGui::SetScrollY(0);
        }

        buttons.show();
    }

} // namespace EditStationPopup
