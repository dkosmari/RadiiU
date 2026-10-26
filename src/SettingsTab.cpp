/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "SettingsTab.hpp"

#include "BrowserTab.hpp"
#include "enumerator.hpp"
#include "IconsFontAwesome4.h"
#include "RadioBrowserAPI.hpp"
#include "Settings.hpp"
#include "Styles.hpp"
#include "TraceFunction.hpp"
#include "UI.hpp"


using namespace std::literals;

using Settings::cfg;


namespace SettingsTab {

    void
    process_ui()
    {
        using namespace ImGui::RAII;

        TraceFunction tf{"SettingsTab"sv};

        const ImGuiStyle& style = ImGui::GetStyle();

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (Child content{"content"}) {

            if (Table settings_table{"settings", 2}) {

                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);


                /*-------*/
                /* Style */
                /*-------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("UI color style"sv,
                          "Select color style for user interface."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (Combo styles_combo{"##style", cfg.style}) {
                    Styles::for_each_style(
                        [](const Styles::Info& info)
                        {
                            const auto& [group, name] = info;
                            if (ImGui::Selectable(to_label(info),
                                                  cfg.style == name)) {
                                cfg.style = name;
                                Styles::load();
                            }
                        }
                    );
                } // styles_combo


                /*------------*/
                /* Resolution */
                /*------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Screen resolution"sv,
                          "Restart the app for the new resolution to apply."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (Combo resolution_combo{"##resolution", to_label(cfg.resolution)}) {
                    for (auto res : enumerator::enumerate<Resolution>()) {
                        if (ImGui::Selectable(to_label(res),
                                              cfg.resolution == res))
                            cfg.resolution = res;
                    }
                } // resolution


                /*-------------*/
                /* Initial tab */
                /*-------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Initial tab"sv,
                          "What tab to show when the app starts."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (Combo initial_combo{
                        "##initial_tab",
                        to_label(cfg.initial_tab)
                    }) {
                    for (auto tab : enumerator::enumerate<TabID>()) {
                        if (ImGui::Selectable(to_label(tab),
                                              cfg.initial_tab == tab))
                            cfg.initial_tab = tab;
                    }
                } // initial_combo


                /*------------------*/
                /* Preferred server */
                /*------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Preferred server"sv,
                          "What Radio Browser mirror to use."sv);

                ImGui::TableNextColumn();
                const char* refresh_label = ICON_FA_REFRESH;
                float refresh_btn_width = 2 * style.FramePadding.x
                                        + 2 * style.FrameBorderSize
                                        + ImGui::CalcTextSize(refresh_label).x;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x
                                        - style.ItemSpacing.x
                                        - refresh_btn_width);
                const std::string random_label = "(random)"s;
                if (Combo server_combo{
                        "##server"s,
                        cfg.server.empty()
                        ? random_label
                        : cfg.server
                    }) {
                    if (ImGui::Selectable(random_label, cfg.server.empty())) {
                        cfg.server.clear();
                        RadioBrowserAPI::set_server(cfg.server);
                    }
                    RadioBrowserAPI::for_each_mirror(
                        [](const std::string& server)
                        {
                            if (ImGui::Selectable(server, cfg.server == server)) {
                                cfg.server = server;
                                RadioBrowserAPI::set_server(cfg.server);
                            }
                        }
                    );
                } // server_combo

                ImGui::SameLine();

                if (ImGui::Button(refresh_label))
                    RadioBrowserAPI::update_mirrors();


                /*-------------------*/
                /* Browser page size */
                /*-------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Browser page size"sv,
                          "How many stations to show per page."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##browser_page_limit",
                              cfg.browser_page_limit,
                              10u, 50u);
                if (ImGui::IsItemDeactivatedAfterEdit())
                    BrowserTab::perform_search();


                /*-----------------------*/
                /* Recent stations limit */
                /*-----------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Recent stations limit"sv,
                          "How many stations are shown in the \"Recent\" tab."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##recent_limit",
                              cfg.recent_limit,
                              10u, 50u);


                /*------------------*/
                /* Switch to player */
                /*------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Switch to Player when playing"sv,
                          "Automatically switch to the player tab when playback starts."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##switch_to_player", cfg.switch_to_player);


                /*------------------------*/
                /* Player buffer duration */
                /*------------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Player buffer duration (s)"sv,
                          "Ensure this many seconds of audio are queued before\n"
                          "playback starts."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                static double player_buffer_duration_ms =
                    cfg.player_buffer_duration.count() / 1000.0;
                if (ImGui::Drag("##player_buffer_duration"s,
                                player_buffer_duration_ms,
                                0.01f,
                                {0.0},
                                {30.0},
                                "%.1f seconds"s)) {
                    cfg.player_buffer_duration = std::chrono::milliseconds(
                        static_cast<int>(1000 * player_buffer_duration_ms)
                    );
                }


                /*----------------------*/
                /* Player history limit */
                /*----------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Player track history limit"sv,
                          "How many tracks are shown in the Player tab."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##player_history_limit",
                              cfg.player_history_limit,
                              0u, 50u);


                /*-------------*/
                /* Disable APD */
                /*-------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Disable Auto Power-Down"sv,
                          "Prevent the system from shutting down while playing."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##disable_apd", cfg.disable_apd);


                /*---------------------*/
                /* Inactive Screen Off */
                /*---------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Turn gamepad screen off on inactivity"sv,
                          "When the gamepad screen turns off, it also stops playing sounds."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##inactive_screen_off", cfg.inactive_screen_off);


                /*----------------------*/
                /* Screen Saver Timeout */
                /*----------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Screen saver timeout"sv,
                          "Time to wait to activate the screen saver,\n"
                          "in seconds (0 = disable screen saver."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Drag("##screen_saver_timeout"s,
                            cfg.screen_saver_timeout,
                            1.0f / 8.0f,
                            {0u}, {600u});


                /*-----------*/
                /* Use SWKBD */
                /*-----------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Use SWKBD"sv,
                          "Turn this off if you don't need the software keyboard."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##use_swkbd"s, cfg.use_swkbd);


                /*-----------------------*/
                /* Send clicks and votes */
                /*-----------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Send clicks and votes"sv,
                          "Enable to send clicks and votes to radio-browser.info."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##send_clicks"s, cfg.send_clicks);


                /*-------------------*/
                /* Ignore SSL errors */
                /*-------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Ignore SSL errors"sv,
                          "Disable verification of SSL certificates."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##ignore_ssl_errors"s, cfg.ignore_ssl_errors);


                /*--------------------*/
                /* Verbose image logs */
                /*--------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Verbose image logs"sv,
                          "Show verbose logs when downloading images."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##verbose_image_logs"s, cfg.verbose_image_logs);


                /*-------------------*/
                /* Verbose REST logs */
                /*-------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Verbose REST logs"sv,
                          "Show verbose logs on REST requests."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##verbose_rest_logs"s, cfg.verbose_rest_logs);


                /*---------------------*/
                /* Verbose stream logs */
                /*---------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Verbose stream logs"sv,
                          "Show verbose logs when streaming radio data."sv);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##verbose_stream_logs"s, cfg.verbose_stream_logs);


                /*-----------------*/
                /* End of settings */
                /*-----------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Reset everything to default"sv,
                          "Restore default settings."sv);

                ImGui::TableNextColumn();

                if (ImGui::Button("Reset")) {
                    Settings::reset();
                }

                /////////////////

            } // settings_table

        } // content
    }

} // namespace SettingsTab
