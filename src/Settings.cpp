/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>

#include <imgui.h>

#include "Settings.hpp"

#include "Browser.hpp"
#include "cfg.hpp"
#include "IconsFontAwesome4.h"
#include "imgui_extras.hpp"
#include "ui.hpp"


using std::cout;
using std::endl;

namespace Settings {

    void
    process_ui()
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::BeginChild("settings")) {

            if (ImGui::BeginTable("settings", 2)) {

                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                /***************
                 * Initial tab *
                 ***************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Initial tab");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if (ImGui::BeginCombo("##initial_tab", to_ui_string(cfg::initial_tab))) {
                    for (unsigned i = 0; i < TabID::count(); ++i) {
                        TabID tab{i};
                        if (ImGui::Selectable(to_ui_string(tab),
                                              cfg::initial_tab == tab))
                            cfg::initial_tab = tab;
                    }
                    ImGui::EndCombo();
                }

                /********************
                 * Preferred server *
                 ********************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Preferred server");

                ImGui::TableNextColumn();

                const char* refresh_label = ICON_FA_REFRESH /*🔃*/;
                float refresh_width = 2 * style.FramePadding.x
                                    + 2 * style.FrameBorderSize
                                    + ImGui::CalcTextSize(refresh_label).x;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x
                                        - style.ItemSpacing.x
                                        - refresh_width);
                if (ImGui::BeginCombo("##server",
                                      cfg::server.empty()
                                      ? "(random)"
                                      : cfg::server.data())) {
                    if (ImGui::Selectable("(random)", cfg::server.empty())) {
                        cfg::server.clear();
                        Browser::connect();
                    }
                    auto mirrors = Browser::get_mirrors();
                    for (const auto& name : mirrors)
                        if (ImGui::Selectable(name, cfg::server == name)) {
                            cfg::server = name;
                            Browser::connect();
                        }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();

                if (ImGui::Button(refresh_label))
                    Browser::refresh_mirrors();

                /*********************
                 * Browser page size *
                 *********************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Browser page size");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##browser_page_limit",
                              cfg::browser_page_limit,
                              10u, 50u);
                if (ImGui::IsItemDeactivatedAfterEdit())
                    Browser::queue_refresh_stations();


                /*************************
                 * Recent stations limit *
                 *************************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Recent stations limit");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##recent_limit",
                              cfg::recent_limit,
                              10u, 50u);

                /**********************
                 * Player buffer size *
                 **********************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Player buffer (KiB)");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##player_buffer_size",
                              cfg::player_buffer_size,
                              4u, 64u,
                              nullptr, ImGuiSliderFlags_Logarithmic);

                /***********************
                 * Player history limit *
                 ***********************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Player history limit");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##player_history_limit",
                              cfg::player_history_limit,
                              0u, 50u);

                /***************
                 * Disable APD *
                 ***************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Disable Auto Power-Down");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##disable_apd", &cfg::disable_apd);

                /***********************
                 * Inactive Screen Off *
                 ***********************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Turn gamepad screen off on inactivity");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##inactive_screen_off", &cfg::inactive_screen_off);

                /*****************
                 * Disable swkbd *
                 *****************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Disable SWKBD");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##disable_swkbd", &cfg::disable_swkbd);

                /*******************
                 * End of settings *
                 *******************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ui::show_label("Reset everything to default");

                ImGui::TableNextColumn();

                if (ImGui::Button("Reset")) {
                    cfg::load_defaults();
                    cfg::save();
                }

                /////////////////
                ImGui::EndTable();
            }

        } // settings
        ImGui::HandleDragScroll();
        ImGui::EndChild();
    }

} // namespace Settings
