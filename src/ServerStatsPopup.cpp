/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <exception>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "ServerStatsPopup.hpp"

#include "App.hpp"
#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "tracer.hpp"
#include "UI.hpp"


namespace ServerStatsPopup {

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

        const std::string popup_id = "ServerStatsPopup";


        /*-----------*/
        /* Variables */
        /*-----------*/

        State state = State::hidden;
        std::string server;
        std::optional<RadioBrowserAPI::ServerStats> stats;
        std::string error_message;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        handle_exception(const std::exception& e);

        void
        handle_success(RadioBrowserAPI::ServerStats result);

        void
        reset();

        void
        show_stats();


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        handle_exception(const std::exception& e)
        {
            TRACE_FUNC;

            if (state != State::waiting)
                return;

            state = State::error;
            error_message = e.what();
            LOG_ERROR("Server Stats: {}", error_message);
        }


        void
        handle_success(RadioBrowserAPI::ServerStats result)
        {
            TRACE_FUNC;

            if (state != State::waiting)
                return;

            state = State::success;
            stats = std::move(result);
            LOG_DEBUG("Received server stats.");
        }


        void
        reset()
        {
            stats.reset();
            error_message.clear();
            server.clear();
        }


        void
        show_stats()
        {
            using namespace ImGui::RAII;

            if (!stats)
                return;

            Font smaller{nullptr, 0.8f * App::get_default_font_size()};

            if (Table fields_table{
                    "fields",
                    2,
                    ImGuiTableFlags_None
                }) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                UI::InfoRow("server", server);
                UI::InfoRow("software_version", stats->software_version);

                UI::FormatInfoRow("stations",         "{}", stats->stations);
                UI::FormatInfoRow("stations_broken",  "{}", stats->stations_broken);
                UI::FormatInfoRow("tags",             "{}", stats->tags);
                UI::FormatInfoRow("clicks_last_hour", "{}", stats->clicks_last_hour);
                UI::FormatInfoRow("clicks_last_day",  "{}", stats->clicks_last_day);
                UI::FormatInfoRow("languages",        "{}", stats->languages);
                UI::FormatInfoRow("countries",        "{}", stats->countries);
            }
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    open()
    {
        TRACE_FUNC;

        state = State::queued;
        server = RadioBrowserAPI::get_server();
        RadioBrowserAPI::get_server_stats(handle_success, handle_exception);
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

        ImGui::SetNextWindowSize({650, 580}, ImGuiCond_Always);
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

        ImGui::TextAligned(0.5f, -1, "Server Stats");
        ImGui::Separator();

        if (Child content{"content",
                          {0, 0},
                          ImGuiChildFlags_NavFlattened}) {

            switch (state) {
                using enum State;

                case waiting:
                    ImGui::FormatTextWrapped("Waiting for {:?} to respond...",
                                             server);
                    break;

                case success:
                    show_stats();
                    break;

                case error:
                    ImGui::FormatTextWrapped("{:?} returned an error.", server);
                    ImGui::TextWrapped(error_message);
                    break;

                default:
                    ;
            }

        }
    }

} // namespace ServerStatsPopup
