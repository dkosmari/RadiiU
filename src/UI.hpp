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
#include <variant>
#include <vector>

#include <imgui.h> // ImVec4

#include <sdl2xx/color.hpp>
#include <sdl2xx/texture.hpp>

#include "Station.hpp"


namespace UI {

    extern const ImVec2 play_button_size;


    const ImVec4&
    get_label_color()
        noexcept;


    const ImVec2
    get_small_button_size();


    void
    FavIcon(const Station& station);


    void
    FavoriteButton(const Station& station);


    void
    Image(const sdl::texture& texture,
          const sdl::vec2& size,
          const sdl::vec2f& uv0 = {0, 0},
          const sdl::vec2f& uv1 = {1, 1});

    void
    Image(const sdl::texture& texture,
          const sdl::vec2f& uv0 = {0, 0},
          const sdl::vec2f& uv1 = {1, 1});


    bool
    ImageButton(const char* str_id,
                      const sdl::texture& texture,
                      const sdl::vec2& size,
                      const sdl::vec2f& uv0 = {0, 0},
                      const sdl::vec2f& uv1 = {1, 1},
                      sdl::color bg_color = sdl::color::transparent,
                      sdl::color tint_color = sdl::color::white);

    bool
    ImageButton(const char* str_id,
                      const sdl::texture& texture,
                      const sdl::vec2f& uv0 = {0, 0},
                      const sdl::vec2f& uv1 = {1, 1},
                      sdl::color bg_color = sdl::color::transparent,
                      sdl::color tint_color = sdl::color::white);

    void
    ImageCentered(const sdl::texture& texture,
                  const sdl::vec2& size,
                  const sdl::vec2& uv0 = {0, 0},
                  const sdl::vec2& uv1 = {1, 1});

    void
    ImageCentered(const sdl::texture& texture,
                  const sdl::vec2f& uv0 = {0, 0},
                  const sdl::vec2f& uv1 = {1, 1});


    void
    InfoRow(const std::string& label,
            const std::string& value);


    template<typename... Args>
    void
    FormatInfoRow(const std::string& label,
                  std::format_string<Args...> fmt,
                  Args&&... args)
    {
        InfoRow(label,
                std::format(std::move(fmt),
                            std::forward<Args>(args)...));
    }


    void
    InfoRow(const std::string& label,
            const std::vector<std::string>& values);


    void
    LinkRow(const std::string& label,
            const std::string& url);


    void
    PlayButton(StationPtr& station);


    void
    StationInfo(const Station& station,
                bool show_extra = false);


    struct FramedSpec {
        std::variant<std::monostate, int, std::string> id{};
        float width = -1;
    };

    struct FramedItem {
        std::string text;
        std::string tooltip{};
        FramedSpec spec = {};
    };


    ImVec2
    CalcFramedTextSize(std::string_view text,
                       float width = -1);

    ImVec2
    CalcFramedTextSize(const FramedItem& item);


    void
    FramedText(std::string_view text,
               std::string_view tooltip = {},
               const FramedSpec& spec = {});

    void
    FramedText(const FramedItem& item);


    bool
    FramedList(const std::vector<FramedItem>& items,
               bool only_first_line);



    void
    BoundingBox();



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
