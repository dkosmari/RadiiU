/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UI_HPP
#define UI_HPP

#include <concepts>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h> // ImVec4

#include <sdl2xx/color.hpp>
#include <sdl2xx/texture.hpp>

#include "Station.hpp"


namespace UI {

    const ImVec4&
    get_label_color()
        noexcept;


    void
    show_favicon(const Station& station);


    void
    show_favorite_button(const Station& station);


    void
    show_image(const sdl::texture& texture,
               const sdl::vec2& size,
               const sdl::vec2f& uv0 = {0, 0},
               const sdl::vec2f& uv1 = {1, 1});

    void
    show_image(const sdl::texture& texture,
               const sdl::vec2f& uv0 = {0, 0},
               const sdl::vec2f& uv1 = {1, 1});


    bool
    show_image_button(const char* str_id,
                      const sdl::texture& texture,
                      const sdl::vec2& size,
                      const sdl::vec2f& uv0 = {0, 0},
                      const sdl::vec2f& uv1 = {1, 1},
                      sdl::color bg_color = sdl::color::transparent,
                      sdl::color tint_color = sdl::color::white);

    bool
    show_image_button(const char* str_id,
                      const sdl::texture& texture,
                      const sdl::vec2f& uv0 = {0, 0},
                      const sdl::vec2f& uv1 = {1, 1},
                      sdl::color bg_color = sdl::color::transparent,
                      sdl::color tint_color = sdl::color::white);

    void
    show_image_centered(const sdl::texture& texture,
                        const sdl::vec2& size,
                        const sdl::vec2& uv0 = {0, 0},
                        const sdl::vec2& uv1 = {1, 1});

    void
    show_image_centered(const sdl::texture& texture,
                        const sdl::vec2f& uv0 = {0, 0},
                        const sdl::vec2f& uv1 = {1, 1});


    void
    show_info_row(const std::string& label,
                  const std::string& value);


    template<std::integral T>
    void
    show_info_row(const std::string& label,
                  T value);


    void
    show_info_row(const std::string& label,
                  const std::vector<std::string>& values);


    void
    show_link_row(const std::string& label,
                  const std::string& url);


    void
    show_play_button(StationPtr& station);


    void
    show_station_basic_info(const Station& station);


    void
    show_tags(const std::vector<std::string>& tags);


    void
    show_boxed(const std::string& text,
               const std::string& tooltip);

    void
    show_boxed(const std::string& text);


    void
    show_last_bounding_box();


    // ImGui-like functions


    // Align to the right, with label color
    void
    Label(std::string_view label);


    template<typename... Args>
    void
    FormatLabel(std::format_string<Args...> fmt,
                Args&&... args)
    {
        Label(std::format(std::move(fmt), std::forward<Args>(args)...));
    }


    bool
    TextLinkOpenURL(const std::string& url);

} // namespace UI

#endif
