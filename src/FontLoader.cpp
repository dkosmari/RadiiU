/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include <imgui.h>
#ifdef IMGUI_ENABLE_FREETYPE
#include <imgui_freetype.h>
#endif

#ifdef __WIIU__
#include <coreinit/memory.h>
#else
#include <fontconfig/fontconfig.h>
#endif

#include "FontLoader.hpp"

#include "LogManager.hpp"
#include "tracer.hpp"


using namespace std::literals;


namespace FontLoader {

    namespace {

        void
        tweak_cafe(ImFontConfig& config)
        {
            LOG_DEBUG("Adjusting for Cafe font.");
            config.GlyphOffset.y = -6;
        }

        void
        tweak_font_awesome(ImFontConfig& config)
        {
            LOG_DEBUG("Adjusting for fontawesome.");
            // font_size *= 33.0f / 40.0f;
            config.GlyphOffset.y = -6;
        }

#ifdef __WIIU__

        const std::array font_names = {
            "CafeCn.ttf"s,
            "CafeKr.ttf"s,
            "CafeStd.ttf"s,
            "CafeTw.ttf"s
        };

        void
        load_system_font(OSSharedDataType type,
                         bool merge = true)
        {
            const auto& style = ImGui::GetStyle();
            ImFontConfig config;
            config.Flags |= ImFontFlags_NoLoadError;
            config.EllipsisChar = U'…';
            config.FontDataOwnedByAtlas = false;
            config.MergeMode = merge;

            LOG_DEBUG("Loading {:?}", font_names.at(type));
            tweak_cafe(config);

            void* blob = nullptr;
            uint32_t blob_size = 0;
            if (!OSGetSharedData(type, 0, &blob, &blob_size))
                throw std::runtime_error{"Could not find \"" + font_names.at(type) + "\""};

            auto& io = ImGui::GetIO();
            if (!io.Fonts->AddFontFromMemoryTTF(blob, blob_size,
                                                style.FontSizeBase, &config))
                    throw std::runtime_error{"Could not parse \"" + font_names.at(type) + "\""};
        }


        void
        load_system_fonts()
        {
            // NOTE: first font is required, so we don't catch the exception here.
            load_system_font(OS_SHAREDDATATYPE_FONT_STANDARD, false);

            try {
                load_system_font(OS_SHAREDDATATYPE_FONT_CHINESE);
            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
            }

            try {
                load_system_font(OS_SHAREDDATATYPE_FONT_KOREAN);
            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
            }

            try {
                load_system_font(OS_SHAREDDATATYPE_FONT_TAIWANESE);
            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
            }
        }

#else // !__WIIU__

        // Helper RAII types for FontConfig
        namespace fc {

            struct Init {

                Init()
                {
                    if (!FcInit())
                        throw std::runtime_error("FcInit() failed");
                }

                ~Init()
                    noexcept
                {
                    FcFini();
                }

                // prevent copying, moving
                Init(const Init&) = delete;

            }; // struct Init


            struct PatternDeleter {
                void
                operator ()(FcPattern* pat)
                    const noexcept
                {
                    FcPatternDestroy(pat);
                }
            }; // struct PatternDeleter

            using PatternPtr = std::unique_ptr<FcPattern, PatternDeleter>;


            struct LangSetDeleter {
                void
                operator ()(FcLangSet* set)
                    const noexcept
                {
                    FcLangSetDestroy(set);
                }
            }; // struct LangSetDeleter

            using LangSetPtr = std::unique_ptr<FcLangSet, LangSetDeleter>;

        } // namespace fc


