/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <curl/curl.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <jansson.h>
#include <mpg123.h>
#include <opus/opus_defines.h>
#include <vorbis/codec.h>

#include <freetype/freetype.h>
#include <SDL_version.h>
#include <SDL_image.h>

#include "About.hpp"

#include "IconsFontAwesome4.h"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "ui.hpp"
#include "utils.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;


namespace About {

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


        std::string
        get_mpg_decoders()
        {
            std::string result;
            const char** mpg_dec = mpg123_decoders();
            for (std::size_t i = 0; mpg_dec[i]; ++i) {
                if (i > 0)
                    result += ", ";
                result += mpg_dec[i];
            }
            return result;
        }


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
            std::vector<RoleName> result;

            try {
                std::ifstream input{ utils::get_content_path() / "CREDITS" };
                std::string line;
                while (getline(input, line)) {
                    if (!line.empty() && line.front() == '#')
                        continue;
                    line = utils::trimmed(line);
                    if (line.empty())
                        continue;
                    auto tokens = utils::split(line, ":", false, 2);
                    if (tokens.size() != 2) {
                        cout << "ERROR: get_credits(): wrong number of tokens ("
                             << tokens.size()
                             << "): \""
                             << line << "\""
                             << endl;
                        continue;
                    }
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
        if (ImGui::BeginChild("about")) {

            auto radiiu_icon_tex = IconManager::get("ui/radiiu-icon.png");
            ImGui::Image(*radiiu_icon_tex, sdl::vec2{128, 128});
            ImGui::SameLine();

            if (ImGui::BeginTable("app-details", 2)) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                ui::show_link_row("Homepage", PACKAGE_URL);
                ui::show_link_row("Bugs", PACKAGE_BUGREPORT);
                ui::show_info_row("User Agent", utils::get_user_agent());

                ImGui::EndTable();
            }

            ImGui::SeparatorText("Credits");
            static const auto credits = get_credits();
            if (ImGui::BeginTable("credits", 2)) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                for (const auto& [role, name] : credits)
                    ui::show_info_row(role, name);

                ImGui::EndTable();
            }

            ImGui::SeparatorText("Components");
            if (ImGui::BeginTable("components", 2)) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                static const std::string sdl_version = get_sdl_version_str();
                ui::show_info_row("SDL", sdl_version);

                static const std::string sdl_img_version = get_sdl_img_version_str();
                ui::show_info_row("SDL_image", sdl_img_version);

                ui::show_info_row("ImGui", IMGUI_VERSION);

                static const std::string ft_version = get_ft_version_str();
                if (!ft_version.empty())
                    ui::show_info_row("FreeType", ft_version);

                ui::show_info_row("libcurl", curl_version());

                ui::show_info_row("JANSSON", jansson_version_str());

                static const std::string mpg_decoders = get_mpg_decoders();
                ui::show_info_row("mpg123 decoders", mpg_decoders);

                ui::show_info_row("Opus", opus_get_version_string());

                ui::show_info_row("Vorbis", vorbis_version_string());

                ImGui::EndTable();
            }
        }

        ImGui::HandleDragScroll();
        ImGui::EndChild();
    }

} // namespace About
