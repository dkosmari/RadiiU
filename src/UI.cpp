/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include <sdl2xx/vec2.hpp>

#include "UI.hpp"

#include "App.hpp"
#include "BrowserTab.hpp"
#include "cfg.hpp"
#include "FavoritesTab.hpp"
#include "ImageLoader.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "PlayerTab.hpp"
#include "RadioBrowserAPI.hpp"
#include "Station.hpp"
#include "string_utils.hpp"
#include "TabID.hpp"


using namespace std::literals;


namespace UI {

    const ImVec4&
    get_label_color()
        noexcept
    {
        const ImVec4* colors = ImGui::GetStyle().Colors;
        return colors[ImGuiCol_SeparatorActive];
    }


    void
    show_favicon(const Station& station)
    {
        using namespace ImGui::RAII;
        if (station.favicon.empty())
            return;

        const sdl::vec2 max_size = {400, 128};
        auto icon = ImageLoader::get(station.favicon, max_size);
        show_image(*icon);
        ImGui::SetItemTooltip(station.favicon);
    }


    void
    show_favorite_button(const Station& station)
    {
        if (FavoritesTab::contains(station)) {
            if (ImGui::Button(ICON_FA_HEART)) // ♥
                FavoritesTab::remove(station);
        } else {
            if (ImGui::Button(ICON_FA_HEART_O)) // ♡
                FavoritesTab::add(station);
        }
    }


    void
    show_image(const sdl::texture& texture,
               const sdl::vec2& size,
               const sdl::vec2f& uv0,
               const sdl::vec2f& uv1)
    {
        ImGui::Image(reinterpret_cast<ImTextureID>(texture.data()),
                     ImGui::ToVec2(size),
                     ImGui::ToVec2(uv0),
                     ImGui::ToVec2(uv1));
    }


    void
    show_image(const sdl::texture& texture,
               const sdl::vec2f& uv0,
               const sdl::vec2f& uv1)
    {
        show_image(texture, texture.get_size(), uv0, uv1);
    }


    bool
    show_image_button(const char* str_id,
                const sdl::texture& texture,
                const sdl::vec2& size,
                const sdl::vec2f& uv0,
                const sdl::vec2f& uv1,
                sdl::color bg_color,
                sdl::color tint_color)
    {
        return ImGui::ImageButton(str_id,
                                  reinterpret_cast<ImTextureID>(texture.data()),
                                  ImGui::ToVec2(size),
                                  ImGui::ToVec2(uv0),
                                  ImGui::ToVec2(uv1),
                                  ImGui::ToVec4(bg_color),
                                  ImGui::ToVec4(tint_color));
    }


    bool
    show_image_button(const char* str_id,
                const sdl::texture& texture,
                const sdl::vec2f& uv0,
                const sdl::vec2f& uv1,
                sdl::color bg_color,
                sdl::color tint_color)
    {
        return show_image_button(str_id,
                           texture,
                           texture.get_size(),
                           uv0,
                           uv1,
                           bg_color,
                           tint_color);
    }


    void
    show_image_centered(const sdl::texture& texture,
                        const sdl::vec2& size,
                        const sdl::vec2f& uv0,
                        const sdl::vec2f& uv1)
    {
        auto window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(0.5f * (window_width - size.x));
        show_image(texture, size, uv0, uv1);
    }


    void
    show_image_centered(const sdl::texture& texture,
                        const sdl::vec2f& uv0,
                        const sdl::vec2f& uv1)
    {
        show_image_centered(texture, texture.get_size(), uv0, uv1);
    }


    void
    show_info_row(const std::string& label,
                  const std::string& value)
    {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        Label(label);
        // show_last_bounding_box();

        ImGui::TableNextColumn();
        ImGui::TextWrapped(value);
        // show_last_bounding_box();
    }


    void
    show_info_row(const std::string& label,
                  const std::vector<std::string>& values)
    {
        show_info_row(label, string_utils::to_csv(values));
    }


    template<std::integral T>
    void
    show_info_row(const std::string& label,
                  T value)
    {
        show_info_row(label, std::to_string(value));
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
        using namespace ImGui::RAII;

        ImGui::TableNextRow();

        ID label_id{label};

        ImGui::TableNextColumn();
        Label(label);

        ImGui::TableNextColumn();
        TextLinkOpenURL(url);
    }


    void
    show_play_button(StationPtr& station)
    {
        using namespace ImGui::RAII;

        const ImVec2 button_size = {96, 96};
        Font text{nullptr, 64};
        if (PlayerTab::is_playing(*station)) {
            // const auto& style = ImGui::GetStyle();
            StyleColor text_color{ImGuiCol_Text, get_label_color()};
            if (ImGui::Button(ICON_FA_STOP, button_size))
                PlayerTab::stop();
        } else {
            if (ImGui::Button(ICON_FA_PLAY, button_size)) {
                if (cfg::state.switch_to_player)
                    App::set_tab(TabID::player);
                PlayerTab::play(station);
                // TODO: call RadioBrowserAPI directly.
                BrowserTab::send_click(station);
            }
        }
    }


