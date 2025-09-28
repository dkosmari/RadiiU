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
#include "imgui_extras.hpp"
#include "ui.hpp"


using std::cout;
using std::endl;

namespace Settings {

    void
    process_ui()
    {
        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::BeginChild("settings")) {

            if (ImGui::BeginTable("settings", 2)) {

                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                /********************
                 * Preferred server *
                 ********************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Preferred server");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
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

                if (ImGui::Button("🔃"))
                Browser::refresh_mirrors();

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
                    for (unsigned i = 0; i <= static_cast<unsigned>(TabIndex::last); ++i) {
                        TabIndex idx{i};
                        if (ImGui::Selectable(to_ui_string(idx), cfg::initial_tab == idx))
                            cfg::initial_tab = idx;
                    }
                    ImGui::EndCombo();
                }

                /****************
                 * Remember tab *
                 ****************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Remember tab");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Checkbox("##remember_tab", &cfg::remember_tab);

                /*********************
                 * Browser page size *
                 *********************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Browser page size");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##browser_page_size",
                              cfg::browser_page_size,
                              10u, 50u);
                if (ImGui::IsItemDeactivatedAfterEdit())
                    Browser::queue_update_stations();

                /***********************
                 * Max recent stations *
                 ***********************/

                ImGui::TableNextRow();

                ImGui::TableNextColumn();

                ImGui::AlignTextToFramePadding();
                ui::show_label("Max recent stations");

                ImGui::TableNextColumn();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::Slider("##max_recent",
                              cfg::max_recent,
                              10u, 50u);

                ImGui::EndTable();
            }

        } // settings
        ImGui::HandleDragScroll();
        ImGui::EndChild();
    }

} // namespace Settings
