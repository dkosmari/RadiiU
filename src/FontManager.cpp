/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
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

#include "FontManager.hpp"

#include "tracer.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace FontManager {

    float default_size = 32;


    namespace {

#ifdef __WIIU__

        void
        load_system_fonts()
        {
            auto& io = ImGui::GetIO();
            // Load main font: CafeStd
            ImFontConfig config;
            config.Flags |= ImFontFlags_NoLoadError;
            config.EllipsisChar = U'…';
#ifdef IMGUI_ENABLE_FREETYPE
            // NOTE: CafeStd seems to always be too low, about 1/8th of the font size
            config.GlyphOffset.y = - default_size * (1.0f / 8.0f);
#endif
            config.FontDataOwnedByAtlas = false;

            void* font_data = nullptr;
            uint32_t font_size = 0;

            if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0,
                                &font_data, &font_size)) {
                if (!io.Fonts->AddFontFromMemoryTTF(font_data, font_size,
                                                    default_size, &config))
                    throw std::runtime_error{"could not load CafeStd"};
            } else
                throw std::runtime_error{"CafeStd font is missing"};

            config.MergeMode = true;

            if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_CHINESE, 0,
                                &font_data, &font_size)) {
                if (!io.Fonts->AddFontFromMemoryTTF(font_data, font_size,
                                                    default_size, &config))
                    throw std::runtime_error{"could not load CafeCn"};
            } else
                throw std::runtime_error{"CafeCn font is missing"};

            if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_KOREAN, 0,
                                &font_data, &font_size)) {
                if (!io.Fonts->AddFontFromMemoryTTF(font_data, font_size,
                                                    default_size, &config))
                    throw std::runtime_error{"could not load CafeKr"};
            } else
                throw std::runtime_error{"CafeKr font is missing"};

            if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_TAIWANESE, 0,
                                &font_data, &font_size)) {
                if (!io.Fonts->AddFontFromMemoryTTF(font_data, font_size,
                                                    default_size, &config))
                    throw std::runtime_error{"could not load CafeTw"};
            } else
                throw std::runtime_error{"CafeTw font is missing"};
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
            FcPatternAddDouble(pattern.get(), FC_SIZE, default_size);

            // cout << "DEBUG: pattern:" << endl;
            // FcPatternPrint(pattern.get());

            FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
            FcDefaultSubstitute(pattern.get());

            FcResult fresult;
            fc::PatternPtr font_pattern{FcFontMatch(nullptr, pattern.get(), &fresult)};
            if (fresult != FcResultMatch)
                throw std::runtime_error{"fc match error"};

            // cout << "DEBUG font_pattern:" << endl;
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

            // if fontconfig returned the same font for every case
            if (extra_fonts.contains(cafe_std_path))
                extra_fonts.clear();

            load(cafe_std_path, false);
            for (const auto& extra_path : extra_fonts)
                try {
                    load(extra_path);
                }
                catch (std::exception& e) {
                    cout << "ERROR: " << e.what() << endl;
                }
        }

#endif // __WIIU__

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

        auto& io = ImGui::GetIO();
#ifdef IMGUI_ENABLE_FREETYPE
        io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
        io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;
#else
        std::ignore = io;
#endif

        load_system_fonts();
    }


    void
    finalize()
    {
        TRACE_FUNC;
    }



    void
    load(const std::filesystem::path& filename,
         bool merge)
    {
        TRACE_FUNC;
        cout << "DEBUG: font = " << filename << endl;

        ImFontConfig config;
        config.EllipsisChar = U'…';
        config.Flags |= ImFontFlags_NoLoadError;
        config.MergeMode = merge;

#ifdef IMGUI_ENABLE_FREETYPE
        // NOTE: CafeStd seems to always be too low, about 1/8th of the font size
        config.GlyphOffset.y = - default_size * (1.0f / 8.0f);
#endif

        auto& io = ImGui::GetIO();
        if (!io.Fonts->AddFontFromFileTTF(filename.c_str(), default_size, &config))
            throw std::runtime_error{"could not load \""s + filename.string() + "\""s};
    }


    void
    load_dir(const std::filesystem::path& dir)
    {
        TRACE_FUNC;
        cout << "DEBUG: dir = " << dir << endl;

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
                cout << "ERROR: " << e.what() << endl;
            }
        }
    }

} // namespace FontManager
