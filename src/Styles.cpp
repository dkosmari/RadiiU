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
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>

#include <imgui.h>

#include "Styles.hpp"

#include "App.hpp"
#include "cfg.hpp"
#include "json.hpp"
#include "tracer.hpp"


using std::cout;
using std::endl;
using namespace std::literals;


namespace Styles {

    struct Style {

        std::string name;
        std::array<ImVec4, ImGuiCol_COUNT> colors;


        constexpr
        Style()
            noexcept = default;

        Style(const json::value& val)
        {
            auto& obj = val.as<json::object>();

            name = obj.at("name").as<json::string>();

            auto& j_colors = obj.at("colors").as<json::array>();
            for (std::size_t i = 0; i < colors.size(); ++i) {
                if (i >= j_colors.size())
                    break;
                auto& color = colors[i];
                auto& j_color = j_colors[i].as<json::array>();
                if (j_color.size() == 3) {
                    color.x = j_color[0].to_real();
                    color.y = j_color[1].to_real();
                    color.z = j_color[2].to_real();
                    color.w = 1.0f;
                } else if (j_color.size() == 4) {
                    color.x = j_color[0].to_real();
                    color.y = j_color[1].to_real();
                    color.z = j_color[2].to_real();
                    color.w = j_color[3].to_real();
                } else
                    throw std::runtime_error{"invalid color array size"};
            }

        }


        json::value
        to_json()
            const
        {
            json::object result;

            result["name"] = name;

            json::array j_colors;
            for (std::size_t i = 0; i < colors.size(); ++i) {
                j_colors.push_back(json::array{
                        colors[i].x,
                        colors[i].y,
                        colors[i].z,
                        colors[i].w,
                    });
            }
            result["colors"] = std::move(j_colors);

            return result;
        }


        static
        Style
        from_imgui(const std::string& name)
        {
            Style result;
            result.name = name;

            ImVec4* im_colors = ImGui::GetStyle().Colors;
            for (unsigned i = 0; i < ImGuiCol_COUNT; ++i)
                result.colors[i] = im_colors[i];

            return result;
        }


        void
        apply()
            const noexcept
        {
            ImVec4* im_colors = ImGui::GetStyle().Colors;
            for (unsigned i = 0; i < ImGuiCol_COUNT; ++i)
                im_colors[i] = colors[i];
        }

    }; // struct Style


    Style imgui_dark;

    // Cache all style names for rendering.
    std::vector<Info> style_list;

    std::optional<Style> current_style;


    void
    find_styles();


    void
    initialize()
    {
        TRACE_FUNC;

        ImGui::StyleColorsDark();
        imgui_dark = Style::from_imgui("ImGui Dark");

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


    std::string
    to_string(Type t)
    {
        switch (t) {
            case Type::imgui:
                return "ImGui";
            case Type::builtin:
                return "built-in";
            case Type::user:
                return "user";
            default:
                return "ERROR";
        }
    }


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

        style_list.emplace_back(Type::imgui, imgui_dark.name);

        for (auto& entry :
                 std::filesystem::directory_iterator{App::get_content_path() / "styles"}) {
            if (!entry.is_regular_file())
                continue;
            auto& path = entry.path();
            if (path.extension() != ".json")
                continue;

            style_list.emplace_back(Type::builtin, path.stem().string());
        }


        for (auto& entry :
                 std::filesystem::directory_iterator{App::get_config_path() / "styles"}) {
            if (!entry.is_regular_file())
                continue;
            auto& path = entry.path();
            if (path.extension() != ".json")
                continue;

            style_list.emplace_back(Type::user, path.stem().string());
        }

        std::ranges::sort(style_list);

        cout << "Found " << style_list.size() << " styles" << endl;

    }
    catch (std::exception& e) {
        cout << "ERROR: Styles::find_styles(): " << e.what() << endl;
    }


    bool
    load_file(const std::filesystem::path& filename)
    {
        if (!exists(filename))
            return false;
        auto root = json::load(filename);
        current_style.emplace(root);
        current_style->apply();
        return true;
    }


    void
    load()
    try {

        if (cfg::style.empty() || cfg::style == imgui_dark.name) {
            cout << "Loading ImGui Dark style" << endl;
            imgui_dark.apply();
            return;
        }

        const std::string filename = cfg::style + ".json";
        if (load_file(App::get_content_path() / "styles" / filename))
            return;

        load_file(App::get_config_path() / "styles" / filename);
    }
    catch (std::exception& e) {
        cout << "ERROR: Styles::load(): " << e.what() << endl;
    }

} // namespace Styles
