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
            fetching,
            done,
            error,
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "details";


        /*-----------*/
        /* Variables */
        /*-----------*/

        bool popup_queued;
        State state = State::hidden;
        std::string request_uuid;
        std::optional<Station> result;
        std::string error_msg;

    } // namespace


    bool
    show_button(const std::string& uuid)
    {
        using namespace ImGui::RAII;

        bool result = false;
        Disabled disable_no_uuid{uuid.empty()};
        if (ImGui::Button(ICON_FA_INFO_CIRCLE)) // 🛈
            result = true;
        if (!uuid.empty())
            ImGui::SetItemTooltip("Request station details from RadioBrowser.");
        return result;
    }


    void
    reset()
    {
        request_uuid.clear();
        error_msg.clear();
        result.reset();
        state = State::hidden;
    }


    void
    open(const std::string& uuid)
    {
        TRACE_FUNC;

        if (popup_queued)
            return;

        if (uuid.empty()) {
            LOG_WARN("Should not be querying station details with empty UUID");
            return;
        }

        reset();
        popup_queued = true;
        request_uuid = uuid;
        state = State::fetching;

        RadioBrowserAPI::get_station(
            request_uuid,
            [](RadioBrowserAPI::Station rb_station)
            {
                LOG_DEBUG("received station details");
                result = Station::from_radio_browser(rb_station);
                state = State::done;
            },
            [](const std::exception& e)
            {
                state = State::error;
                error_msg = e.what();
                if (auto ee = dynamic_cast<const rest::error*>(&e)) {
                    if (!ee->content_type.empty())
                        error_msg += "\nContent-Type: " + ee->content_type;
                    if (!ee->response.empty())
                        error_msg += "\n" + ee->response;
                }
                LOG_ERROR("{}", error_msg);
            });
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        if (state == State::hidden)
            return;

        if (popup_queued) {
            ImGui::OpenPopup(popup_id);
            popup_queued = false;
        }

        ImGui::SetNextWindowSize({1100, 600}, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Always,
                                {0.5f, 0.5f});
        Popup popup{popup_id,
                    ImGuiWindowFlags_NoSavedSettings |
                    //ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoMove};
        if (!popup) {
            reset();
            return;
        }

        switch (state) {
            case State::fetching:
                ImGui::Text("Waiting for station details...");
                break;

            case State::error:
                ImGui::FormatTextWrapped("Error: {}", error_msg);
                break;

            case State::done:
                if (Table fields_table{
                        "fields",
                        2,
                        ImGuiTableFlags_None
                    }) {

                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    if (!result)
                        throw std::logic_error{"BUG: should not be done with no station"};

                    const Station& st = *result;
                    UI::show_info_row("name",         st.name);
                    UI::show_link_row("url",          st.url);
                    UI::show_link_row("url_resolved", st.url_resolved);
                    UI::show_link_row("homepage",     st.homepage);
                    UI::show_link_row("favicon",      st.favicon);
                    UI::show_info_row("countrycode",  st.countrycode);
                    UI::show_info_row("language",     st.language);
                    UI::show_info_row("tags",         st.tags);
                    UI::show_info_row("stationuuid",  st.stationuuid);

                    UI::show_info_row("votes",        st.votes);
                    UI::show_info_row("clickcount",   st.click_count);
                    UI::show_info_row("clicktrend",   st.click_trend);
                    UI::show_info_row("bitrate",      st.bitrate);
                    UI::show_info_row("codec",        st.codec);

                } // fields_table
                break;

            default:
                ;
        }
    }

} // namespace StationDetailsPopup
