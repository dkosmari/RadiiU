/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <exception>
#include <filesystem>
#include <flat_map>
#include <ranges>
#include <unordered_map>
#include <vector>

#include <SDL_stdinc.h>

#include <sdl2xx/img.hpp>
#include <sdl2xx/surface.hpp>
#include <sdl2xx/unique_ptr.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include "CountryFlagManager.hpp"

#include "App.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "tracer.hpp"


using namespace std::literals;


namespace CountryFlagManager {

    namespace {

        // Types

        struct FlagEntry {
            std::string utf8 = {};
            char32_t codepoint = 0;
            std::flat_map<int, std::filesystem::path> files = {};
        };


        // Constants

        const char32_t first_codepoint = 0xe200;


        // Variables

        ImFontLoader font_loader;
        ImFontConfig font_config;

        std::unordered_map<std::string, FlagEntry> flags;
        std::unordered_map<char32_t, std::string> codepoint_to_iso_code;

        char32_t last_codepoint;


        // Function declarations

        bool
        font_loader_src_init(ImFontAtlas*,
                             ImFontConfig*);

        void
        font_loader_src_destroy(ImFontAtlas*,
                                ImFontConfig*);

        bool
        font_loader_src_contains_glyph(ImFontAtlas*,
                                       ImFontConfig*,
                                       ImWchar codepoint);

        bool
        font_loader_baked_init(ImFontAtlas* atlas,
                               ImFontConfig* config,
                               ImFontBaked* baked,
                               void*);

        void
        font_loader_baked_destroy(ImFontAtlas*,
                                  ImFontConfig*,
                                  ImFontBaked*,
                                  void*);

        bool
        font_loader_baked_load_glyph(ImFontAtlas* atlas,
                                     ImFontConfig* config,
                                     ImFontBaked* baked,
                                     void*,
                                     ImWchar codepoint,
                                     ImFontGlyph* glyph,
                                     float* p_advance_x);


        // Function definitions

        bool
        font_loader_src_init(ImFontAtlas*,
                             ImFontConfig*)
        {
            TRACE_FUNC;
            return true;
        }


        void
        font_loader_src_destroy(ImFontAtlas*,
                                ImFontConfig*)
        {
            TRACE_FUNC;
        }


        bool
        font_loader_src_contains_glyph(ImFontAtlas*,
                                       ImFontConfig*,
                                       ImWchar codepoint)
        {
            return codepoint >= first_codepoint && codepoint <= last_codepoint;
        }


        bool
        font_loader_baked_init([[maybe_unused]] ImFontAtlas* atlas,
                               [[maybe_unused]] ImFontConfig* config,
                               [[maybe_unused]] ImFontBaked* baked,
                               void*)
        {
            // TRACE_FUNC;
            // LOG_DEBUG("baked_init(): atlas={}, config={:?}, baked={}",
            //           (void*)atlas,
            //           config->Name,
            //           (void*)baked);
            return true;
        }


        void
        font_loader_baked_destroy(ImFontAtlas*,
                                  ImFontConfig*,
                                  ImFontBaked*,
                                  void*)
        {
            // TRACE_FUNC;
        }


