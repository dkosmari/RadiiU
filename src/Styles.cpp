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
        switch (col) {
            case ImGuiCol_Text:
                return "Text";
            case ImGuiCol_TextDisabled:
                return "TextDisabled";
            case ImGuiCol_WindowBg:
                return "WindowBg";
            case ImGuiCol_ChildBg:
                return "ChildBg";
            case ImGuiCol_PopupBg:
                return "PopupBg";
            case ImGuiCol_Border:
                return "Border";
            case ImGuiCol_BorderShadow:
                return "BorderShadow";
            case ImGuiCol_FrameBg:
                return "FrameBg";
            case ImGuiCol_FrameBgHovered:
                return "FrameBgHovered";
            case ImGuiCol_FrameBgActive:
                return "FrameBgActive";
            case ImGuiCol_TitleBg:
                return "TitleBg";
            case ImGuiCol_TitleBgActive:
                return "TitleBgActive";
            case ImGuiCol_TitleBgCollapsed:
                return "TitleBgCollapsed";
            case ImGuiCol_MenuBarBg:
                return "MenuBarBg";
            case ImGuiCol_ScrollbarBg:
                return "ScrollbarBg";
            case ImGuiCol_ScrollbarGrab:
                return "ScrollbarGrab";
            case ImGuiCol_ScrollbarGrabHovered:
                return "ScrollbarGrabHovered";
            case ImGuiCol_ScrollbarGrabActive:
                return "ScrollbarGrabActive";
            case ImGuiCol_CheckMark:
                return "CheckMark";
            case ImGuiCol_SliderGrab:
                return "SliderGrab";
            case ImGuiCol_SliderGrabActive:
                return "SliderGrabActive";
            case ImGuiCol_Button:
                return "Button";
            case ImGuiCol_ButtonHovered:
                return "ButtonHovered";
            case ImGuiCol_ButtonActive:
                return "ButtonActive";
            case ImGuiCol_Header:
                return "Header";
            case ImGuiCol_HeaderHovered:
                return "HeaderHovered";
            case ImGuiCol_HeaderActive:
                return "HeaderActive";
            case ImGuiCol_Separator:
                return "Separator";
            case ImGuiCol_SeparatorHovered:
                return "SeparatorHovered";
            case ImGuiCol_SeparatorActive:
                return "SeparatorActive";
            case ImGuiCol_ResizeGrip:
                return "ResizeGrip";
            case ImGuiCol_ResizeGripHovered:
                return "ResizeGripHovered";
            case ImGuiCol_ResizeGripActive:
                return "ResizeGripActive";
            case ImGuiCol_InputTextCursor:
                return "InputTextCursor";
            case ImGuiCol_TabHovered:
                return "TabHovered";
            case ImGuiCol_Tab:
                return "Tab";
            case ImGuiCol_TabSelected:
                return "TabSelected";
            case ImGuiCol_TabSelectedOverline:
                return "TabSelectedOverline";
            case ImGuiCol_TabDimmed:
                return "TabDimmed";
            case ImGuiCol_TabDimmedSelected:
                return "TabDimmedSelected";
            case ImGuiCol_TabDimmedSelectedOverline:
                return "TabDimmedSelectedOverline";
            case ImGuiCol_PlotLines:
                return "PlotLines";
            case ImGuiCol_PlotLinesHovered:
                return "PlotLinesHovered";
            case ImGuiCol_PlotHistogram:
                return "PlotHistogram";
            case ImGuiCol_PlotHistogramHovered:
                return "PlotHistogramHovered";
            case ImGuiCol_TableHeaderBg:
                return "TableHeaderBg";
            case ImGuiCol_TableBorderStrong:
                return "TableBorderStrong";
            case ImGuiCol_TableBorderLight:
                return "TableBorderLight";
            case ImGuiCol_TableRowBg:
                return "TableRowBg";
            case ImGuiCol_TableRowBgAlt:
                return "TableRowBgAlt";
            case ImGuiCol_TextLink:
                return "TextLink";
            case ImGuiCol_TextSelectedBg:
                return "TextSelectedBg";
            case ImGuiCol_TreeLines:
                return "TreeLines";
            case ImGuiCol_DragDropTarget:
                return "DragDropTarget";
            case ImGuiCol_DragDropTargetBg:
                return "DragDropTargetBg";
            case ImGuiCol_UnsavedMarker:
                return "UnsavedMarker";
            case ImGuiCol_NavCursor:
                return "NavCursor";
            case ImGuiCol_NavWindowingHighlight:
                return "NavWindowingHighlight";
            case ImGuiCol_NavWindowingDimBg:
                return "NavWindowingDimBg";
            case ImGuiCol_ModalWindowDimBg:
                return "ModalWindowDimBg";
            case ImGuiCol_COUNT:
                return "COUNT";
            default:
                throw std::logic_error{"invalid ImGuiCol_"};
        }
    }


    // ImVec4
    // to_imvec4(const json::value& val)
    // {
    //     auto& c = val.as<json::array>();
    //     return ImVec4{c.at(0).to_real(),
    //                   c.at(1).to_real(),
    //                   c.at(2).to_real(),
    //                   c.at(3).to_real()};
    // }


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
                auto& j_color = j_colors.at(label).as<json::array>();
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

        style_list.emplace_back(Group::imgui, imgui_dark.name);

        for (auto& entry :
                 std::filesystem::directory_iterator{App::get_content_path() / "styles"}) {
            if (!entry.is_regular_file())
                continue;
            auto& path = entry.path();
            if (path.extension() != ".json")
                continue;

            style_list.emplace_back(Group::builtin, path.stem().string());
        }


        for (auto& entry :
                 std::filesystem::directory_iterator{App::get_config_path() / "styles"}) {
            if (!entry.is_regular_file())
                continue;
            auto& path = entry.path();
            if (path.extension() != ".json")
                continue;

            style_list.emplace_back(Group::user, path.stem().string());
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
