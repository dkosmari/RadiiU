/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <glaze/core/meta.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <sdl2xx/color.hpp>

#include "Styles.hpp"

#include "App.hpp"
#include "LogManager.hpp"
#include "Settings.hpp"
#include "string_utils.hpp"
#include "tracer.hpp"


using namespace std::literals;

using ImGui::to_string;

using Settings::cfg;


struct HexColor32 {

    ImVec4 value = {};


    static
    HexColor32
    from_string(std::string_view input)
    {
        if (input.empty())
            return {};
        if (input[0] != '#')
            throw std::runtime_error{"invalid hex color"};

        auto read_octet = [](std::string_view s) -> Uint8
        {
            Uint8 result = 0;
            auto [ptr, ec] = std::from_chars(s.data(),
                                             s.data() + s.size(),
                                             result,
                                             16);
            if (ec != std::errc{})
                throw std::system_error{std::make_error_code(ec), "failed to parse"};
            if (ptr != s.data() + s.size())
                throw std::runtime_error{"invalid character"};
            return result;
        };

        Uint8 r, g, b, a = 1;
        auto hex = input.substr(1); // skip the '#'

        switch (hex.size()) {

            case 4:
                a = read_octet(hex.substr(3, 1));
                a = (a << 4) | a; // double the bits
                [[fallthrough]];

            case 3:
                r = read_octet(hex.substr(0, 2));
                g = read_octet(hex.substr(1, 2));
                b = read_octet(hex.substr(2, 2));
                r = (r << 4) | r; // double the bits
                g = (g << 4) | g; // double the bits
                b = (b << 4) | b; // double the bits
                break;

            case 8:
                a = read_octet(hex.substr(6, 2));
                [[fallthrough]];

            case 6:
                r = read_octet(hex.substr(0, 2));
                g = read_octet(hex.substr(2, 2));
                b = read_octet(hex.substr(4, 2));
                break;

            default:
                throw std::runtime_error{"invalid hex string length"};

        }

        sdl::color c{r, g, b, a};
        auto rgba = c.to_rgba();
        HexColor32 result;
        result.value.x = rgba.r;
        result.value.y = rgba.g;
        result.value.z = rgba.b;
        result.value.w = rgba.a;
        return result;
    }

}; // struct HexColor32


std::string
to_string(const HexColor32& hc)
{
    sdl::rgba rgba{
        hc.value.x,
        hc.value.y,
        hc.value.z,
        hc.value.w
    };
    auto c = sdl::color::from_rgba(rgba);
    return std::format("#{:x}{:x}{:x}{:x}",
                       c.r,
                       c.g,
                       c.b,
                       c.a);
}


template<>
struct glz::meta<HexColor32> {
    using T = HexColor32;

    static constexpr
    auto read_fn =
        [](T& c, std::string_view input)
        {
            c = T::from_string(input);
        };

    static constexpr
    auto write_fn =
        [](const T& c) -> std::string
        {
            return to_string(c);
        };

    static constexpr
    auto value = glz::custom<read_fn, write_fn>;
};


namespace Styles {

    namespace {

        /*--------------*/
        /* Type aliases */
        /*--------------*/

        using ColorMap = std::unordered_map<std::string, HexColor32>;


        /*-------*/
        /* Types */
        /*-------*/

        struct Style {

            std::string name;
            ColorMap colors;


            void
            apply()
                const noexcept;

            static
            Style
            from_current_style(const std::string& name);

        }; // struct Style


        /*-----------*/
        /* Constants */
        /*-----------*/


        /*-----------*/
        /* Variables */
        /*-----------*/

        // dark, light, classic styles
        std::array<Style, 3> imgui_styles;

        // Cache all style names for rendering.
        std::vector<Info> style_list;

        std::optional<Style> current_style;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        std::optional<unsigned>
        color_name_to_index(const std::string& name);

        void
        find_styles();

        bool
        load_style_file(const std::filesystem::path& filename);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        std::optional<unsigned>
        color_name_to_index(const std::string& name)
        {
            for (unsigned i = 0; i < ImGuiCol_COUNT; ++i)
                if (to_string(static_cast<ImGuiCol_>(i)) == name)
                    return i;
            return {};
        }


