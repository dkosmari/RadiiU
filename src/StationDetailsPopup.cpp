/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <optional>
#include <stdexcept>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "StationDetailsPopup.hpp"

#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "rest.hpp"
#include "Station.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using namespace std::literals;


namespace StationDetailsPopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class State {
            hidden,
            queued,
            waiting,
            success,
            error,
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "details";


        /*-----------*/
        /* Variables */
        /*-----------*/

        State state = State::hidden;
        std::string uuid;
        std::optional<Station> station;
        std::string error_message;
        std::string error_content_type;
        std::string error_response;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        handle_exception(const std::exception& e);

        void
        handle_success(RadioBrowserAPI::Station rb_station);

        void
        reset();


        /*-----------------------*/
        /* Function definitions; */
        /*-----------------------*/

        void
        handle_exception(const std::exception& e)
        {
            TRACE_FUNC;

            if (state != State::waiting)
                return;

            state = State::error;
            error_message = e.what();
            LOG_ERROR("{}", error_message);
            if (auto ee = dynamic_cast<const rest::error*>(&e)) {
                error_content_type = ee->content_type;
                error_response = ee->response;
                LOG_ERROR("Content-Type: {}", error_content_type);
                LOG_ERROR("Response:\n<response>\n{}\n</response>", error_response);
            }
        }


        void
        handle_success(RadioBrowserAPI::Station rb_station)
        {
            TRACE_FUNC;

            if (state != State::waiting)
                return;

            state = State::success;
            LOG_DEBUG("received station details");
            station = Station::from_radio_browser(rb_station);
        }


        void
        reset()
        {
            uuid.clear();
            error_message.clear();
            error_content_type.clear();
            error_response.clear();
            station.reset();
        }


        void
        show_error()
        {
            using namespace ImGui::RAII;

            ImGui::Text("ERROR!");

            Font smaller{nullptr, 24};
            ImGui::TextWrapped(error_message);
            if (!error_content_type.empty())
                ImGui::FormatText("Content-Type: {}", error_content_type);
            if (!error_response.empty())
                ImGui::FormatTextWrapped("Response:\n{}", error_response);
        }


        void
        show_success()
        {
            using namespace ImGui::RAII;

            if (!station)
                return;

            Font smaller{nullptr, 24};

            if (Table fields{"fields",
                             2,
                             ImGuiTableFlags_None}) {

                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                UI::InfoRow("name",         station->name);
                UI::LinkRow("url",          station->url);
                UI::LinkRow("url_resolved", station->url_resolved);
                UI::LinkRow("homepage",     station->homepage);
                UI::LinkRow("favicon",      station->favicon);
                UI::InfoRow("countrycode",  station->countrycode);
                UI::InfoRow("language",     station->language);
                UI::InfoRow("tags",         station->tags);
                UI::InfoRow("stationuuid",  station->stationuuid);

                UI::InfoRow("codec", station->codec.value_or(""));
                UI::FormatInfoRow("bitrate", "{}", station->bitrate.value_or(0));

                UI::FormatInfoRow("votes",      "{}", station->votes.value_or(0));
                UI::FormatInfoRow("clickcount", "{}", station->click_count.value_or(0));
                UI::FormatInfoRow("clicktrend", "{}", station->click_trend.value_or(0));
            } // fields
        }

    } // namespace


    bool
    Button(const std::string& uuid)
    {
        using namespace ImGui::RAII;

        bool result = false;
        Disabled disable_no_uuid{uuid.empty()};
        if (ImGui::Button(ICON_FA_INFO_CIRCLE, UI::get_small_button_size())) // 🛈
            result = true;
        if (!uuid.empty())
            ImGui::SetItemTooltip("Request station details from RadioBrowser.");
        return result;
    }


    void
    open(const std::string& uuid_)
    {
        TRACE_FUNC;

        if (uuid_.empty()) {
            LOG_ERROR("Should not be querying station details with empty UUID");
            return;
        }

        state = State::queued;
        uuid = uuid_;

        RadioBrowserAPI::get_station(uuid, handle_success, handle_exception);
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        if (state == State::hidden)
            return;

        if (state == State::queued) {
            state = State::waiting;
            ImGui::OpenPopup(popup_id);
        }

        ImGui::SetNextWindowSize({1100, 600}, ImGuiCond_Always);
        auto viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetWorkCenter(),
                                ImGuiCond_Always,
                                {0.5f, 0.5f});
        Popup popup{popup_id,
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings};
        if (!popup) {
            state = State::hidden;
            reset();
            return;
        }

        ImGui::TextAligned(0.5f, -1, "Station Details");
        ImGui::Separator();

        if (Child content{"content"}) {

            switch (state) {
                case State::waiting:
                    ImGui::Text("Waiting for station details...");
                    break;

                case State::error:
                    show_error();
                    break;

                case State::success:
                    show_success();
                    break;

                default:
                    ;
            }
        }

    }

} // namespace StationDetailsPopup
