/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <cmath>
#include <deque>
#include <optional>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "LogsTab.hpp"

#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "tracer.hpp"


// Define this to inject log messages.
// #define DEBUG_MESSAGE_COLORS


using namespace std::literals;

// TODO: allow user to use custom font from SD:/wiiu/fonts/


namespace LogsTab {

    using LogManager::Level;

    namespace {

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::array log_level_colors = {
            ImVec4{0.75f, 0.75f, 0.5f, 1.0f}, // DEBUG
            ImVec4{0.5f, 0.5f, 1.0f, 1.0f},   // INFO
            ImVec4{1.0f, 1.0f, 0.5f, 1.0f},   // WARN
            ImVec4{1.0f, 0.5f, 0.5f, 1.0f}    // ERROR
        };

        const ImVec4 log_bg_color{0.0f, 0.0f, 0.0f, 1.0f};
        const ImVec4 log_location_color{0.5f, 1.0f, 0.5f, 1.0f};
        const ImVec4 log_text_color{1.0f, 1.0f, 1.0f, 1.0f};

        const std::array levels = {
            Level::debug,
            Level::info,
            Level::warning,
            Level::error
        };

        const float log_font_size = 20;


        /*-----------*/
        /* Variables */
        /*-----------*/

        LogManager::Timestamp timestamp;

        Level min_level;

        std::optional<float> level_label_width;

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize()
    {
        TRACE_FUNC;
        timestamp = LogManager::get_timestamp();
        min_level = Level::info;
        level_label_width.reset();
    }


    void
    finalize()
    {
        TRACE_FUNC;
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        // Toolbar

        if (ImGui::Button("Clear"))
            LogManager::clear();

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_DOWNLOAD " Save"))
            LogManager::save();

        ImGui::SameLine();

        auto& style = ImGui::GetStyle();
        float combo_width =
            ImGui::CalcTextSize("DEBUG").x +
            ImGui::GetFrameHeight() +
            2 * style.FramePadding.x;
        ImGui::SetNextItemWidth(combo_width);
        if (Combo min_level_combo{"##min_level_combo", to_string(min_level)}) {
            for (auto level : levels)
                if (ImGui::Selectable(to_string(level), min_level == level)) {
                    min_level = level;
                    timestamp = 0;
                }
        }

#ifdef DEBUG_MESSAGE_COLORS
        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_PLUS " debug"))
            LOG_DEBUG("This is an injected debug message.");

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_PLUS " info"))
            LOG_INFO("This is an injected info message.");

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_PLUS " warn"))
            LOG_WARN("This is an injected warning message.");

        ImGui::SameLine();

        if (ImGui::Button(ICON_FA_PLUS " error"))
            LOG_ERROR("This is an injected error message.");
#endif

        // Text box
        StyleColor bg_color{ImGuiCol_ChildBg, log_bg_color};
        if (Child text_box{"text_box",
                           {0, 0},
                           ImGuiChildFlags_Borders}) {

            Font font{nullptr, log_font_size};

            if (!level_label_width) [[unlikely]] {
                float width = 0;
                for (auto level : levels)
                    width = std::fmax(width,
                                      ImGui::CalcTextSize(to_string(level)).x);
                level_label_width = width;
            }

            LogManager::for_each(
                min_level,
                [](const LogManager::Message& msg)
                {
                    {
                        unsigned idx = static_cast<unsigned>(msg.level);
                        StyleColor text_color{ImGuiCol_Text, log_level_colors.at(idx)};
                        ImGui::TextAligned(0.0f, *level_label_width, to_string(msg.level));
                    }

                    ImGui::SameLine();

                    {
                        StyleColor text_color{ImGuiCol_Text, log_location_color};
                        ImGui::FormatText("[{}:{}]",
                                          msg.location.file_name(),
                                          msg.location.line());
                    }
                    // NOTE: use theme text color for the popup.
                    if (ItemTooltip function_tooltip{}) {
                        TextWrapPos wrap_at{900};
                        ImGui::TextUnformatted(msg.location.function_name());
                    }

                    {
                        Indent one;
                        StyleColor text_color{ImGuiCol_Text, log_text_color};
                        ImGui::TextWrapped(msg.text);
                    }
                }
            );

            auto new_timestamp = LogManager::get_timestamp();
            if (new_timestamp != timestamp) {
                // Scroll to the bottom.
                ImGui::SetScrollHereY(1.0f);
                timestamp = new_timestamp;
            }

        }
    }

} // namespace LogsTab
