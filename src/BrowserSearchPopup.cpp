/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <regex>
#include <thread>
#include <vector>

#include <SDL_stdinc.h>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "BrowserSearchPopup.hpp"

#include "App.hpp"
#include "ButtonHBox.hpp"
#include "CountryManager.hpp"
#include "enumerator.hpp"
#include "humanize.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "thread_safe.hpp"
#include "Timer.hpp"
#include "TraceFunction.hpp"
#include "TraceManager.hpp"
#include "tracer.hpp"
#include "UI.hpp"
#include "utils.hpp"


// Comment this to use JSON for the local cache.
#define USE_BEVE_CACHE


#ifndef USE_BEVE_CACHE
#include <glaze/exceptions/json_exceptions.hpp>
#else
#include <glaze/exceptions/binary_exceptions.hpp>
#endif


using namespace std::literals;

using utils::throw_if_stopped;


namespace BrowserSearchPopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class State {
            hidden,
            queued,
            visible,
        };


        struct Language {
            std::string name;
            std::optional<std::string> code;
        };


        using CodecVec = std::vector<std::string>;


        using TagVec = std::vector<std::string>;
        using OptTagVec = std::optional<TagVec>;


        using LanguageVec = std::vector<Language>;
        using OptLanguageVec = std::optional<LanguageVec>;

        using FileTimestamp = std::filesystem::file_time_type;


        // RAII type for resources
        struct Resources {

            ImGuiTextFilter tag_text_filter;
            ImGuiTextFilter country_text_filter;
            ImGuiTextFilter language_text_filter;

            std::optional<CodecVec> codecs;

            SearchParams search_params;
            ConfirmFunction confirm_func;

            thread_safe<OptLanguageVec> safe_languages;
            std::jthread languages_loader_thread;

            std::optional<std::regex> ignored_tags_regex;
            thread_safe<OptTagVec> safe_tags;
            std::jthread tags_loader_thread;

        }; // struct Resources


        /*-----------*/
        /* Constants */
        /*-----------*/

        const auto popup_id = "Search on RadioBrowser.info"s;

        constexpr auto cache_expiration = 24h;


        /*-----------*/
        /* Variables */
        /*-----------*/

        std::optional<Resources> res;

        State state = State::hidden;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        action_confirm();

        void
        action_reset_fields();

        void
        fetch_codecs();

        void
        fetch_languages(OptLanguageVec& languages);

        void
        fetch_tags(OptTagVec& tags);

        void
        languages_loader_thread_function(std::stop_token stoken)
            noexcept;

        [[nodiscard]]
        std::regex
        load_ignored_tags_regex(std::stop_token& stoken);

        [[nodiscard]]
        std::optional<FileTimestamp>
        load_languages_cache(std::stop_token& stoken)
            noexcept;

        [[nodiscard]]
        std::optional<FileTimestamp>
        load_tags_cache(std::stop_token& stoken)
            noexcept;

        std::string
        make_codec_label(const std::string& codec);

        std::string
        make_country_label(const std::string& code,
                           std::string name = "");

        std::string
        make_language_label(const Language& lang);

        std::string
        make_tag_label(const std::string& tag);

        void
        save_languages_cache(const RadioBrowserAPI::LanguageVec& rb_languages)
            noexcept;

        void
        save_tags_cache(const RadioBrowserAPI::TagVec& rb_tags)
            noexcept;

        void
        show_filters();

        void
        tags_loader_thread_function(std::stop_token stoken)
            noexcept;

        std::string
        to_label(Order order);

        bool
        try_open_file(std::ifstream& stream,
                      const std::filesystem::path& filename);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        action_confirm()
        {
            TRACE_FUNC;

            assert(res);

            ImGui::CloseCurrentPopup();
            if (res->confirm_func)
                res->confirm_func(res->search_params);
        }


        void
        action_reset_fields()
        {
            TRACE_FUNC;

            res->search_params = {};
        }


        void
        fetch_codecs()
        {
            TRACE_FUNC;

            assert(res);

            // TODO: when RB errors out, it should be possible to try again
            if (res->codecs)
                return;

            res->codecs.emplace();

            RadioBrowserAPI::CodecsParams params;
            params.order = RadioBrowserAPI::CodecsParams::Order::name;
            params.hidebroken = true;
            params.limit = 1000;

            RadioBrowserAPI::get_codecs(
                params,
                [](RadioBrowserAPI::CodecVec rb_codecs)
                {
                    assert(res);

                    for (auto& [name, stationcount] : rb_codecs)
                        res->codecs->push_back(std::move(name));
                    LOG_INFO("Received {} codecs.", res->codecs->size());
                },
                [](const std::exception& e)
                {
                    LOG_ERROR("Fetching codecs: {}", e.what());
                }
            );
        }


        void
        fetch_languages(OptLanguageVec& languages)
        {
            TRACE_FUNC;

            if (languages)
                return;

            languages.emplace();

            assert(res);
            res->languages_loader_thread = std::jthread{languages_loader_thread_function};
        }


        void
        fetch_tags(OptTagVec& tags)
        {
            TRACE_FUNC;

            // TODO: when RB errors out, it should be possible to try again
            if (tags)
                return;

            tags.emplace();

            assert(res);
            res->tags_loader_thread = std::jthread{tags_loader_thread_function};
        }


        void
        languages_loader_thread_function(std::stop_token stoken)
            noexcept
        {
            TraceManager::thread_name("languages loader thread"sv);

            TraceFunction tf{"BrowserSearchPopup"sv};

            try {
                auto now = std::chrono::file_clock::now();
                auto timestamp = load_languages_cache(stoken);

                throw_if_stopped(stoken);

                // Skip downloading a new languages list if the cache is recent.
                if (timestamp) {
                    auto cache_age = now - *timestamp;
                    if (cache_age < cache_expiration) {
                        LOG_DEBUG("languages cache is recent: {}",
                                  humanize::duration(cache_age));
                        return;
                    }
                }

                RadioBrowserAPI::LanguagesParams params;
                params.order = RadioBrowserAPI::LanguagesParams::Order::name;
                params.hidebroken = true;
                params.limit = 10000;

                RadioBrowserAPI::get_languages_sync(
                    stoken,
                    ""s,
                    params,
                    [&stoken]
                    (const RadioBrowserAPI::LanguageVec rb_languages)
                        mutable
                    {
                        assert(res);

                        throw_if_stopped(stoken);

                        LanguageVec new_languages;

                        for (auto& [name, code, stationcount] : rb_languages) {
                            throw_if_stopped(stoken);
                            new_languages.emplace_back(name, code);
                        }

                        LOG_INFO("Received {} languages.", new_languages.size());
                        res->safe_languages.store(std::move(new_languages));

                        throw_if_stopped(stoken);

                        save_languages_cache(rb_languages);
                    },
                    [](const std::exception& e)
                    {
                        LOG_ERROR("Fetching languages: {}", e.what());
                    }
                );

            }
            catch (std::exception& e) {
                LOG_ERROR("languages_thread failed: {}", e.what());
            }
        }


        std::regex
        load_ignored_tags_regex(std::stop_token& stoken)
        try {
            std::ifstream input;
            std::filesystem::path filename = "ignored_tags.txt";
            // First, try user customized filter.
            if (!try_open_file(input, App::get_config_dir() / filename))
                // Then try default filter.
                if (!try_open_file(input, App::get_content_dir() / filename))
                    throw std::runtime_error{"could not find \""s + filename.string() + "\""s};

            throw_if_stopped(stoken);

            std::string line;
            std::string full_regex;
            unsigned counter = 0;
            while (getline(input, line)) {
                if (stoken.stop_requested())
                    throw std::runtime_error{"stop requested"};
                if (line.empty())
                    continue;
                if (counter)
                    full_regex += "|";
                full_regex += "(?:" + line + ")";
                ++counter;
            }

            throw_if_stopped(stoken);

            std::regex result;
            result.assign(full_regex,
                          std::regex_constants::ECMAScript |
                          std::regex_constants::optimize);
            LOG_INFO("Found {} rules in {:?}.", counter, filename.string());
            // LOG_DEBUG("{}", full_regex);
            return result;
        }
        catch (std::exception& e) {
            LOG_ERROR("{}", e.what());
            return {};
        }


        std::optional<FileTimestamp>
        load_languages_cache(std::stop_token& stoken)
            noexcept
        {
            assert(res);

            try {
#ifndef USE_BEVE_CACHE
                auto cached_languages_filename = App::get_data_dir() / "languages.json";
#else
                auto cached_languages_filename = App::get_data_dir() / "languages.beve";
#endif
                if (!exists(cached_languages_filename))
                    return {};

                FileTimestamp result = last_write_time(cached_languages_filename);

                RadioBrowserAPI::LanguageVec rb_languages;

                Timer t{"load languages cache"};
                t.start();
#ifndef USE_BEVE_CACHE
                glz::ex::read_file_json(
                    rb_languages,
                    cached_languages_filename.string(),
                    std::string{}
                );
#else
                glz::ex::read_file_beve(
                    rb_languages,
                    cached_languages_filename.string(),
                    std::string{}
                );
#endif
                t.stop();
                t.report();

                throw_if_stopped(stoken);

                LanguageVec new_languages;

                for (auto& [name, code, stationcount] : rb_languages) {
                    throw_if_stopped(stoken);
                    new_languages.emplace_back(std::move(name), std::move(code));
                }

                LOG_INFO("Loaded {} languages.", new_languages.size());
                res->safe_languages.store(std::move(new_languages));
                LOG_INFO("Loaded languages from local cache: {:?}",
                         cached_languages_filename.string());

                return result;
            }
            catch (std::exception& e) {
                LOG_ERROR("Failed to load cached languages: {}", e.what());
                return {};
            }
        }


        std::optional<FileTimestamp>
        load_tags_cache(std::stop_token& stoken)
            noexcept
        {
            assert(res);
            auto& ignored_tags_regex = res->ignored_tags_regex;

            try {
#ifndef USE_BEVE_CACHE
                auto cached_tags_filename = App::get_data_dir() / "tags.json";
#else
                auto cached_tags_filename = App::get_data_dir() / "tags.beve";
#endif
                if (!exists(cached_tags_filename))
                    return {};

                FileTimestamp result = last_write_time(cached_tags_filename);

                RadioBrowserAPI::TagVec rb_tags;

                Timer t{"load tags cache"};
                t.start();
#ifndef USE_BEVE_CACHE
                glz::ex::read_file_json(
                    rb_tags,
                    cached_tags_filename.string(),
                    std::string{}
                );
#else
                glz::ex::read_file_beve(
                    rb_tags,
                    cached_tags_filename.string(),
                    std::string{}
                );
#endif
                t.stop();
                t.report();

                throw_if_stopped(stoken);

                if (!ignored_tags_regex)
                    ignored_tags_regex = load_ignored_tags_regex(stoken);

                throw_if_stopped(stoken);

                std::smatch matches;
                TagVec new_tags;
                for (auto& [name, stationcount] : rb_tags) {
                    throw_if_stopped(stoken);
                    if (res->ignored_tags_regex &&
                        regex_search(name, matches, *ignored_tags_regex) &&
                        matches.length() > 0) {
                        // LOG_DEBUG("Ignored tag: {} (from {})",
                        //           name,
                        //           matches.str());
                        continue; // skip this tag
                    }

                    new_tags.push_back(std::move(name));
                }
                LOG_INFO("Loaded {} tags.", new_tags.size());
                res->safe_tags.store(std::move(new_tags));
                LOG_INFO("Loaded tags from local cache: {:?}", cached_tags_filename.string());

                return result;
            }
            catch (std::exception& e) {
                LOG_ERROR("Failed to load cached tags: {}", e.what());
                return {};
            }
        }


        std::string
        make_codec_label(const std::string& codec)
        {
            if (codec.empty())
                return {};
            return ICON_FA_FLASK " "s + codec;
        }


        std::string
        make_country_label(const std::string& code,
                           std::string name)
        {
            if (code.empty())
                return {};
            auto utf8 = CountryManager::get_utf8(code);
            if (utf8.empty()) {
                if (!name.empty())
                    return name + " ("s + code + ")"s;
                return code;
            }
            if (name.empty())
                name = CountryManager::get_name(code);
            return utf8 + " "s + name + " ("s + code + ")"s;
        }


        std::string
        make_language_label(const Language& lang)
        {
            std::string result = ICON_FA_LANGUAGE " "s + lang.name;
            if (lang.code)
                result += " ("s + *lang.code + ")"s;
            return result;
        }


        std::string
        make_tag_label(const std::string& tag)
        {
            if (tag.empty())
                return {};
            return ICON_FA_TAG " "s + tag;
        }


        void
        save_languages_cache(const RadioBrowserAPI::LanguageVec& rb_languages)
            noexcept
        {
            try {
                Timer t{"save languages cache"};
                t.start();
#ifndef USE_BEVE_CACHE
                auto cached_languages_filename = App::get_data_dir() / "languages.json";
                glz::ex::write_file_json(
                    rb_languages,
                    cached_languages_filename.string(),
                    std::string{}
                );
#else
                auto cached_languages_filename = App::get_data_dir() / "languages.beve";
                glz::ex::write_file_beve(
                    rb_languages,
                    cached_languages_filename.string(),
                    std::string{}
                );
#endif
                t.stop();
                t.report();

                LOG_INFO("Saved languages to local cache: {:?}",
                         cached_languages_filename.string());
            }
            catch (std::exception& e) {
                LOG_ERROR("Failed to cache languages: {}", e.what());
            }
        }


        void
        save_tags_cache(const RadioBrowserAPI::TagVec& rb_tags)
            noexcept
        {
            try {
                Timer t{"save tags cache"};
                t.start();
#ifndef USE_BEVE_CACHE
                auto cached_tags_filename = App::get_data_dir() / "tags.json";
                glz::ex::write_file_json(
                    rb_tags,
                    cached_tags_filename.string(),
                    std::string{}
                );
#else
                auto cached_tags_filename = App::get_data_dir() / "tags.beve";
                glz::ex::write_file_beve(
                    rb_tags,
                    cached_tags_filename.string(),
                    std::string{}
                );
#endif
                t.stop();
                t.report();

                LOG_INFO("Saved tags to local cache: {:?}", cached_tags_filename.string());
            }
            catch (std::exception& e) {
                LOG_ERROR("Failed to cache tags: {}", e.what());
            }
        }


        void
        show_filters()
        {
            using namespace ImGui::RAII;

            assert(res);

            if (Child filters_group{"filters_group",
                                    {0, 0},
                                    ImGuiChildFlags_AutoResizeX |
                                    ImGuiChildFlags_AutoResizeY |
                                    ImGuiChildFlags_Borders |
                                    ImGuiChildFlags_NavFlattened}) {

                ImGui::TextUnformatted(ICON_FA_FILTER " Filters");

                auto& filter = res->search_params.filter;

                const std::string name_entry_label     = "Name"s;
                const std::string tag_combo_label      = "Tag"s;
                const std::string country_combo_label  = "Country"s;
                const std::string language_combo_label = "Language"s;
                const std::string codec_combo_label    = "Codec"s;
                const float labels_width = UI::max_width({
                        name_entry_label,
                        tag_combo_label,
                        country_combo_label,
                        language_combo_label,
                        codec_combo_label
                    });

                ItemWidth items_width{500};

                /*-----------------*/
                /* Filter by name. */
                /*-----------------*/
                ImGui::AlignTextToFramePadding();
                ImGui::TextAligned(1.0f, labels_width, name_entry_label);
                ImGui::SameLine();
                ImGui::InputText("##name", filter.name);

                /*----------------*/
                /* Filter by tag. */
                /*----------------*/
                ImGui::AlignTextToFramePadding();
                ImGui::TextAligned(1.0f, labels_width, tag_combo_label);
                ImGui::SameLine();
                if (Combo tag_combo{"##tag",
                                    make_tag_label(filter.tag),
                                    ImGuiComboFlags_HeightLargest}) {
                    auto& text_filter = res->tag_text_filter;

                    if (ImGui::IsWindowAppearing()) {
                        ImGui::SetKeyboardFocusHere();
                        SDL_strlcpy(text_filter.InputBuf,
                                    filter.tag.data(),
                                    sizeof text_filter.InputBuf);
                        text_filter.Build();
                    }

                    ImGui::SetNextItemWidth(800);
                    if (text_filter.Draw("##tag_text_filter"))
                        filter.tag = text_filter.InputBuf;

                    if (ImGui::Selectable("(any tag)", filter.tag.empty()))
                        filter.tag.clear();

                    auto tags_guard = res->safe_tags.lock();
                    auto& tags = *tags_guard;
                    if (!tags)
                        fetch_tags(tags);

                    // The rest of tags.
                    if (Child list{"list",
                                   {0.0f, 12 * ImGui::GetTextLineHeight()},
                                   ImGuiChildFlags_NavFlattened}) {

                        for (auto [index, tag] : *tags | std::views::enumerate) {
                            if (!text_filter.PassFilter(tag.data()))
                                continue;
                            ID id{index};
                            if (ImGui::Selectable(make_tag_label(tag), filter.tag == tag)) {
                                filter.tag = tag;
                                // NOTE: must explicitly close the popup because of the nesting.
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                }

                /*--------------------*/
                /* Filter by country. */
                /*--------------------*/
                ImGui::AlignTextToFramePadding();
                ImGui::TextAligned(1.0, labels_width, country_combo_label);
                ImGui::SameLine();
                if (Combo country_combo{"##country",
                                        filter.country,
                                        ImGuiComboFlags_HeightLargest}) {
                    auto& text_filter = res->country_text_filter;

                    if (ImGui::IsWindowAppearing()) {
                        ImGui::SetKeyboardFocusHere();
                        SDL_strlcpy(text_filter.InputBuf,
                                    filter.country.data(),
                                    sizeof text_filter.InputBuf);
                        text_filter.Build();
                    }

                    // NOTE: only exact matches are allowed
                    ImGui::SetNextItemWidth(1100);
                    text_filter.Draw("##country");

                    if (ImGui::Selectable("(any country)", filter.country.empty()))
                        filter.country.clear();

                    // The rest of countries
                    if (Child list{"list",
                                   {0.0f, 12 * ImGui::GetTextLineHeight()},
                                   ImGuiChildFlags_NavFlattened}) {
                        CountryManager::for_each_country(
                            [&filter, &text_filter]
                            (std::size_t index,
                             const CountryManager::Country& c)
                            {
                                const auto& [code, name] = c;

                                if (!text_filter.PassFilter(code.data()) &&
                                    !text_filter.PassFilter(name.data()))
                                    return;

                                ID id{index};
                                if (ImGui::Selectable(make_country_label(code, name),
                                                      filter.country == code)) {
                                    filter.country = code;
                                    // NOTE: must explicitly close the popup because of the
                                    // nesting.
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                        );
                    }
                }

                /*---------------------*/
                /* Filter by language. */
                /*---------------------*/
                ImGui::AlignTextToFramePadding();
                ImGui::TextAligned(1.0, labels_width, language_combo_label);
                ImGui::SameLine();
                if (Combo language_combo{"##language",
                                         filter.language,
                                         ImGuiComboFlags_HeightLargest}) {
                    auto& text_filter = res->language_text_filter;

                    if (ImGui::IsWindowAppearing()) {
                        ImGui::SetKeyboardFocusHere();
                        SDL_strlcpy(text_filter.InputBuf,
                                    filter.language.data(),
                                    sizeof text_filter.InputBuf);
                        text_filter.Build();
                    }

                    ImGui::SetNextItemWidth(900);
                    if (text_filter.Draw("##language_text_filter"))
                        filter.language = text_filter.InputBuf;

                    if (ImGui::Selectable("(any language)", filter.language.empty()))
                        filter.language.clear();

                    auto languages_guard = res->safe_languages.lock();
                    auto& languages = *languages_guard;
                    if (!languages)
                        fetch_languages(languages);

                    if (Child list{"list",
                            {0.0f, 12 * ImGui::GetTextLineHeight()},
                            ImGuiChildFlags_NavFlattened}) {
                        for (auto [index, lang] : *languages | std::views::enumerate) {

                            bool passed_name = text_filter.PassFilter(lang.name.data());
                            bool passed_code = lang.code
                                && text_filter.PassFilter(lang.code->data());
                            if (!passed_name && !passed_code)
                                continue;

                            ID id{index};
                            if (ImGui::Selectable(make_language_label(lang),
                                                  filter.language == lang.name)) {
                                filter.language = lang.name;
                                // NOTE: must explicitly close the popup because of the nesting.
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                }

                /*------------------*/
                /* Filter by codec. */
                /*------------------*/
                ImGui::AlignTextToFramePadding();
                ImGui::TextAligned(1.0, labels_width, codec_combo_label);
                ImGui::SameLine();
                if (Combo codec_combo{"##codec",
                                      make_codec_label(filter.codec),
                                      ImGuiComboFlags_HeightLarge}) {

                    if (ImGui::Selectable("(any codec)", filter.codec.empty()))
                        filter.codec = "";

                    // The rest of codecs.
                    if (!res->codecs)
                        fetch_codecs();

                    for (auto [index, codec] : *res->codecs | std::views::enumerate) {
                        ID id{index};
                        if (ImGui::Selectable(make_codec_label(codec),
                                              filter.codec == codec))
                            filter.codec = codec;
                    }
                }

            } // filters_group

        }


        void
        tags_loader_thread_function(std::stop_token stoken)
            noexcept
        {
            TraceManager::thread_name("tags loader thread"sv);

            TraceFunction tf{"BrowserSearchPopup"sv};

            try {
                auto now = std::chrono::file_clock::now();
                auto timestamp = load_tags_cache(stoken);

                throw_if_stopped(stoken);

                // Skip downloading a new tags list if the cache is recent.
                if (timestamp) {
                    auto cache_age = now - *timestamp;
                    if (cache_age < cache_expiration) {
                        LOG_DEBUG("tags cache is recent: {}",
                                  humanize::duration(cache_age));
                        return;
                    }
                }

                RadioBrowserAPI::TagsParams params;
                params.order = RadioBrowserAPI::TagsParams::Order::name;
                params.limit = 50000;
                params.hidebroken = true;

                RadioBrowserAPI::get_tags_sync(
                    stoken,
                    params,
                    [&stoken]
                    (const RadioBrowserAPI::TagVec rb_tags)
                        mutable
                    {
                        throw_if_stopped(stoken);

                        assert(res);

                        auto& ignored_tags_regex = res->ignored_tags_regex;

                        if (!ignored_tags_regex)
                            ignored_tags_regex = load_ignored_tags_regex(stoken);

                        throw_if_stopped(stoken);

                        TagVec new_tags;
                        std::smatch matches;

                        for (auto& [name, stationcount] : rb_tags) {
                            throw_if_stopped(stoken);

                            if (ignored_tags_regex &&
                                regex_search(name, matches, *ignored_tags_regex) &&
                                matches.length() > 0) {
#if 0
                                // NOTE: don't log this for releases, since the whole point is
                                // to suppress spam/vulgar language.
                                LOG_DEBUG("Ignored tag: {} (from {})",
                                          name,
                                          matches.str());
#endif
                                continue; // skip this tag
                            }

                            new_tags.push_back(name);
                        }

                        LOG_INFO("Received {} tags.", new_tags.size());
                        res->safe_tags.store(std::move(new_tags));

                        throw_if_stopped(stoken);

                        save_tags_cache(rb_tags);
                    },
                    [](const std::exception& e)
                    {
                        LOG_ERROR("Fetching tags: {}", e.what());
                    }
                );
            }
            catch (std::exception& e) {
                LOG_ERROR("tags_thread failed: {}", e.what());
            }

        }


        std::string
        to_label(Order order)
        {
            switch (order) {
                using enum Order;
                case name_asc:
                    return "Name " ICON_FA_SORT_ALPHA_ASC;
                case name_desc:
                    return "Name " ICON_FA_SORT_ALPHA_DESC;
                case country_asc:
                    return "Country " ICON_FA_SORT_ALPHA_ASC;
                case country_desc:
                    return "Country " ICON_FA_SORT_ALPHA_DESC;
                case language_asc:
                    return "Language " ICON_FA_SORT_ALPHA_ASC;
                case language_desc:
                    return "Language " ICON_FA_SORT_ALPHA_DESC;
                case clicks_desc:
                    return "Clicks " ICON_FA_SORT_AMOUNT_DESC;
                case clicks_asc:
                    return "Clicks " ICON_FA_SORT_AMOUNT_ASC;
                case votes_desc:
                    return "Votes " ICON_FA_SORT_AMOUNT_DESC;
                case votes_asc:
                    return "Votes " ICON_FA_SORT_AMOUNT_ASC;
                case random:
                    return "Random";
                default:
                    return "ERROR";
            }
        }


        bool
        try_open_file(std::ifstream& stream,
                      const std::filesystem::path& filename)
        {
            if (!exists(filename))
                return false;
            stream.open(filename);
            return stream.is_open();
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize()
    {
        TRACE_FUNC;

        res.emplace();
    }


    void
    finalize()
    {
        TRACE_FUNC;

        res.reset();
    }


    void
    open(ConfirmFunction func)
    {
        TRACE_FUNC;

        state = State::queued;

        assert(res);
        res->confirm_func = std::move(func);
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        if (state == State::hidden)
            return;

        if (state == State::queued) {
            ImGui::OpenPopup(popup_id);
            state = State::visible;
        }

        assert(res);

        auto viewport = ImGui::GetMainViewport();
        auto center = viewport->GetWorkCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        Popup popup{popup_id,
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoMove};
        if (!popup) {
            state = State::hidden;
            res->confirm_func = {};
            return;
        }

        UI::Title(popup_id);

        if (Child content{"content",
                          {0, 0},
                          ImGuiChildFlags_AutoResizeX |
                          ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_NavFlattened}) {

            show_filters();

            ImGui::SameLine();

            if (Child sorting{
                    "sorting",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeX |
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_Borders |
                    ImGuiChildFlags_NavFlattened
                }) {

                ImGui::TextUnformatted(ICON_FA_SORT " Order");

                ImGui::SetNextItemWidth(280);
                if (Combo order_combo{"##order",
                                      to_label(res->search_params.order),
                                      ImGuiComboFlags_HeightLargest}) {

                    for (auto o : enumerator::enumerate<Order>())
                        if (ImGui::Selectable(to_label(o), res->search_params.order == o))
                            res->search_params.order = o;

                }

            } // sorting

        } // content

        ButtonHBox buttons;
        buttons.spread = true;
        buttons.add(
            ICON_FA_TIMES " Cancel",
            ImGui::CloseCurrentPopup
        );
        buttons.add(
            ICON_FA_ERASER " Reset",
            "Reset browser options to default.",
            false,
            action_reset_fields
        );
        buttons.add(
            ICON_FA_BINOCULARS " Search",
            "Search with the selected options.",
            true,
            action_confirm
        );
        buttons.show();
    }

} // namespace BrowserSearchPopup
