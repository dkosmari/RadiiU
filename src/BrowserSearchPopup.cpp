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

// #include <glaze/core/meta.hpp>

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


using namespace std::literals;


#if 0
template<>
struct glz::meta<BrowserSearchPopup::Order> {
    using enum BrowserSearchPopup::Order;
    static constexpr
    auto value = enumerate(name_asc,
                           name_desc,
                           country_asc,
                           country_desc,
                           language_asc,
                           language_desc,
                           votes_asc,
                           votes_desc,
                           clicks_asc,
                           clicks_desc,
                           random,
                           count);
};
#endif


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
        make_country_label(const std::string& code,
                           const std::string& name);

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
        make_country_label(const std::string& code,
                           const std::string& name)
        {
            return CountryManager::get_utf8(code)
                + " "s
                + name
                + " ("s
                + code
                + ")"s;
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


        ImGui::SetNextWindowSize({1100, 600}, ImGuiCond_Always);
        PopupModal search_modal{popup_id,
                                nullptr,
                                ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoMove};
        if (!search_modal) {
            state = State::hidden;
            confirm_func = {};
            return;
        }

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

        if (Child content{"content",
                          {0, - buttons.get_height_with_spacing()},
                          ImGuiChildFlags_NavFlattened,
                          ImGuiWindowFlags_NoSavedSettings}) {

            if (Child filters_group{"filters_group",
                                    {0, 0},
                                    ImGuiChildFlags_AutoResizeX |
                                    ImGuiChildFlags_AutoResizeY |
                                    ImGuiChildFlags_Borders |
                                    ImGuiChildFlags_NavFlattened}) {

                auto& filter = search_params.filter;

                ItemWidth filters_width{500};

                ImGui::TextUnformatted(ICON_FA_FILTER " Filters");

                /*-----------------*/
                /* Filter by name. */
                /*-----------------*/
                ImGui::InputText("Name", filter.name);

                /*----------------*/
                /* Filter by tag. */
                /*----------------*/
                if (Combo tag_combo{"Tag",
                                    filter.tag,
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
                            auto label = ICON_FA_TAG " " + tag;
                            if (ImGui::Selectable(label, is_selected)) {
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
                std::string filter_country_label;
                std::string filter_country_name = CountryManager::get_name(filter.country);
                if (!filter_country_name.empty())
                    filter_country_label = make_country_label(filter.country,
                                                              filter_country_name);
                else
                    filter_country_label = filter.country;
                if (Combo country_combo{"Country",
                                        filter_country_label,
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
                if (Combo codec_combo{"Codec",
                                      filter.codec,
                                      ImGuiComboFlags_HeightLarge}) {

                    if (ImGui::Selectable("(any codec)", filter.codec.empty()))
                        filter.codec = "";

                    // The rest of codecs.
                    if (!codecs)
                        fetch_codecs();

                    for (const auto& codec : *codecs)
                        if (ImGui::Selectable(codec, filter.codec == codec))
                            filter.codec = codec;
                }

            }

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

        buttons.show();

    }

} // namespace BrowserSearchPopup
