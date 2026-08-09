/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cctype>
#include <exception>
#include <format>
#include <fstream>
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

#ifdef __WIIU__
#include <rpxloader/rpxloader.h>
#endif

#ifdef IMGUI_ENABLE_FREETYPE
#include <freetype/freetype.h>
#endif

#include "AboutTab.hpp"

#include "App.hpp"
#include "ImageLoader.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "string_utils.hpp"
#include "tracer.hpp"
#include "UI.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using namespace std::literals;


namespace AboutTab {

    namespace {

#ifdef __WIIU__
        std::filesystem::path real_save_path;
#endif


        std::string
        get_faad2_version()
        {
            char* faad_id = nullptr;
            NeAACDecGetVersion(&faad_id, nullptr);
            return faad_id;
        }


#ifdef IMGUI_ENABLE_FREETYPE
        std::string
        get_ft_version()
        {
            FT_Library lib;
            if (!FT_Init_FreeType(&lib)) {
                FT_Int major, minor, patch;
                FT_Library_Version(lib, &major, &minor, &patch);
                FT_Done_FreeType(lib);
                return std::format("{}.{}.{}",
                                   major,
                                   minor,
                                   patch);
            }
            return "";
        }
#endif


        std::string
        get_glaze_version()
        {
            return std::format("{}.{}.{}",
                               glz::version.major,
                               glz::version.minor,
                               glz::version.patch);
        }


        std::string
        get_sdl_version()
        {
            SDL_version v;
            SDL_GetVersion(&v);
            return std::format("{}.{}.{}",
                               v.major,
                               v.minor,
                               v.patch);
        }


        std::string
        get_sdl_img_version()
        {
            const SDL_version* v = IMG_Linked_Version();
            return std::format("{}.{}.{}",
                               v->major,
                               v->minor,
                               v->patch);
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
                result.replace(pos, src.size(), dst);
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
                        LOG_ERROR("wrong number of tokens ({}): {:?}",
                                  tokens.size(),
                                  line);
                        continue;
                    }
                    for (auto& t : tokens)
                        t = trimmed(t, ' ');

                    tokens[1] = replace_brand_glyphs(tokens[1]);
                    result.emplace_back(std::move(tokens[0]), std::move(tokens[1]));
                }
            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
            }

            return result;
        }

    } // namespace


    void
    initialize()
    {
        TRACE_FUNC;

#ifdef __WIIU__
        if (RPXLoader_InitLibrary() == RPX_LOADER_RESULT_SUCCESS) {
            char real_save_buf[1024];
            if (RPXLoader_GetPathOfSaveRedirection(real_save_buf,
                                                   sizeof real_save_buf)
                == RPX_LOADER_RESULT_SUCCESS)
                real_save_path = std::filesystem::path{"SD:/"} / real_save_buf;
            RPXLoader_DeInitLibrary();
        }
#endif
    }


    void
    finalize()
    {
        TRACE_FUNC;
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (Child about{"about"}) {

            auto radiiu_icon_tex = ImageLoader::get("content:/ui/radiiu-icon.png");
            UI::Image(*radiiu_icon_tex, sdl::vec2{128, 128});
            ImGui::SameLine();

            if (Table app_table{"app-details", 2}) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                UI::show_link_row("Homepage", PACKAGE_URL);
                UI::show_link_row("Bugs", PACKAGE_BUGREPORT);
                UI::show_info_row("User Agent", App::get_user_agent());
#ifdef __WIIU__
                if (!real_save_path.empty())
                    UI::show_info_row("Save folder", real_save_path);
                else
                    UI::show_info_row("Save folder", App::get_config_path());
#else
                UI::show_info_row("Save folder", App::get_config_path());
#endif

            }

            ImGui::SeparatorText("Credits");
            static const auto credits = get_credits();
            if (Table credits_table{"credits", 2}) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                for (const auto& [role, name] : credits)
                    UI::show_info_row(role, name);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                UI::Label("Stations list");
                ImGui::TableNextColumn();
                UI::TextLinkOpenURL("https://www.radio-browser.info");
                // ImGui::SameLine();
                // auto rb_icon_tex = ImageLoader::get("https://www.radio-browser.info/favicon.ico");
                // ImGui::Image(*rb_icon_tex, sdl::vec2{64, 64});

            }

            ImGui::SeparatorText("Components");
            if (Table componets_table{"components", 2}) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                static const std::string sdl_version_str = get_sdl_version();
                UI::show_info_row("SDL", sdl_version_str);

                static const std::string sdl_img_version_str = get_sdl_img_version();
                UI::show_info_row("SDL_image", sdl_img_version_str);
                // TODO: show versions for all image libraries

                UI::show_info_row("ImGui", IMGUI_VERSION);

#ifdef IMGUI_ENABLE_FREETYPE
                static const std::string ft_version_str = get_ft_version();
                if (!ft_version_str.empty())
                    UI::show_info_row("FreeType", ft_version_str);
#endif

                static const std::string curl_version_str = curl_version();
                UI::show_info_row("libcurl", curl_version_str);

                static const std::string glaze_version_str = get_glaze_version();
                UI::show_info_row("glaze", glaze_version_str);

                static const std::string mpg123_version_str = MPG123_VERSION;
                UI::show_info_row("mpg123", mpg123_version_str);

                static const std::string opus_version_str = opus_get_version_string();
                UI::show_info_row("Opus", opus_version_str);

                static const std::string vorbis_version_str = vorbis_version_string();
                UI::show_info_row("Vorbis", vorbis_version_str);

                static const std::string faad2_version_str = get_faad2_version();
                UI::show_info_row("FAAD2", faad2_version_str);

            }
        }
    }

} // namespace AboutTab
