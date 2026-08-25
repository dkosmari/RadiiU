/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <fstream>
#include <optional>
#include <regex>
#include <vector>
#include <filesystem>

#include <SDL_stdinc.h>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "BrowserSearchPopup.hpp"

#include "App.hpp"
#include "ButtonHBox.hpp"
#include "CountryManager.hpp"
#include "enumerator.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using namespace std::literals;


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


        struct Country {
            std::string code;
            std::string name;
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "Search on RadioBrowser.info";


        /*-----------*/
        /* Variables */
        /*-----------*/

        State state = State::hidden;
        SearchParams search_params;
        ConfirmFunction confirm_func;

        ImGuiTextFilter tag_text_filter;
        ImGuiTextFilter country_text_filter;

        std::optional<std::vector<std::string>> tags;
        std::optional<std::vector<std::string>> codecs;

        std::regex tags_regex;


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
        fetch_tags();

        void
        load_tags_regex();

        std::string
        make_codec_label(const std::string& codec);

        std::string
        make_country_label(const std::string& code,
                           std::string name = "");

        std::string
        make_tag_label(const std::string& tag);

        void
        show_filters();

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

            ImGui::CloseCurrentPopup();
            if (confirm_func)
                confirm_func(search_params);
        }


        void
        action_reset_fields()
        {
            TRACE_FUNC;

            search_params = {};
        }


        void
        fetch_codecs()
        {
            TRACE_FUNC;

            // TODO: when RB errors out, it should be possible to try again
            if (codecs)
                return;

            codecs.emplace();

            RadioBrowserAPI::CodecParams params;
            params.order = RadioBrowserAPI::CodecParams::Order::name;
            params.hidebroken = true;

            RadioBrowserAPI::get_codecs(
                params,
                [](RadioBrowserAPI::CodecVec rb_codecs)
                {
                    for (auto& [name, stationcount] : rb_codecs)
                        codecs->push_back(std::move(name));
                    LOG_INFO("Received {} codecs.", codecs->size());
                },
                [](const std::exception& e)
                {
                    LOG_ERROR("Fetching codecs: {}", e.what());
                }
            );
        }


        // TODO: should run this in a background thread, with streaming, and caching to SD card.
        void
        fetch_tags()
        {
            TRACE_FUNC;

            // TODO: when RB errors out, it should be possible to try again
            if (tags)
                return;

            tags.emplace();

            RadioBrowserAPI::TagParams params;
            params.order = RadioBrowserAPI::TagParams::Order::name;
            params.limit = 20000;
            params.hidebroken = true;

            RadioBrowserAPI::get_tags(
                params,
                [](RadioBrowserAPI::TagVec rb_tags)
                {
                    std::smatch matches;
                    for (auto& [name, stationcount] : rb_tags) {
                        // ignore some bogus tags
                        if (name.size() < 2 || name.size() > 32)
                            continue;
                        if (regex_search(name, matches, tags_regex) &&
                            matches.length() > 0) {
                            // LOG_DEBUG("Ignored tag: {} (from {})", name, matches.str());
                            continue;
                        }
                        tags->push_back(std::move(name));
                    }
                    LOG_INFO("Received {} tags.", tags->size());
                },
                [](const std::exception& e)
                {
                    LOG_ERROR("Fetching tags: {}", e.what());
                }
            );
        }


        void
        load_tags_regex()
        try {
            std::ifstream input;
            if (!try_open_file(input, App::get_config_path() / "tags.ignore"))
                if (!try_open_file(input, App::get_content_path() / "tags.ignore"))
                    throw std::runtime_error{"could not find tags.ignore"};
            std::string line;
            std::string full_regex;
            unsigned counter = 0;
            while (getline(input, line)) {
                if (line.empty())
                    continue;
                if (counter)
                    full_regex += "|";
                full_regex += "(?:" + line + ")";
                ++counter;
            }
            tags_regex.assign(full_regex,
                              std::regex_constants::ECMAScript |
                              std::regex_constants::optimize);
            LOG_INFO("Found {} rules in tags.ignore.", counter);
            // LOG_DEBUG("{}", full_regex);
        }
        catch (std::exception& e) {
            LOG_ERROR("{}", e.what());
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
        make_tag_label(const std::string& tag)
        {
            if (tag.empty())
                return {};
            return ICON_FA_TAG " "s + tag;
        }


        void
        show_filters()
        {
            using namespace ImGui::RAII;

            if (Child filters_group{"filters_group",
                                    {0, 0},
                                    ImGuiChildFlags_AutoResizeX |
                                    ImGuiChildFlags_AutoResizeY |
                                    ImGuiChildFlags_Borders |
                                    ImGuiChildFlags_NavFlattened}) {

                ImGui::TextUnformatted(ICON_FA_FILTER " Filters");

                auto& filter = search_params.filter;

                const std::string name_entry_label = "Name";
                const std::string tag_combo_label = "Tag";
                const std::string country_combo_label = "Country";
                const std::string codec_combo_label = "Codec";
                const float labels_width = UI::max_width({
                        name_entry_label,
                        tag_combo_label,
                        country_combo_label,
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
                    if (ImGui::IsWindowAppearing()) {
                        ImGui::SetKeyboardFocusHere();
                        SDL_strlcpy(tag_text_filter.InputBuf,
                                    filter.tag.data(),
                                    sizeof tag_text_filter.InputBuf);
                        tag_text_filter.Build();
                    }
                    tag_text_filter.Draw("##tag_text_filter", 800);

                    if (ImGui::Selectable("(any tag)", filter.tag.empty()))
                        filter.tag.clear();

                    if (!tags)
                        fetch_tags();

                    // The rest of tags.
                    if (Child list{"list",
                                   {0.0f, 12 * ImGui::GetTextLineHeight()},
                                   ImGuiChildFlags_NavFlattened}) {

                        for (auto& tag : *tags) {
                            if (!tag_text_filter.PassFilter(tag.data()))
                                continue;
                            const bool is_selected = filter.tag == tag;
                            if (ImGui::Selectable(make_tag_label(tag), is_selected)) {
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
                                        make_country_label(filter.country),
                                        ImGuiComboFlags_HeightLargest}) {
                    if (ImGui::IsWindowAppearing()) {
                        ImGui::SetKeyboardFocusHere();
                        SDL_strlcpy(country_text_filter.InputBuf,
                                    filter.country.data(),
                                    sizeof country_text_filter.InputBuf);
                        country_text_filter.Build();
                    }
                    country_text_filter.Draw("##country", 1100);

                    if (ImGui::Selectable("(any country)", filter.country.empty()))
                        filter.country.clear();

                    // The rest of countries
                    if (Child list{"list",
                                   {0.0f, 12 * ImGui::GetTextLineHeight()},
                                   ImGuiChildFlags_NavFlattened}) {
                        CountryManager::for_each_country(
                            [&filter](const CountryManager::Country& c)
                            {
                                const auto& [code, name] = c;

                                if (!country_text_filter.PassFilter(code.data()) &&
                                    !country_text_filter.PassFilter(name.data()))
                                    return;

                                const bool is_selected = filter.country == code;
                                if (ImGui::Selectable(make_country_label(code, name),
                                                      is_selected)) {
                                    filter.country = code;
                                    // NOTE: must explicitly close the popup because of the
                                    // nesting.
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                        );
                    }
                }

                // TODO: add language filter

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
                    if (!codecs)
                        fetch_codecs();

                    for (const auto& codec : *codecs)
                        if (ImGui::Selectable(make_codec_label(codec),
                                              filter.codec == codec))
                            filter.codec = codec;
                }

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

        load_tags_regex();
    }


    void
    finalize()
    {
        TRACE_FUNC;


        action_reset_fields();
        confirm_func = {};
        tags_regex = {};
    }


    void
    open(ConfirmFunction func)
    {
        TRACE_FUNC;

        state = State::queued;
        confirm_func = std::move(func);
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


        auto viewport = ImGui::GetMainViewport();
        auto center = viewport->GetWorkCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        Popup popup{popup_id,
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove};
        if (!popup) {
            state = State::hidden;
            confirm_func = {};
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
                                      to_label(search_params.order),
                                      ImGuiComboFlags_HeightLargest}) {

                    for (auto o : enumerator::enumerate<Order>())
                        if (ImGui::Selectable(to_label(o), search_params.order == o))
                            search_params.order = o;

                }

            } // sorting

        } // content

        ButtonHBox buttons;
        buttons.expand = true;
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
