/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UI_HPP
#define UI_HPP

#include <concepts>
#include <cstdarg>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h> // ImVec4

#include <sdl2xx/color.hpp>
#include <sdl2xx/texture.hpp>

#include "csv_strings.hpp"


struct Station;


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
                  const csv_strings& values);


    // Align to the right, with label color
    void
    show_label(const char* fmt,
               ...)
        IM_FMTARGS(1);

    void
    show_label(const std::string& label);


    bool
    show_link(const std::string& label);


    void
    show_link_row(const std::string& label,
                  const std::string& url);


    void
    show_play_button(std::shared_ptr<Station>& station);


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


    struct TextSpec {
        float halign = 0;
        float width = 0;
        float wrap = -1;
        std::optional<ImVec4> color = {};
    };


    void
    show_text(const TextSpec& spec,
              const char* fmt,
              ...)
        IM_FMTARGS(2);

    void
    show_text(const TextSpec& spec,
              const std::string& text);

    void
    show_textv(const TextSpec& spec,
               const char* fmt,
               std::va_list args)
        IM_FMTLIST(2);


    void
    show_text_centered(const char* fmt,
                  ...)
        IM_FMTARGS(1);

    void
    show_text_centered(const std::string& text);


    void
    show_text_right(const char* fmt,
               ...)
        IM_FMTARGS(1);


    void
    show_text_right(const std::string& text);

} // namespace UI

#endif
