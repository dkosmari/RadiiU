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
#include "IconsFontAwesome4.h"
#include "ImageLoader.hpp"
#include "LogManager.hpp"
#include "string_utils.hpp"
#include "TraceFunction.hpp"
#include "tracer.hpp"
#include "UI.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using namespace std::literals;


namespace AboutTab {

    namespace {

#ifdef __WIIU__
        std::filesystem::path real_save_dir;
#endif


        std::string
        get_faad2_version()
        {
            using string_utils::concat;

            char* faad_version = nullptr;
            NeAACDecGetVersion(&faad_version, nullptr);
            return faad_version;
        }


        void
        show_faad2_info()
        {
            using namespace ImGui::RAII;

            ID faad2_id{"faad2"};

            ImGui::TableNextRow();

            ImGui::TableNextColumn();

            UI::Label("FAAD2");

            ImGui::TableNextColumn();

            static const std::string faad2_version_str = get_faad2_version();

            ImGui::Text(faad2_version_str);

            if (auto version_node = TreeNode{"features"s}) {

                auto cap = NeAACDecGetCapabilities();

                if (cap & LC_DEC_CAP)
                    ImGui::BulletText("Low Complexity (LC) decoding"s);

                if (cap & MAIN_DEC_CAP)
                    ImGui::BulletText("Main Profile decoding"s);

                if (cap & LTP_DEC_CAP)
                    ImGui::BulletText("Long Term Prediction (LTP) decoding"s);

                if (cap & LD_DEC_CAP)
                    ImGui::BulletText("Low Delay (LD) decoding"s);

                if (cap & ERROR_RESILIENCE_CAP)
                    ImGui::BulletText("Error Resilience (ER) decoding"s);

                if (cap & FIXED_POINT_CAP)
                    ImGui::BulletText("Fixed Point (FP) decoding"s);

            }
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
                std::ifstream input{ App::get_content_dir() / "CREDITS" };
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

        void
        show_curl_info()
        {
            using namespace ImGui::RAII;

            ID curl_id{"curl"};

            auto info = curl_version_info(CURLVERSION_NOW);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            UI::Label("libcurl");

            ImGui::TableNextColumn();

            ImGui::FormatText("{} ({})", info->version, info->host);

            if (auto version_node = TreeNode{"components"}) {

                if (info->ssl_version)
                    ImGui::FormatBulletText("SSL: {}", info->ssl_version);

                if (info->libz_version)
                    ImGui::FormatBulletText("libz: {}", info->libz_version);

                if (info->ares)
                    ImGui::FormatBulletText("ares: {}", info->ares);

                if (info->libidn)
                    ImGui::FormatBulletText("libidn: {}", info->libidn);

                if (info->libssh_version)
                    ImGui::FormatBulletText("libssh: {}", info->libssh_version);

                if (info->brotli_version)
                    ImGui::FormatBulletText("brotli: {}", info->brotli_version);

                if (info->nghttp2_version)
                    ImGui::FormatBulletText("nghttp2: {}", info->nghttp2_version);

                if (info->zstd_version)
                    ImGui::FormatBulletText("zstd: {}", info->zstd_version);

                if (info->hyper_version)
                    ImGui::FormatBulletText("hyper: {}", info->hyper_version);

                if (info->gsasl_version)
                    ImGui::FormatBulletText("gsasl: {}", info->gsasl_version);

            }

            if (TreeNode features_node{"features"}) {
                for (int i = 0; info->feature_names[i]; ++i)
                    ImGui::BulletText(info->feature_names[i]);
            }

            if (info->cainfo) {
                ImGui::Bullet();
                ImGui::FormatTextWrapped("cainfo: {}", info->cainfo);
            }

            if (info->capath) {
                ImGui::Bullet();
                ImGui::FormatTextWrapped("capath: {}", info->capath);
            }

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
                real_save_dir = std::filesystem::path{"SD:/"} / real_save_buf;
            RPXLoader_DeInitLibrary();

            real_save_dir /= App::get_config_dir().filename();
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

        TraceFunction tf{"AboutTab"sv};

        // NOTE: flat navigation doesn't work well on child windows that scroll.
        if (Child about{"about"}) {

            auto radiiu_icon_tex = ImageLoader::get("content:/ui/radiiu-icon.png");
            UI::Image(*radiiu_icon_tex, sdl::vec2{128, 128});
            ImGui::SameLine();

            if (Table app_table{"app-details", 2}) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                UI::LinkRow("Homepage", PACKAGE_URL);
                UI::LinkRow("Bugs", PACKAGE_BUGREPORT);
                UI::InfoRow("User Agent", App::get_user_agent());
#ifdef __WIIU__
                if (!real_save_dir.empty())
                    UI::InfoRow("Save folder", real_save_dir);
                else
                    UI::InfoRow("Save folder", App::get_config_dir());
#else
                UI::InfoRow("Config folder", App::get_config_dir());
                UI::InfoRow("Data folder", App::get_data_dir());
#endif

            }

            {
                static const auto credits = get_credits();
                ImGui::SeparatorText("Credits");
                Font smaller{nullptr, 0, 0.75f};
                if (Table credits_table{"credits", 2}) {

                    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    for (const auto& [role, name] : credits)
                        UI::InfoRow(role, name);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    UI::Label("Stations list");
                    ImGui::TableNextColumn();
                    UI::TextLinkOpenURL("https://www.radio-browser.info");
                        // ImGui::SameLine();
                    // auto rb_icon_tex = ImageLoader::get("https://www.radio-browser.info/favicon.ico");
                    // ImGui::Image(*rb_icon_tex, sdl::vec2{64, 64});

                    UI::InfoRow("Country flags", "GoSquared (http://www.gosquared.com/)");
                }
            }

            {
                ImGui::SeparatorText("Components");
                Font smaller{nullptr, 0, 0.75f};
                if (Table componets_table{"components", 2}) {

                    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    static const std::string sdl_version_str = get_sdl_version();
                    UI::InfoRow("SDL", sdl_version_str);

                    static const std::string sdl_img_version_str = get_sdl_img_version();
                    UI::InfoRow("SDL_image", sdl_img_version_str);
                    // TODO: show versions for all image libraries

                    UI::InfoRow("ImGui", IMGUI_VERSION);

#ifdef IMGUI_ENABLE_FREETYPE
                    static const std::string ft_version_str = get_ft_version();
                    if (!ft_version_str.empty())
                        UI::InfoRow("FreeType", ft_version_str);
#endif

                    show_curl_info();

                    static const std::string glaze_version_str = get_glaze_version();
                    UI::InfoRow("glaze", glaze_version_str);

                    static const std::string mpg123_version_str = MPG123_VERSION;
                    UI::InfoRow("mpg123", mpg123_version_str);

                    static const std::string opus_version_str = opus_get_version_string();
                    UI::InfoRow("Opus", opus_version_str);

                    static const std::string vorbis_version_str = vorbis_version_string();
                    UI::InfoRow("Vorbis", vorbis_version_str);

                    show_faad2_info();
                }
            }
        }
    }

} // namespace AboutTab
