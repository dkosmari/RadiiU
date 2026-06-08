/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <glaze/core/meta.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include <imgui.h>

#include "Styles.hpp"

#include "App.hpp"
#include "cfg.hpp"
#include "tracer.hpp"


using std::cout;
using std::endl;
using namespace std::literals;


template<>
struct glz::meta<ImVec4> {
    using T = ImVec4;
    static constexpr
    auto value = array(&T::x, &T::y, &T::z, &T::w);
};


namespace Styles {

    std::string
    to_string(Group g)
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
    to_string(ImGuiCol_ col)
    {
        return ImGui::GetStyleColorName(col);
    }


    std::optional<unsigned>
    color_name_to_index(const std::string& name)
    {
        for (unsigned i = 0; i < ImGuiCol_COUNT; ++i)
            if (to_string(static_cast<ImGuiCol_>(i)) == name)
                return i;
        return {};
    }


    using Colors = std::unordered_map<std::string, ImVec4>;


    struct Style {

        std::string name;
        Colors colors;


        static
        Style
        from_current_style(const std::string& name)
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


        void
        apply()
            const noexcept
        {
            ImVec4* im_colors = ImGui::GetStyle().Colors;
            for (auto [key, value] : colors) {
                if (auto idx = color_name_to_index(key)) {
                    im_colors[*idx] = value;
                } else {
                    cout << "WARNING: style color for \"" << key << "\" is invalid" << endl;
                }
            }
        }

    }; // struct Style


    // dark, light, classic styles
    std::array<Style, 3> imgui_styles;


    // Cache all style names for rendering.
    std::vector<Info> style_list;

    std::optional<Style> current_style;


    void
    find_styles();


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
    process_ui()
    {}


    const std::vector<Info>&
    get_styles()
        noexcept
    {
        return style_list;
    }


    void
    find_styles()
    try {
        TRACE_FUNC;

        style_list.clear();

        for (auto& st : imgui_styles)
            style_list.emplace_back(Group::imgui, st.name);

        for (auto& entry :
                 std::filesystem::directory_iterator{App::get_content_path() / "styles"}) {
            if (!entry.is_regular_file())
                continue;
            auto& path = entry.path();
            if (path.extension() != ".json")
                continue;

            style_list.emplace_back(Group::builtin, path.stem().string());
        }


        auto user_styles_path = App::get_config_path() / "styles";
        if (exists(user_styles_path)) {
            try {
                for (auto& entry :
                         std::filesystem::directory_iterator{}) {
                    if (!entry.is_regular_file())
                        continue;
                    auto& path = entry.path();
                    if (path.extension() != ".json")
                        continue;

                    style_list.emplace_back(Group::user, path.stem().string());
                }
            }
            catch (std::exception& e) {
                cout << "ERROR: trying to list user styles: " << e.what() << endl;
            }
        }

        std::ranges::sort(style_list);

        cout << "Found " << style_list.size() << " styles" << endl;

    }
    catch (std::exception& e) {
        cout << "ERROR: Styles::find_styles(): " << e.what() << endl;
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
    load()
    try {

        if (cfg::state.style.empty()) {
            cout << "Loading " << imgui_styles.front().name << " style" << endl;
            imgui_styles.front().apply();
            return;
        }

        for (auto& st : imgui_styles) {
            if (st.name == cfg::state.style) {
                cout << "Loading " << st.name << " style" << endl;
                st.apply();
                return;
            }
        }

        const std::string filename = "styles/" + cfg::state.style + ".json";

        // Prioritize user theme, in case of name clash.
        if (load_style_file(App::get_config_path() / filename))
            return;

        load_style_file(App::get_content_path() / filename);
    }
    catch (std::exception& e) {
        cout << "ERROR: Styles::load(): " << e.what() << endl;
    }

} // namespace Styles