        void
        find_styles()
        try {
            TRACE_FUNC;

            style_list.clear();

            for (auto& st : imgui_styles)
                style_list.emplace_back(Group::imgui, st.name);

            auto built_in_styles_dir = App::get_content_dir() / "styles";
            for (auto& entry : std::filesystem::directory_iterator{built_in_styles_dir}) {
                if (!entry.is_regular_file())
                    continue;
                auto& path = entry.path();
                if (path.extension() != ".json")
                    continue;

                style_list.emplace_back(Group::builtin, path.stem().string());
            }


            auto user_styles_dir = App::get_data_dir() / "styles";
            if (exists(user_styles_dir)) {
                try {
                    for (auto& entry : std::filesystem::directory_iterator{user_styles_dir}) {
                        if (!entry.is_regular_file())
                            continue;
                        auto& path = entry.path();
                        if (path.extension() != ".json")
                            continue;

                        style_list.emplace_back(Group::user, path.stem().string());
                    }
                }
                catch (std::exception& e) {
                    LOG_ERROR("Trying to list user styles: {}", e.what());
                }
            }

            std::ranges::sort(style_list);
            // std::sort(style_list.begin(), style_list.end());

            LOG_INFO("Found {} styles.", style_list.size());

        }
        catch (std::exception& e) {
            LOG_ERROR("{}", e.what());
        }


        bool
        load_style_file(const std::filesystem::path& filename)
        {
            if (!exists(filename))
                return false;
            Style st;
            glz::ex::read_file_json(st, filename.c_str(), std::string{});
            current_style.emplace(st);
            current_style->apply();
            return true;
        }


        void
        Style::apply()
            const noexcept
        {
            ImVec4* im_colors = ImGui::GetStyle().Colors;
            for (auto [key, color] : colors) {
                if (auto idx = color_name_to_index(key)) {
                    im_colors[*idx] = color.value;
                } else {
                    LOG_ERROR("Style color name {:?} is invalid.", key);
                }
            }
        }


        Style
        Style::from_current_style(const std::string& name)
        {
            Style result;
            result.name = name;

            ImVec4* im_colors = ImGui::GetStyle().Colors;
            for (unsigned i = 0; i < ImGuiCol_COUNT; ++i) {
                auto ic = static_cast<ImGuiCol_>(i);
                auto key = to_string(ic);
                auto value = im_colors[i];
                result.colors.emplace(key, value);
            }

            return result;
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/


    std::strong_ordering
    Info::operator <=>(const Info& other)
        const noexcept
    {
        auto g = group <=> other.group;
        if (g != std::strong_ordering::equal)
            return g;

        return string_utils::compare_case(name, other.name);
    }


    void
    initialize()
    {
        TRACE_FUNC;

        ImGui::StyleColorsLight();
        imgui_styles[1] = Style::from_current_style("ImGui Light");

        ImGui::StyleColorsClassic();
        imgui_styles[2] = Style::from_current_style("ImGui Classic");

        ImGui::StyleColorsDark();
        imgui_styles[0] = Style::from_current_style("ImGui Dark");

        find_styles();
        load();
    }


    void
    finalize()
    {
        TRACE_FUNC;
    }


    void
    for_each_style(const StyleInfoFunction& func)
    {
        for (const auto& info : style_list)
            func(info);
    }


    void
    load()
    try {

        if (cfg.style.empty()) {
            LOG_INFO("Applying style: {:?}", imgui_styles.front().name);
            imgui_styles.front().apply();
            return;
        }

        for (auto& st : imgui_styles) {
            if (st.name == cfg.style) {
                LOG_INFO("Applying style: {:?}", st.name);
                st.apply();
                return;
            }
        }

        const std::string filename = "styles/" + cfg.style + ".json";

        // Prioritize user theme, in case of name clash.
        if (load_style_file(App::get_data_dir() / filename))
            return;

        load_style_file(App::get_content_dir() / filename);
    }
    catch (std::exception& e) {
        LOG_ERROR("{}", e.what());
    }


    std::string
    to_label(Group g)
    {
        switch (g) {
            case Group::imgui:
                return "ImGui";
            case Group::builtin:
                return "built-in";
            case Group::user:
                return "user";
            default:
                throw std::logic_error{"invalid group"};
        }
    }

    std::string
    to_label(const Info& info)
    {
        return "("s + to_label(info.group) + ") "s + info.name;
    }

} // namespace Styles
