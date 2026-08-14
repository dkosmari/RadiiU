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
#include "UI.hpp"


using namespace std::literals;

using Settings::cfg;


namespace SettingsTab {

    void
    process_ui()
    {
        using namespace ImGui::RAII;

        const ImGuiStyle& style = ImGui::GetStyle();

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (Child settings_child{"settings"}) {

            if (Table settings_table{"settings", 2}) {

                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);


                /*-------*/
                /* Style */
                /*-------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("UI color style");
                ImGui::SetItemTooltip("Select color style for user interface");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (Combo styles_combo{"##style", cfg.style}) {
                    auto& styles = Styles::get_styles();
                    for (const auto& [type, name] : styles) {
                        std::string label = "("s + to_label(type) + ") "s + name;
                        if (ImGui::Selectable(label, cfg.style == name)) {
                            cfg.style = name;
                            Styles::load();
                        }
                    }
                } // styles_combo


                /*-------------*/
                /* Initial tab */
                /*-------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Initial tab");

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
                UI::Label("Preferred server");

                ImGui::TableNextColumn();
                const char* refresh_label = ICON_FA_REFRESH;
                float refresh_btn_width = 2 * style.FramePadding.x
                                        + 2 * style.FrameBorderSize
                                        + ImGui::CalcTextSize(refresh_label).x;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x
                                        - style.ItemSpacing.x
                                        - refresh_btn_width);
                const std::string random_label = "(random)";
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
                UI::Label("Browser page size");
                ImGui::SetItemTooltip("How many stations to show per page.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##browser_page_limit",
                              cfg.browser_page_limit,
                              10u, 50u);
                if (ImGui::IsItemDeactivatedAfterEdit())
                    BrowserTab::search_stations();


                /*-----------------------*/
                /* Recent stations limit */
                /*-----------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Recent stations limit");

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
                UI::Label("Switch to Player when playing");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##switch_to_player", cfg.switch_to_player);


                /*--------------------*/
                /* Player buffer size */
                /*--------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Player buffer size (KiB)");
                ImGui::SetItemTooltip("Playback will only start after this many bytes"
                                      " are received.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##player_buffer_size",
                              cfg.player_buffer_size,
                              4u, 64u,
                              "",
                              ImGuiSliderFlags_Logarithmic);


                /*----------------------*/
                /* Player history limit */
                /*----------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Player track history limit");

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
                UI::Label("Disable Auto Power-Down");
                ImGui::SetItemTooltip("APD is only disabled while playing.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##disable_apd", cfg.disable_apd);


                /*---------------------*/
                /* Inactive Screen Off */
                /*---------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Turn gamepad screen off on inactivity");
                ImGui::SetItemTooltip("When the gamepad screen turns off,"
                                      " it also stops playing sounds.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##inactive_screen_off", cfg.inactive_screen_off);


                /*----------------------*/
                /* Screen Saver Timeout */
                /*----------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Screen saver timeout");
                ImGui::SetItemTooltip("Time to wait to activate the screen saver, in seconds (0 = disable screen saver.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Drag("##screen_saver_timeout"s,
                            cfg.screen_saver_timeout,
                            1.0f / 8.0f,
                            {0u}, {600u});


                /*---------------*/
                /* Disable swkbd */
                /*---------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Disable SWKBD");
                ImGui::SetItemTooltip("Use only USB keyboard for text input.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##disable_swkbd", cfg.disable_swkbd);


                /*-----------------------*/
                /* Send clicks and votes */
                /*-----------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Send clicks and votes");
                ImGui::SetItemTooltip("Enable to send clicks and votes to radio-browser.info.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##send_clicks", cfg.send_clicks);


                /*--------------------*/
                /* Verbose image logs */
                /*--------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Verbose image logs");
                ImGui::SetItemTooltip("Show verbose CURL logs when downloading images.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##verbose_image_logs", cfg.verbose_image_logs);


                /*-------------------*/
                /* Verbose REST logs */
                /*-------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Verbose REST logs");
                ImGui::SetItemTooltip("Show verbose CURL logs on REST requests.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##verbose_rest_logs", cfg.verbose_rest_logs);


                /*---------------------*/
                /* Verbose stream logs */
                /*---------------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Verbose stream logs");
                ImGui::SetItemTooltip("Show verbose CURL logs when streaming radio data.");

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##verbose_stream_logs", cfg.verbose_stream_logs);


                /*-----------------*/
                /* End of settings */
                /*-----------------*/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                UI::Label("Reset everything to default");

                ImGui::TableNextColumn();

                if (ImGui::Button("Reset")) {
                    Settings::load_defaults();
                    Settings::save();
                }

                /////////////////

            } // settings_table

        } // settings_child
    }

} // namespace SettingsTab
