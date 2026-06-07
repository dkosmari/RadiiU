/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cctype>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <curl/curl.h>
#include <glaze/version.hpp>
#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>
#include <neaacdec.h>
#include <opus/opus_defines.h>
#include <SDL_image.h>
#include <SDL_version.h>
#include <vorbis/codec.h>

#ifdef IMGUI_ENABLE_FREETYPE
#include <freetype/freetype.h>
#endif

#include "AboutTab.hpp"

#include "App.hpp"
#include "IconsFontAwesome4.h"
#include "IconManager.hpp"
#include "string_utils.hpp"
#include "UI.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;

using namespace std::literals;


namespace AboutTab {

    namespace {

        std::string
        get_sdl_version_str()
        {
            SDL_version v;
            SDL_GetVersion(&v);
            char buf[32];
            std::snprintf(buf, sizeof buf,
                          "%u.%u.%u",
                          v.major,
                          v.minor,
                          v.patch);
            return buf;
        }


        std::string
        get_sdl_img_version_str()
        {
            const SDL_version* v = IMG_Linked_Version();
            char buf[32];
            std::snprintf(buf, sizeof buf,
                          "%u.%u.%u",
                          v->major,
                          v->minor,
                          v->patch);
            return buf;
        }


#ifdef IMGUI_ENABLE_FREETYPE
        std::string
        get_ft_version_str()
        {
            FT_Library lib;
            if (!FT_Init_FreeType(&lib)) {
                FT_Int major, minor, patch;
                FT_Library_Version(lib, &major, &minor, &patch);
                FT_Done_FreeType(lib);
                char buf[32];
                std::snprintf(buf, sizeof buf,
                              "%d.%d.%d",
                              major,
                              minor,
                              patch);
                return buf;
            }
            return "";
        }
#endif

        std::string
        replace_brand_glyphs(const std::string& input)
        {
            static const std::vector<std::tuple<std::string, std::string>> replacements = {
                { "github:", ICON_FA_GITHUB },
                // { "discord:", ICON_FA_DISCORD }
            };

            std::string result = input;

            for (const auto& [src, dst] : replacements) {
                auto pos = result.find(src);
                if (pos == std::string::npos)
                    continue;
                result = result.substr(0, pos) + dst + result.substr(pos + src.size());
            }
            return result;
        }


        struct RoleName {
            std::string role;
            std::string name;
        };

        std::vector<RoleName>
        get_credits()
        {
            using string_utils::split;
            using string_utils::trimmed;

            std::vector<RoleName> result;

            try {
                std::ifstream input{ App::get_content_path() / "CREDITS" };
                std::string line;
                while (getline(input, line)) {
                    if (!line.empty() && line.front() == '#')
                        continue;
                    line = trimmed(line);
                    if (line.empty())
                        continue;
                    auto tokens = split(line, ":", false, 2);
                    if (tokens.size() != 2) {
                        cout << "ERROR: get_credits(): wrong number of tokens ("
                             << tokens.size()
                             << "): \""
                             << line << "\""
                             << endl;
                        continue;
                    }
                    for (auto& t : tokens)
                        t = trimmed(t, ' ');

                    tokens[1] = replace_brand_glyphs(tokens[1]);
                    result.emplace_back(std::move(tokens[0]), std::move(tokens[1]));
                }
            }
            catch (std::exception& e) {
                cout << "ERROR: get_credits(): " << e.what() << endl;
            }

            return result;
        }

    } // namespace


    void
    process_ui()
    {
        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::RAII::Child about{"about"}) {

            auto radiiu_icon_tex = IconManager::get("ui/radiiu-icon.png");
            UI::show_image(*radiiu_icon_tex, sdl::vec2{128, 128});
            ImGui::SameLine();

            if (ImGui::RAII::Table app_table{"app-details", 2}) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                UI::show_link_row("Homepage", PACKAGE_URL);
                UI::show_link_row("Bugs", PACKAGE_BUGREPORT);
                UI::show_info_row("User Agent", App::get_user_agent());

            }

            ImGui::SeparatorText("Credits");
            static const auto credits = get_credits();
            if (ImGui::RAII::Table credits_table{"credits", 2}) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                for (const auto& [role, name] : credits)
                    UI::show_info_row(role, name);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                UI::show_label("Stations list");
                ImGui::TableNextColumn();
                auto rb_url = "https://www.radio-browser.info";
                if (UI::show_link(rb_url)) {
                    // TODO
                }
                // ImGui::SameLine();
                // auto rb_icon_tex = IconManager::get("https://www.radio-browser.info/favicon.ico");
                // ImGui::Image(*rb_icon_tex, sdl::vec2{64, 64});

            }

            ImGui::SeparatorText("Components");
            if (ImGui::RAII::Table componets_table{"components", 2}) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                static const std::string sdl_version = get_sdl_version_str();
                UI::show_info_row("SDL", sdl_version);

                static const std::string sdl_img_version = get_sdl_img_version_str();
                UI::show_info_row("SDL_image", sdl_img_version);
                // TODO: show versions for all image libraries

                UI::show_info_row("ImGui", IMGUI_VERSION);

#ifdef IMGUI_ENABLE_FREETYPE
                static const std::string ft_version = get_ft_version_str();
                if (!ft_version.empty())
                    UI::show_info_row("FreeType", ft_version);
#endif

                UI::show_info_row("libcurl", curl_version());

                static const std::string glaze_version =
                    std::to_string(glz::version.major) + "." +
                    std::to_string(glz::version.minor) + "." +
                    std::to_string(glz::version.patch);
                UI::show_info_row("glaze", glaze_version);

                UI::show_info_row("mpg123", MPG123_VERSION);

                UI::show_info_row("Opus", opus_get_version_string());

                UI::show_info_row("Vorbis", vorbis_version_string());

                char* faad_id = nullptr;
                NeAACDecGetVersion(&faad_id, nullptr);
                UI::show_info_row("FAAD2", faad_id);

            }
        }
    }

} // namespace AboutTab
