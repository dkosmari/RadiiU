/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <optional>
#include <stdexcept>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "StationDetailsPopup.hpp"

#include "IconsFontAwesome4.h"
#include "RadioBrowserAPI.hpp"
#include "Station.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using std::cout;
using std::endl;


using namespace std::literals;


namespace StationDetailsPopup {

    enum class State {
        hidden,
        fetching,
        done,
        error,
    };


    bool popup_queued;
    State state = State::hidden;
    std::string request_uuid;
    const std::string popup_id = "details";
    std::optional<Station> result;
    std::string error_msg;


    bool
    show_button(const std::string& uuid)
    {
        bool result = false;
        ImGui::RAII::Disabled disable_no_uuid{uuid.empty()};
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
            cout << "WARNING: should not be querying station details with empty UUID" << endl;
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
                cout << "got station details" << endl;
                result = Station::from_radio_browser(rb_station);
                state = State::done;
            },
            [](const std::exception& e,
               const std::string& response)
            {
                error_msg = e.what();
                if (!response.empty())
                    error_msg += "\n" + response;
            });
    }


    void
    process_ui()
    {
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
        ImGui::RAII::Popup popup{popup_id,
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
                ImGui::TextWrapped("Error: %s", error_msg.data());
                break;

            case State::done:
                if (ImGui::RAII::Table fields_table{
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