    void
    show_station_basic_info(const Station& station)
    {
        using namespace ImGui::RAII;

        if (Child basic_info_child{
                "basic_info",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_NavFlattened
            }) {

            ImGui::TextWrapped(station.name);

            if (!station.homepage.empty())
                TextLinkOpenURL(station.homepage);

            bool has_country = false;
            if (!station.countrycode.empty()) {
                has_country = true;
                std::string tooltip = BrowserTab::get_country_name(station.countrycode);
                show_boxed(ICON_FA_FLAG_O " " + station.countrycode, tooltip);
            }

            if (!station.language.empty()) {
                if (has_country)
                    ImGui::SameLine();
                for (auto& lang : station.language) {
                    show_boxed(ICON_FA_LANGUAGE " " + lang,
                               "Language spoken in this broadcast.");
                    ImGui::SameLine();
                }
                ImGui::NewLine();
            }

        } // basic_info_child
    }


    void
    show_tags(const std::vector<std::string>& tags)
    {
        if (tags.empty())
            return;

        for (const auto& tag : tags) {
            show_boxed(ICON_FA_TAG " " + tag, {});
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }


    void
    show_boxed(const std::string& text,
               const std::string& tooltip)
    {
        using namespace ImGui::RAII;

        ID text_id{text};

        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 size = ImGui::CalcTextSize(text)
            + 2 * style.FramePadding
            + 2 * style.FrameBorderSize * ImVec2{1, 1};
        const float spacing = style.ItemSpacing.x;
        const ImVec2 available = ImGui::GetContentRegionAvail();
        if (size.x + spacing > available.x)
            ImGui::NewLine();

        if (Child boxed_child{
                "boxed",
                size,
                ImGuiChildFlags_FrameStyle
            }) {

            ImGui::Text(text);
            if (!tooltip.empty())
                ImGui::SetItemTooltip(tooltip);

        }
    }


    void
    show_boxed(const std::string& text)
    {
        show_boxed(text, {});
    }


    // DEBUG
    void
    show_last_bounding_box()
    {
        {
            auto min = ImGui::GetItemRectMin();
            auto max = ImGui::GetItemRectMax();
            ImU32 col = ImGui::GetColorU32(ImVec4{1.0f, 0.0f, 0.0f, 0.5f});
            auto draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(min, max, col);
        }
    }


    void
    Label(std::string_view label)
    {
        auto available = ImGui::GetContentRegionAvail();
        auto label_size = ImGui::CalcTextSize(label);
        float offset = available.x - label_size.x;
        if (offset > 0)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        ImGui::TextColored(get_label_color(), label);
        // show_last_bounding_box();
    }


    bool
    TextLinkOpenURL(const std::string& url)
    {
        using namespace ImGui::RAII;
#if 1
        const auto& style = ImGui::GetStyle();
        const auto link_color = style.Colors[ImGuiCol_TextLink];
        float url_width = ImGui::CalcTextSize(url).x;
        {
            StyleColor text_color{ImGuiCol_Text, link_color};
            ImGui::TextUnformatted(ICON_FA_LINK);
            ImGui::SameLine();
            if (ImGui::TextAligned(0, -1, url))
                url_width = -1; // got truncated
        }

        // Draw underline
        auto draw_list = ImGui::GetWindowDrawList();
        auto min = ImGui::GetItemRectMin();
        auto max = ImGui::GetItemRectMax();
        auto baked = ImGui::GetFontBaked();
        float underline_y = max.y
            + std::floor(baked->Descent * style.FontSizeBase / baked->Size * 0.2f);
        float underline_max = url_width < 0 ? max.x : min.x + url_width;
        draw_list->AddLineH(min.x,
                            underline_max,
                            underline_y,
                            ImGui::GetColorU32(link_color),
                            2.0f);

        // Mouse hover
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        // Left-click
        if (ImGui::IsItemClicked()) {
            // TODO: show QR popup
            LOG_DEBUG("clicked on link: {}", url);
            return true;
        }

        // Right-click
        if (PopupContextItem context_menu{"context-" + url}) {
            if (ImGui::MenuItem("Copy link to clipboard"))
                ImGui::SetClipboardText(url);
        }
        return false;
#else
        TextWrapPos wrapper{0};
        return ImGui::TextLinkOpenURL(url);
#endif
    }


} // namespace UI