        std::filesystem::path
        find_font(const std::string& family,
                  const std::string& lang)
        {
            fc::PatternPtr pattern{FcPatternCreate()};
            if (!pattern)
                throw std::runtime_error{"FcPatterCreate() failed"};

            FcPatternAddString(pattern.get(), FC_FAMILY,
                               reinterpret_cast<const FcChar8*>(family.data()));
            // Tell fontconfig the fallback font should be "sans"
            FcPatternAddString(pattern.get(), FC_FAMILY,
                               reinterpret_cast<const FcChar8*>("sans"));

            fc::LangSetPtr lang_set{FcLangSetCreate()};
            if (!lang_set)
                throw std::runtime_error{"FcLangSetCreate() failed"};

            FcLangSetAdd(lang_set.get(), reinterpret_cast<const FcChar8*>(lang.data()));
            FcPatternAddLangSet(pattern.get(), FC_LANG, lang_set.get());
            const auto& style = ImGui::GetStyle();
            FcPatternAddDouble(pattern.get(), FC_SIZE, style.FontSizeBase);

            // FcPatternPrint(pattern.get());

            FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
            FcDefaultSubstitute(pattern.get());

            FcResult fresult;
            fc::PatternPtr font_pattern{FcFontMatch(nullptr, pattern.get(), &fresult)};
            if (fresult != FcResultMatch)
                throw std::runtime_error{"fc match error"};

            // FcPatternPrint(font_pattern.get());

            FcChar8* file = nullptr;
            if (FcPatternGetString(font_pattern.get(), FC_FILE, 0, &file) != FcResultMatch)
                throw std::logic_error{"font has no file!"};

            std::filesystem::path result = reinterpret_cast<char*>(file);

            return result;
        }


        void
        load_system_fonts()
        {
            fc::Init fc_init;

            auto cafe_std_path = find_font("nintendo_NTLG-DB_002", "en");
            auto cafe_cn_path  = find_font("nintendo_HeiTiW5_002", "zh-cn");
            auto cafe_ko_path  = find_font("nintendo_Tae-Gothic_002", "ko");
            auto cafe_tw_path  = find_font("nintendo_HeiMedium-B5_002", "zh-tw");

            std::set<std::filesystem::path> extra_fonts{
                cafe_cn_path,
                cafe_ko_path,
                cafe_tw_path
            };

            if (extra_fonts.contains(cafe_std_path))
                extra_fonts.erase(cafe_std_path);

            // NOTE: first font is required, so we don't catch the exception here.
            load(cafe_std_path, false);

            for (const auto& extra_path : extra_fonts)
                try {
                    load(extra_path);
                }
                catch (std::exception& e) {
                    LOG_ERROR("{}", e.what());
                }
        }

#endif // !__WIIU__

        std::u32string
        to_lower(const std::u32string& input)
        {
            auto output = input;
            for (auto& c : output) {
                if (c >= U'A' && c <= U'Z')
                    c += U'a' - U'A';
            }
            return output;
        }

    } // namespace


    void
    initialize()
    {
        TRACE_FUNC;

#ifdef IMGUI_ENABLE_FREETYPE
        auto& io = ImGui::GetIO();
        io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
        io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;
#endif

        load_system_fonts();
    }


    void
    finalize()
    {
        TRACE_FUNC;
    }


    void
    load(const std::filesystem::path& font,
         bool merge)
    {
        TRACE_FUNC;
        LOG_DEBUG("load({:?})", font.string());

        const auto& style = ImGui::GetStyle();
        ImFontConfig config;
        config.EllipsisChar = U'…';
        config.Flags |= ImFontFlags_NoLoadError;
        config.MergeMode = merge;

        if (font.filename() == "fontawesome-webfont.ttf")
            tweak_font_awesome(config);

#ifndef __WIIU__
        if (font.filename().string().starts_with("Cafe"))
            tweak_cafe(config);
#endif

        auto& io = ImGui::GetIO();
        if (!io.Fonts->AddFontFromFileTTF(font.c_str(), style.FontSizeBase, &config))
            throw std::runtime_error{"Could not load \""s + font.string() + "\""s};
    }


    void
    load_dir(const std::filesystem::path& dir)
    {
        TRACE_FUNC;
        LOG_DEBUG("load_dir({:?})", dir.string());

        if (!exists(dir) || !is_directory(dir))
            return;

        for (const auto& entry : std::filesystem::directory_iterator{dir}) {
            if (!entry.is_regular_file())
                continue;
            auto ext = to_lower(entry.path().extension().u32string());
            if (ext != U".ttf" && ext != U".otf")
                continue;
            try {
                load(entry);
            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
            }
        }
    }

} // namespace FontLoader
