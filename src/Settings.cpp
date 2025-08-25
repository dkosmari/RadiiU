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
        if (ImGui::BeginChild("settings")) {

            {
                unsigned player_buffer_size_kb = cfg::player_buffer_size / 1024;
                if (ImGui::Slider("Player buffer (KiB)",
                                  player_buffer_size_kb,
                                  2u,
                                  256u,
                                  nullptr,
                                  ImGuiSliderFlags_Logarithmic)) {
                    cfg::player_buffer_size = player_buffer_size_kb * 1024;
                }
            }

            ImGui::Checkbox("Disable Auto Power-Down", &cfg::disable_auto_power_down);

            ImGui::Slider("Browser page size",
                          cfg::browser_page_size,
                          10u, 50u);
            if (ImGui::IsItemDeactivatedAfterEdit())
                Browser::update_list();

        }

        ImGui::EndChild();
    }

} // namespace Settings
