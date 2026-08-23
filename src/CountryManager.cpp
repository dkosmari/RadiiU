/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
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

#include "CountryManager.hpp"

#include "App.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "string_utils.hpp"
#include "tracer.hpp"


using namespace std::literals;


namespace CountryManager {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        struct ImageEntry {

            int icon_size{};
            std::filesystem::path file{};
            sdl::surface image{nullptr};

            void
            load();

        }; // struct ImageEntry


        struct FlagEntry {

            using ImageMap = std::flat_map<int, ImageEntry>;

            std::string utf8 = {};
            char32_t codepoint = 0;
            ImageMap images = {};

            ImageEntry*
            find_image(int size);

        }; // struct FlagEntry


        /*-----------*/
        /* Constants */
        /*-----------*/

        const char32_t first_codepoint = 0xe200;


        /*-----------*/
        /* Variables */
        /*-----------*/

        ImFontLoader font_loader;
        ImFontConfig font_config;

        std::unordered_map<std::string, FlagEntry> flags;
        std::unordered_map<char32_t, std::string> codepoint_to_code;
        std::unordered_map<std::string, std::string> code_to_name;
        std::unordered_map<std::string, std::string> name_to_code;

        char32_t last_codepoint;

        std::optional<std::vector<Country>> countries;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        fetch_countries();

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

        float
        get_final_font_size(const ImFontConfig* config,
                            const ImFontBaked* baked);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        fetch_countries()
        {
            TRACE_FUNC;

            // TODO: when RB errors out, it should be possible to try again
            if (countries)
                return;

            countries.emplace();

            RadioBrowserAPI::CountryParams params;
            // NOTE: RadioBrowser does not respect order by name, it orders by code.
            // params.order = RadioBrowserAPI::CountryParams::Order::name;
            params.hidebroken = true;
            params.limit = 1000;

            RadioBrowserAPI::get_countries(
                {},
                [](RadioBrowserAPI::CountryVec rb_countries)
                {
                    for (auto& [name, code, count] : rb_countries) {
                        code_to_name[code] = name;
                        name_to_code[name] = code;
                        countries->emplace_back(std::move(code),
                                                std::move(name));
                    }

                    std::ranges::sort(*countries,
                                      string_utils::less_case,
                                      &Country::name);

                    LOG_INFO("Received {} countries.", countries->size());
                },
                [](const std::exception& e)
                {
                    LOG_ERROR("Fetching countries: {}", e.what());
                }
            );
        }


        ImageEntry*
        FlagEntry::find_image(int size)
        {
            auto it = images.upper_bound(size);
            if (it != images.begin())
                --it;
            if (it == images.end()) [[unlikely]]
                return nullptr;
            return &it->second;
        }


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
        font_loader_baked_init(ImFontAtlas*,
                               ImFontConfig* config,
                               ImFontBaked* baked,
                               void*)
        {
            float font_size = get_final_font_size(config, baked);

            // Load all flags for this size.
            for (auto& [code, flag_entry] : flags) {
                auto image_entry = flag_entry.find_image(font_size);
                if (!image_entry)
                    continue;
                image_entry->load();
            }

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

                float font_size = get_final_font_size(config, baked);

                auto& flag_entry = flags.at(codepoint_to_code.at(codepoint));

                auto image_entry = flag_entry.find_image(font_size);
                if (!image_entry) [[unlikely]]
                    return false; // if somehow there are no images for this flag

                const auto& [icon_size, file, img] = *image_entry;
                if (!img) // image didn't load
                    return false;

                const float raw_padding = font_size * (1.0f / 16.f);
                const float raw_advance_x = font_size + 2 * raw_padding;
                const float advance_x = raw_advance_x / rasterizer_density;
                if (p_advance_x) {
                    *p_advance_x = advance_x;
                    return true;
                }

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


        float
        get_final_font_size(const ImFontConfig* config,
                            const ImFontBaked* baked)
        {
            float font_size = baked->Size;
            const float first_font_size = baked->OwnerFont->Sources[0]->SizePixels;
            if (config->MergeMode && config->SizePixels != 0)
                font_size *= config->SizePixels / first_font_size;
            font_size *= config->ExtraSizeScale;
            return font_size;
        }


        void
        ImageEntry::load()
        {
            if (image)
                return;
            auto full_path = App::get_content_path() / "flags" / std::to_string(icon_size) / file;
            try {
                image = sdl::img::load_png(full_path);
                // LOG_DEBUG("Loaded {}", full_path.string());
            }
            catch (std::exception& e) {
                LOG_ERROR("Loading flag {}: {}", full_path.string(), e.what());
                return;
            }
            auto target_format = sdl::pixels::format_enum::rgba_32;
            if (image.get_format_enum() != target_format)
                image = sdl::surface{std::move(image), target_format};
        }

    } // namespace


    /*-------------------*/
    /* Public functions. */
    /*-------------------*/

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
                    const int icon_size = std::stoi(size_entry.path().filename());
                    if (icon_size < 16 || icon_size > 64)
                        continue;

                    for (auto country_entry :
                             std::filesystem::directory_iterator{size_entry.path()}) {

                        if (!country_entry.is_regular_file())
                            continue;

                        std::string country_code = country_entry.path().stem();
                        auto& flag_entry = flags[country_code];
                        auto& image_entry = flag_entry.images[icon_size];
                        image_entry = {
                            icon_size,
                            country_entry.path().filename()
                        };

                        // Preload sizes 32 and 64
                        if (icon_size == 32 || icon_size == 64)
                            image_entry.load();
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
            auto& [code, entry] = item;
            entry.codepoint = idx + first_codepoint;
            last_codepoint = entry.codepoint;
            codepoint_to_code[entry.codepoint] = code;
            sdl::unique_ptr<char> str{
                SDL_iconv_string("UTF-8",
                                 "UTF-32",
                                 reinterpret_cast<const char*>(&entry.codepoint),
                                 sizeof entry.codepoint)
            };
            if (str)
                entry.utf8 = str.get();
            else
                LOG_ERROR("UTF-8 conversion for {:?} failed!", code);
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
        codepoint_to_code.clear();
    }


    char32_t
    get_codepoint(const std::string& code)
    {
        auto it = flags.find(code);
        if (it == flags.end())
            return 0;
        return it->second.codepoint;
    }


    std::string
    get_utf8(const std::string& code)
    {
        auto it = flags.find(code);
        if (it == flags.end())
            return ICON_FA_FLAG_O;
        return it->second.utf8;
    }


    std::string
    get_code(const std::string& name)
    {
        if (name.empty())
            return {};

        if (!countries) {
            fetch_countries();
            return {};
        }

        auto it = name_to_code.find(name);
        if (it == name_to_code.end())
            return {};
        return it->second;
    }


    std::string
    get_name(const std::string& code)
    {
        if (code.empty())
            return {};

        if (!countries)
            fetch_countries();

        auto it = code_to_name.find(code);
        if (it == code_to_name.end())
            return {};
        return it->second;
    }


    void
    for_each_country(const CountryFunction& func)
    {
        if (!countries)
            fetch_countries();

        for (const auto& c : *countries)
            func(c);
    }

} // namespace CountryManager