        bool
        font_loader_baked_load_glyph(ImFontAtlas* atlas,
                                     ImFontConfig* config,
                                     ImFontBaked* baked,
                                     void*,
                                     ImWchar codepoint,
                                     ImFontGlyph* glyph,
                                     float* p_advance_x)
        {
            // Early out: if not a valid codepoint
            if (!font_loader_src_contains_glyph(atlas, config, codepoint))
                return false;

            try {
                const float rasterizer_density =
                    config->RasterizerDensity * baked->RasterizerDensity;

                // LOG_DEBUG("rasterizer_density: {}", rasterizer_density);

                float font_size = baked->Size;
                const float first_font_size = baked->OwnerFont->Sources[0]->SizePixels;
                if (config->MergeMode && config->SizePixels != 0)
                    font_size *= config->SizePixels / first_font_size;
                font_size *= config->ExtraSizeScale;

                auto& entry = flags.at(codepoint_to_iso_code.at(codepoint));

                auto it = entry.files.upper_bound(static_cast<int>(font_size));
                if (it != entry.files.begin())
                    --it;
                if (it == entry.files.end()) [[unlikely]]
                    return false; // if somehow there are no images for this flag

                const auto& [icon_size, filename] = *it;

                const float raw_padding = font_size * (1.0f / 16.f);
                const float raw_advance_x = font_size + 2 * raw_padding;
                const float advance_x = raw_advance_x / rasterizer_density;
                if (p_advance_x) {
                    *p_advance_x = advance_x;
                    return true;
                }

                sdl::surface img = sdl::img::load_png(
                    App::get_content_path() / "flags" / std::to_string(icon_size) / filename
                );
                // Convert to the correct format if necessary.
                auto target_format = sdl::pixels::format_enum::rgba_32;
                if (img.get_format_enum() != target_format)
                    img = sdl::surface{std::move(img), target_format};

                const auto [w, h] = img.get_size();
                const int stride = img.get_pitch();

                glyph->Colored = 1;
                glyph->Visible = 1;
                glyph->Codepoint = codepoint;
                glyph->AdvanceX = advance_x;

                glyph->X0 = raw_padding / rasterizer_density;
                glyph->Y0 = 0;
                glyph->X1 = (font_size + raw_padding) / rasterizer_density;
                glyph->Y1 = font_size / rasterizer_density;

                ImFontAtlasRectId pack_id = ImFontAtlasPackAddRect(atlas, w, h);
                ImTextureRect* rect = ImFontAtlasPackGetRect(atlas, pack_id);

                glyph->PackId = pack_id;

                const auto pixels = img.get_pixels_as<const unsigned char>();

                ImFontAtlasBakedSetFontGlyphBitmap(atlas,
                                                   baked,
                                                   config,
                                                   glyph,
                                                   rect,
                                                   pixels,
                                                   ImTextureFormat_RGBA32,
                                                   stride);

                return true;

            }
            catch (std::exception& e) {
                LOG_ERROR("Failed to bake glyph: {}", e.what());
                return false;
            }
        }

    } // namespace


    void
    initialize()
    {
        TRACE_FUNC;

        try {
            auto flags_root = App::get_content_path() / "flags";
            for (auto& size_entry : std::filesystem::directory_iterator{flags_root}) {
                if (!size_entry.is_directory())
                    continue;
                try {
                    const int size = std::stoi(size_entry.path().filename());
                    if (size < 16 || size > 64)
                        continue;

                    for (auto country_entry :
                             std::filesystem::directory_iterator{size_entry.path()}) {

                        if (!country_entry.is_regular_file())
                            continue;

                        std::string country_code = country_entry.path().stem();
                        flags[country_code].files[size] = country_entry.path().filename();
                    }
                }
                catch (...) {
                }
            }
        }
        catch (std::exception& e){
            LOG_ERROR("Failed to load flags: {}", e.what());
            return;
        }

        // Fill in the custom codepoints, and the utf8 representation.
        for (auto [idx, item] : flags | std::views::enumerate) {
            auto& [iso_code, entry] = item;
            entry.codepoint = idx + first_codepoint;
            last_codepoint = entry.codepoint;
            codepoint_to_iso_code[entry.codepoint] = iso_code;
            sdl::unique_ptr<char> str{
                SDL_iconv_string("UTF-8",
                                 "UTF-32",
                                 reinterpret_cast<const char*>(&entry.codepoint),
                                 sizeof entry.codepoint)
            };
            if (str)
                entry.utf8 = str.get();
            else
                LOG_ERROR("UTF-8 conversion for {:?} failed!", iso_code);
        }


        // Set up the ImFontLoader
        font_loader.FontSrcInit          = font_loader_src_init;
        font_loader.FontSrcDestroy       = font_loader_src_destroy;
        font_loader.FontSrcContainsGlyph = font_loader_src_contains_glyph;
        font_loader.FontBakedInit        = font_loader_baked_init;
        font_loader.FontBakedDestroy     = font_loader_baked_destroy;
        font_loader.FontBakedLoadGlyph   = font_loader_baked_load_glyph;

        // Load it as a custom font.
        SDL_strlcpy(font_config.Name,
                    "Country Flags Loader",
                    sizeof font_config.Name);
        font_config.MergeMode = true;
        font_config.FontLoader = &font_loader;
        auto& io = ImGui::GetIO();
        io.Fonts->AddFont(&font_config);
    }


    void
    finalize()
    {
        TRACE_FUNC;

        flags.clear();
        codepoint_to_iso_code.clear();
    }


    char32_t
    get_codepoint(const std::string& iso_code)
    {
        auto it = flags.find(iso_code);
        if (it == flags.end())
            return 0;
        return it->second.codepoint;
    }


    std::string
    get_utf8(const std::string& iso_code)
    {
        auto it = flags.find(iso_code);
        if (it == flags.end())
            return ICON_FA_FLAG_O + " "s + iso_code;
        return it->second.utf8;
    }

} // namespace CountryFlagManager
