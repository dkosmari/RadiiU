/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdint>

#include <sdl2xx/vec2.hpp>

#include <imgui_internal.h>

#include "ui_common.hpp"

#include "constants.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "Player.hpp"
#include "Station.hpp"
#include "utils.hpp"


namespace ui_common {

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
    show_info_row(const std::string& label,
                  const std::string& value)
    {
        ImGui::TableNextRow();

        ImGui::PushID(label);

        ImGui::TableNextColumn();
        ImGui::TextRightColored(constants::label_color,  "%s", label.data());

        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", value.data());

        ImGui::PopID();
    }


    template<std::integral T>
    void
    show_info_row(const std::string& label,
                  T value)
    {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextRightColored(constants::label_color, "%s", label.data());

        ImGui::TableNextColumn();
        const std::string fmt = "%" + utils::format(value);
        ImGui::TextWrapped(fmt.data(), value);
    }

    /* ----------------------------------------------- */
    /* Explicit instantiations for show_info_row<T>() */
    /* ----------------------------------------------- */

    template
    void show_info_row<std::int8_t>(const std::string& label, std::int8_t value);
    template
    void show_info_row<std::int16_t>(const std::string& label, std::int16_t value);
    template
    void show_info_row<std::int32_t>(const std::string& label, std::int32_t value);
    template
    void show_info_row<std::int64_t>(const std::string& label, std::int64_t value);

    template
    void show_info_row<std::uint8_t>(const std::string& label, std::uint8_t value);
    template
    void show_info_row<std::uint16_t>(const std::string& label, std::uint16_t value);
    template
    void show_info_row<std::uint32_t>(const std::string& label, std::uint32_t value);
    template
    void show_info_row<std::uint64_t>(const std::string& label, std::uint64_t value);


    void
    show_link_row(const std::string& label,
                  const std::string& url)
    {
        ImGui::TableNextRow();

        ImGui::PushID(label);

        ImGui::TableNextColumn();
        ImGui::TextRightColored(constants::label_color,  "%s", label.data());

        ImGui::TableNextColumn();
        if (ImGui::TextLinkOpenURL(url, url)) {
#ifdef __WIIU__
            // TODO: show QR code
#endif
        }

        ImGui::PopID();
    }


    void
    show_play_button(const Station& station)
    {
        const sdl::vec2 button_size = {96, 96};
        if (Player::is_playing(station)) {
            if (ImGui::ImageButton("stop_button",
                                   *IconManager::get("ui/stop-button.png"),
                                   button_size))
                Player::stop();
        } else {
            if (ImGui::ImageButton("play_button",
                                   *IconManager::get("ui/play-button.png"),
                                   button_size))
                Player::play(station);
        }
    }


    void
    show_station_basic_info(const Station& station,
                            ImGuiID scroll_target)
    {
        if (ImGui::BeginChild("basic_info",
                              {0, 0},
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_NavFlattened)) {

            ImGui::TextWrapped("%s", station.name.data());

            if (!station.homepage.empty()) {
                if (ImGui::TextLinkOpenURL(station.homepage, station.homepage)) {
#ifdef __WIIU__
                    // TODO: show QR code
#endif
                }
            }

            if (!station.country_code.empty())
                ImGui::Text("🏳 %s", station.country_code.data());

        } // basic_info
        ImGui::HandleDragScroll(scroll_target);
        ImGui::EndChild();
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

} // namespace ui_common
