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


using std::cout;
using std::endl;

namespace Settings {

    void
    process_ui()
    {
        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::BeginChild("settings")) {

            if (ImGui::BeginCombo("Preferred server",
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

            unsigned player_buffer_size_kb = cfg::player_buffer_size / 1024;
            if (ImGui::Slider("Player buffer (KiB)",
                              player_buffer_size_kb,
                              2u,
                              64u,
                              nullptr,
                              ImGuiSliderFlags_Logarithmic)) {
                cfg::player_buffer_size = player_buffer_size_kb * 1024;
            }

            ImGui::Checkbox("Disable Auto Power-Down", &cfg::disable_auto_power_down);

            if (ImGui::BeginCombo("Start tab", to_ui_string(cfg::start_tab))) {
                for (unsigned i = 0; i <= static_cast<unsigned>(TabIndex::last); ++i) {
                    TabIndex idx{i};
                    if (ImGui::Selectable(to_ui_string(idx), cfg::start_tab == idx))
                        cfg::start_tab = idx;
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Remember last tab", &cfg::remember_last_tab);

            ImGui::Slider("Browser page size",
                          cfg::browser_page_size,
                          10u, 50u);
            if (ImGui::IsItemDeactivatedAfterEdit())
                Browser::queue_update_stations();

            ImGui::Drag("Max recent stations",
                        cfg::max_recent,
                        0u,
                        50u,
                        0.1f);


        } // settings
        ImGui::HandleDragScroll();
        ImGui::EndChild();
    }

} // namespace Settings
