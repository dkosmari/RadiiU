/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include <sdl2xx/vec2.hpp>

#include "UI.hpp"

#include "Browser.hpp"
#include "Favorites.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
#include "Player.hpp"
#include "RadioBrowserAPI.hpp"
#include "Station.hpp"
#include "string_utils.hpp"


using std::cout;
using std::endl;

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
        if (station.favicon.empty())
            return;

        auto icon = IconManager::get(station.favicon);
        auto icon_size = icon->get_size();
        sdl::vec2 size = {128, 128};
        size.x = icon_size.x * size.y / icon_size.y;
        show_image(*IconManager::get(station.favicon), size);
        ImGui::SetItemTooltip(station.favicon);
    }


    void
    show_favorite_button(const Station& station)
    {
        if (Favorites::contains(station)) {
            if (ImGui::Button(ICON_FA_HEART)) // ♥
                Favorites::remove(station);
        } else {
            if (ImGui::Button(ICON_FA_HEART_O)) // ♡
                Favorites::add(station);
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
        show_label(label);
        // show_last_bounding_box();

        ImGui::TableNextColumn();
        ImGui::TextWrapped(value);
        // show_last_bounding_box();
    }


    void
    show_info_row(const std::string& label,
                  const csv_strings& values)
    {
        std::optional<std::string> values_str{values};
        show_info_row(label, values_str.value_or(""));
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
    show_label(const char* fmt,
               ...)
    {
        std::string text;
        std::va_list args;
        va_start(args, fmt);
        try {
            text = string_utils::cpp_vsprintf(fmt, args);
            va_end(args);
        }
        catch (...) {
            va_end(args);
            throw;
        }
        TextSpec spec{
            .halign = 1.0f,
            .color = get_label_color(),
        };
        show_text(spec, text);
    }


    void
    show_label(const std::string& label)
    {
        show_label("%s", label.data());
    }


    bool
    show_link(const std::string& label)
    {
        auto& style = ImGui::GetStyle();
        TextSpec spec{
            .wrap = 0,
            .color = style.Colors[ImGuiCol_TextLink],
        };
        show_text(spec, ICON_FA_LINK " " + label);
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) {
            cout << "clicked on link" << endl;
            return true;
        }
        return false;
    }


    void
    show_link_row(const std::string& label,
                  const std::string& url)
    {
        ImGui::TableNextRow();

        ImGui::RAII::ID label_id{label};

        ImGui::TableNextColumn();
        show_label(label);

        ImGui::TableNextColumn();
        ImGui::RAII::TextWrapPos wrapper{0.0f};
        if (show_link(url)) {
#ifdef __WIIU__
            // TODO: show QR code
#endif
        }
    }


    void
    show_play_button(std::shared_ptr<Station>& station)
    {
        const sdl::vec2 button_size = {96, 96};
        if (Player::is_playing(station)) {
            if (show_image_button("stop_button",
                                  *IconManager::get("ui/stop-button.svg"),
                                  button_size))
                Player::stop();
        } else {
            if (show_image_button("play_button",
                                  *IconManager::get("ui/play-button.svg"),
                                  button_size))
                Player::play(station);
        }
    }


    void
    show_station_basic_info(const Station& station)
    {
        if (ImGui::RAII::Child basic_info_child{
                "basic_info",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_NavFlattened
            }) {

            ImGui::TextWrapped(station.name);

            if (!station.homepage.empty()) {
                if (show_link(station.homepage)) {
#ifdef __WIIU__
                    // TODO: show QR code
#endif
                }
            }

            bool has_country = false;
            if (!station.countrycode.empty()) {
                has_country = true;
                std::string tooltip = Browser::get_country_name(station.countrycode);
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
        ImGui::RAII::ID text_id{text};

        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 size = ImGui::CalcTextSize(text)
            + 2 * style.FramePadding
            + style.FrameBorderSize * ImVec2{2, 2};
        const float spacing = style.ItemSpacing.x;
        const ImVec2 available = ImGui::GetContentRegionAvail();
        if (size.x + spacing > available.x)
            ImGui::NewLine();

        if (ImGui::RAII::Child boxed_child{
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
            auto draw_list = ImGui::GetForegroundDrawList();
            draw_list->AddRect(min, max, col);
        }
    }



    void
    show_text(const TextSpec& spec,
              const char* fmt,
              ...)
    {
        std::va_list args;
        va_start(args, fmt);
        show_textv(spec, fmt, args);
        va_end(args);
    }


    void
    show_text(const TextSpec& spec,
         const std::string& text)
    {
        show_text(spec, "%s", text.data());
    }


    void
    show_textv(const TextSpec& spec,
               const char* fmt,
               std::va_list args)
    {
        std::optional<ImGui::RAII::StyleColor> color_style;
        if (spec.color)
            color_style.emplace(ImGuiCol_Text, *spec.color);

        auto text = string_utils::cpp_vsprintf(fmt, args);

        float width = spec.width > 0
            ? spec.width
            : ImGui::GetContentRegionAvail().x;
        auto str_size = ImGui::CalcTextSize(text, true, spec.wrap);
        float space = width - str_size.x;
        if (space > 0) {
            float old_x = ImGui::GetCursorPosX();
            float space_left = spec.halign * space;
            float new_x = old_x + space_left;
            ImGui::SetCursorPosX(new_x);
        }
        ImGui::RAII::TextWrapPos wrapper{spec.wrap};
        ImGui::Text(text);
    }


    void
    show_text_centered(const char* fmt,
                       ...)
    {
        std::va_list args;
        va_start(args, fmt);
        try {
            show_textv({ .halign = 0.5f, }, fmt, args);
            va_end(args);
        }
        catch (...) {
            va_end(args);
            throw;
        }
    }


    void
    show_text_centered(const std::string& text)
    {
        show_text_centered("%s", text.data());
    }


    void
    show_text_right(const char* fmt,
                    ...)
    {
        std::va_list args;
        va_start(args, fmt);
        try {
            show_textv({ .halign = 1.0f }, fmt, args);
            va_end(args);
        }
        catch (...) {
            va_end(args);
            throw;
        }
    }


    void
    show_text_right(const std::string& text)
    {
        show_text_right("%s", text.data());
    }

} // namespace UI
