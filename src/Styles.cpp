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


    ImVec4
    to_imvec4(const json::value& val)
    {
        auto& c = val.as<json::array>();
        if (c.size() == 3) {
            return ImVec4(c.at(0).to_real(),
                          c.at(1).to_real(),
                          c.at(2).to_real(),
                          1.0f);
        } else if (c.size() == 4) {
            return ImVec4(c.at(0).to_real(),
                          c.at(1).to_real(),
                          c.at(2).to_real(),
                          c.at(3).to_real());
        } else
            throw std::runtime_error{"invalid color array size"};
    }


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

            auto& j_colors = obj.at("colors").as<json::object>();
            for (std::size_t i = 0; i < colors.size(); ++i) {
                auto& color = colors[i];
                auto label = to_string(static_cast<ImGuiCol_>(i));
                if (!j_colors.contains(label)) {
                    cout << "Warning: missing color for " << label << endl;
                    continue;
                }
                color = to_imvec4(j_colors.at(label));
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
        imgui_styles[1] = Style::from_imgui("ImGui Light");

        ImGui::StyleColorsClassic();
        imgui_styles[2] = Style::from_imgui("ImGui Classic");

        ImGui::StyleColorsDark();
        imgui_styles[0] = Style::from_imgui("ImGui Dark");

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

        if (cfg::style.empty()) {
            cout << "Loading " << imgui_styles.front().name << " style" << endl;
            imgui_styles.front().apply();
            return;
        }

        for (auto& st : imgui_styles) {
            if (st.name == cfg::style) {
                cout << "Loading " << st.name << " style" << endl;
                st.apply();
                return;
            }
        }

        const std::string filename = cfg::style + ".json";

        // Prioritize user theme, in case of name clash.
        if (load_file(App::get_config_path() / "styles" / filename))
            return;

        load_file(App::get_content_path() / "styles" / filename);
    }
    catch (std::exception& e) {
        cout << "ERROR: Styles::load(): " << e.what() << endl;
    }

} // namespace Styles
