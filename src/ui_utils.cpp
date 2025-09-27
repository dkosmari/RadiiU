/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sdl2xx/vec2.hpp>

#include <imgui_internal.h>

#include "ui_utils.hpp"

#include "IconManager.hpp"
#include "imgui_extras.hpp"


namespace ui_utils {

    void
    show_favicon(const std::string& favicon)
    {
        if (favicon.empty())
            return;

        auto icon = IconManager::get(favicon);
        auto icon_size = icon->get_size();
        sdl::vec2 size = {128, 128};
        size.x = icon_size.x * size.y / icon_size.y;
        ImGui::Image(*IconManager::get(favicon), size);
    }


    void
    show_tags(const std::vector<std::string>& tags,
              ImGuiID scroll_target)
    {
        if (tags.empty())
            return;

        bool first_tag = true;
        const ImGuiStyle& style = ImGui::GetStyle();
        for (const auto& tag : tags) {
            ImGui::PushID(&tag);

            if (!first_tag)
                ImGui::SameLine();
            first_tag = false;

            std::string label = "🏷 " + tag;
            float width = ImGui::CalcTextSize(label).x
                + style.ItemSpacing.x
                + 2 * style.FramePadding.x
                + 2 * style.FrameBorderSize;
            float available = ImGui::GetContentRegionAvail().x;
            if (width > available)
                ImGui::NewLine();

            if (ImGui::BeginChild("tag",
                                  {0,0},
                                  ImGuiChildFlags_AutoResizeX |
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_FrameStyle)) {
                ImGui::Text("%s", label.data());
            } // tag
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild();
            ImGui::PopID();
        }
    }

} // namespace ui_utils
