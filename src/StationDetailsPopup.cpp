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
        std::string request_uuid;
        std::optional<Station> result;
        std::string error_msg;

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
    reset()
    {
        request_uuid.clear();
        error_msg.clear();
        result.reset();
    }


    void
    open(const std::string& uuid)
    {
        TRACE_FUNC;

        if (uuid.empty()) {
            LOG_WARN("Should not be querying station details with empty UUID");
            return;
        }

        reset();
        state = State::queued;
        request_uuid = uuid;

        RadioBrowserAPI::get_station(
            request_uuid,
            [](RadioBrowserAPI::Station rb_station)
            {
                if (state == State::hidden)
                    return;
                state = State::success;
                LOG_DEBUG("received station details");
                result = Station::from_radio_browser(rb_station);
            },
            [](const std::exception& e)
            {
                if (state == State::hidden)
                    return;
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

        if (state == State::queued) {
            state = State::waiting;
            ImGui::OpenPopup(popup_id);
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
            state = State::hidden;
            reset();
            return;
        }

        switch (state) {
            case State::waiting:
                ImGui::TextAligned(0.5f, -1, "Station Details");
                ImGui::Separator();
                if (Child content{"content"})
                    ImGui::Text("Waiting for station details...");
                break;

            case State::error:
                ImGui::TextAligned(0.5f, -1, "Station Details Error");
                ImGui::Separator();
                if (Child content{"content"})
                    ImGui::TextWrapped(error_msg);
                break;

            case State::success:
                ImGui::TextAligned(0.5f, -1, "Station Details");
                ImGui::Separator();
                if (Child content{"content"}) {
                    if (Table fields{
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

                    } // fields
                }
                break;

            default:
                ;
        }
    }

} // namespace StationDetailsPopup
