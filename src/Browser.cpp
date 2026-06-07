/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <compare>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <regex>
#include <span>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <SDL_stdinc.h>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include <curlxx/easy.hpp>

#include <glaze/json.hpp>
#include <glaze/exceptions/json_exceptions.hpp>
#include <glaze/core/istream_buffer.hpp>

#include "Browser.hpp"

#include "App.hpp"
#include "cfg.hpp"
#include "Favorites.hpp"
#include "humanize.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
#include "net/address.hpp"
#include "net/resolver.hpp"
#include "Player.hpp"
#include "RadioBrowserAPI.hpp"
#include "rest.hpp"
#include "Station.hpp"
#include "StationDetailsPopup.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using std::cout;
using std::endl;

using namespace std::literals;

using sdl::vec2;


namespace Browser {

    enum class SortOrder : unsigned {
        name_asc,
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
        count
    };

} // namespace Browser


template<>
struct glz::meta<Browser::SortOrder> {
    using enum Browser::SortOrder;
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

namespace Browser {

    using RadioBrowserAPI::ClickResult;
    using RadioBrowserAPI::SearchStationParams;
    using RadioBrowserAPI::ServerStats;
    using RadioBrowserAPI::VoteResult;

    // Persistent state that's saved in browser.json

    struct State {
        struct Filter {
            std::optional<std::string> name    = {};
            std::optional<std::string> tag     = {};
            std::optional<std::string> country = {};
            std::optional<std::string> codec   = {};
        }; // struct StateFilter

        std::optional<Filter> filter;
        std::optional<SortOrder> order;
        std::optional<unsigned> page;
    }; // struct State


    struct Country {
        std::string code;
        std::string name;
    };


    namespace gui {

        std::string filter_name;
        std::string filter_tag;
        std::string filter_country;
        // Country* selected_country;
        std::string filter_codec;
        std::optional<SortOrder> order;
        unsigned page;
        bool options_visible = true;
        bool scroll_to_top = false;

    } // namespace gui


    std::regex tags_regex;

    std::vector<std::shared_ptr<Station>> stations;

    // TODO: allow votes to expire after 10 min.
    std::unordered_map<std::string, VoteResult> votes_cast;


    const std::string server_stats_popup_id = "info";

    std::optional<ServerStats> server_stats_result;
    std::string server_stats_error;

    std::optional<std::vector<Country>> countries;
    std::optional<std::vector<std::string>> codecs;
    std::optional<std::vector<std::string>> tags;


    void
    fetch_codecs();

    void
    fetch_countries();

    void
    fetch_tags();

    void
    load();

    void
    save();


    const std::string&
    by_code(const Country& c);

    const std::string&
    by_name(const Country& c);

    void
    common_error_handler(const std::exception& e);

    void
    fetch_server_stats();


    std::string
    to_label(SortOrder order)
    {
        switch (order) {
            using enum SortOrder;
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

    std::string
    to_label(std::optional<SortOrder> order)
    {
        if (!order)
            return "";
        return to_label(*order);
    }


    std::tuple<SearchStationParams::Order, bool>
    get_order_reverse(SortOrder o)
    {
        switch (o) {
            using enum SortOrder;

            default:
            case name_asc:
                return {SearchStationParams::Order::name, false};
            case name_desc:
                return {SearchStationParams::Order::name, true};
            case country_asc:
                return {SearchStationParams::Order::country, false};
            case country_desc:
                return {SearchStationParams::Order::country, true};
            case language_asc:
                return {SearchStationParams::Order::language, false};
            case language_desc:
                return {SearchStationParams::Order::language, true};
            case clicks_asc:
                return {SearchStationParams::Order::clickcount, false};
            case clicks_desc:
                return {SearchStationParams::Order::clickcount, true};
            case votes_asc:
                return {SearchStationParams::Order::votes, false};
            case votes_desc:
                return {SearchStationParams::Order::votes, true};
            case random:
                return {SearchStationParams::Order::random, false};
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
        cout << "tags_regex has " << counter << " rules" << endl;
        // cout << full_regex << endl;
    }
    catch (std::exception& e) {
        cout << "ERROR: load_tags_regex(): " << e.what() << endl;
    }


    void
    initialize()
    {
        TRACE_FUNC;

        load_tags_regex();
        load();
        // search_stations();
    }


    void
    finalize()
    {
        TRACE_FUNC;

        save();
    }


    void
    load()
    try {
        TRACE_FUNC;

        State state;
        auto filename = App::get_config_path() / "browser.json";
        glz::ex::read_file_json(state, filename.c_str(), std::string{});

        // Transfer state to gui variables.
        if (state.filter) {
            gui::filter_name    = state.filter->name.value_or("");
            gui::filter_tag     = state.filter->tag.value_or("");
            gui::filter_country = state.filter->country.value_or("");
            gui::filter_codec   = state.filter->codec.value_or("");
        } else {
            gui::filter_name.clear();
            gui::filter_tag.clear();
            gui::filter_country.clear();
            gui::filter_codec.clear();
        }
        gui::order = state.order;
        gui::page = state.page.value_or(1u);
    }
    catch (std::exception& e) {
        cout << "ERROR: Browser::load(): " << e.what() << endl;
    }


    void
    save()
    try {
        TRACE_FUNC;

        State state;
        // Transfer gui variables to state.
        State::Filter filter;
        if (!gui::filter_name.empty())
            filter.name = gui::filter_name;
        if (!gui::filter_tag.empty())
            filter.tag = gui::filter_tag;
        if (!gui::filter_country.empty())
            filter.country = gui::filter_country;
        if (!gui::filter_codec.empty())
            filter.codec = gui::filter_codec;
        if (filter.name || filter.tag || filter.country || filter.codec)
            state.filter = std::move(filter);
        state.order = gui::order;
        if (gui::page != 1u)
            state.page = gui::page;

        auto filename = App::get_config_path() / "browser.json";
        glz::ex::write_file_json(state, filename.c_str(), std::string{});
    }
    catch (std::exception& e) {
        cout << "ERROR: Browser::save(): " << e.what() << endl;
    }


    void
    reset_options()
    {
        TRACE_FUNC;

        gui::filter_name.clear();
        gui::filter_tag.clear();
        gui::filter_country.clear();
        gui::order.reset();
        gui::page = 1;
    }


    void
    process_server_stats_popup()
    {
        // ImGui::SetNextWindowSize({550, 0}, ImGuiCond_Always);
        if (ImGui::RAII::Popup server_stats{
                server_stats_popup_id,
                ImGuiWindowFlags_NoSavedSettings
            }) {

            auto server = RadioBrowserAPI::get_server();
            ImGui::SeparatorText("Server status for " + server);

            if (!server_stats_result) {

                if (!server_stats_error.empty())
                    ImGui::TextWrapped("Error: %s", server_stats_error.data());
                else
                    ImGui::Text("Fetching server details...");

            } else {

                if (ImGui::RAII::Table fields_table{
                        "fields",
                        2,
                        ImGuiTableFlags_None
                    }) {
                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    using UI::show_info_row;
                    show_info_row("software_version", server_stats_result->software_version);
                    show_info_row("stations",         server_stats_result->stations);
                    show_info_row("stations_broken",  server_stats_result->stations_broken);
                    show_info_row("tags",             server_stats_result->tags);
                    show_info_row("clicks_last_hour", server_stats_result->clicks_last_hour);
                    show_info_row("clicks_last_day",  server_stats_result->clicks_last_day);
                    show_info_row("languages",        server_stats_result->languages);
                    show_info_row("countries",        server_stats_result->countries);

                }
            }

        }
    }


    void
    show_status()
    {
        if (ImGui::RAII::Child status_child{
                "status",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_NavFlattened
            }) {

            if (ImGui::Button(ICON_FA_REFRESH)) {
                RadioBrowserAPI::set_server(cfg::state.server);
                RadioBrowserAPI::reconnect();
            }

            ImGui::SetItemTooltip("Reconnect to server, or try a different random mirror.");

            ImGui::SameLine();

            auto server = RadioBrowserAPI::get_server();
            if (!server.empty()) {

                if (ImGui::Button(ICON_FA_INFO_CIRCLE)) {
                    fetch_server_stats();
                    ImGui::OpenPopup(server_stats_popup_id);
                }
                ImGui::SetItemTooltip("Show server details.");

                process_server_stats_popup();

                ImGui::SameLine();

                ImGui::Text("%s", server.data());

            }

        }
    }


    void
    show_options()
    {
        if (ImGui::RAII::Child options_child{
                "options",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened
            }) {

            ImGui::SetNextItemOpen(gui::options_visible);
            if ((gui::options_visible = ImGui::CollapsingHeader("Options"))) {

                ImGui::Indent();

                if (ImGui::RAII::Child filters_guard{
                        "filters",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeX |
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_FrameStyle |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    ImGui::RAII::ItemWidth filters_width{500};

                    ImGui::TextUnformatted(ICON_FA_FILTER " Filters");

                    /*******************
                     * Filter by name. *
                     *******************/
                    ImGui::InputText("Name", gui::filter_name);

                    /******************
                     * Filter by tag. *
                     ******************/
                    ImGui::SetNextWindowSizeConstraints({0, 0},
                                                        {1200.0f, FLT_MAX});
                    if (ImGui::RAII::Combo tag_combo{
                            "Tag",
                            gui::filter_tag,
                            ImGuiComboFlags_HeightLargest
                        }) {
                        static ImGuiTextFilter text_filter{gui::filter_tag.data()};
                        if (ImGui::IsWindowAppearing()) {
                            ImGui::SetKeyboardFocusHere();
                            SDL_strlcpy(text_filter.InputBuf,
                                        gui::filter_tag.data(),
                                        sizeof text_filter.InputBuf);
                            text_filter.Build();
                        }
                        text_filter.Draw("##tag", 900);
                        // Add empty entry for removing filter.
                        if (ImGui::Selectable("##", gui::filter_tag.empty()))
                            gui::filter_tag.clear();
                        // The rest of tags.
                        if (!tags)
                            fetch_tags();
                        for (auto& tag : *tags) {
                            const bool is_selected = gui::filter_tag == tag;
                            if (text_filter.PassFilter(tag.data()))
                                if (ImGui::Selectable(tag, is_selected))
                                    gui::filter_tag = tag;
                        }
                    }

                    /**********************
                     * Filter by country. *
                     **********************/
                    if (ImGui::RAII::Combo country_combo{
                            "Country",
                            gui::filter_country,
                            ImGuiComboFlags_HeightLargest
                        }) {
                        static ImGuiTextFilter text_filter{gui::filter_country.data()};
                        if (ImGui::IsWindowAppearing()) {
                            ImGui::SetKeyboardFocusHere();
                            SDL_strlcpy(text_filter.InputBuf,
                                        gui::filter_country.data(),
                                        sizeof text_filter.InputBuf);
                            text_filter.Build();
                        }
                        text_filter.Draw("##country");
                        // Add empty entry for removing filter.
                        if (ImGui::Selectable("##", gui::filter_country.empty()))
                            gui::filter_country.clear();
                        // The rest of countries
                        if (!countries)
                            fetch_countries();
                        for (const auto& [code, name] : *countries) {
                            const bool is_selected = gui::filter_country == code;
                            auto label = code + " - " + name;
                            if (text_filter.PassFilter(label.data()))
                                if (ImGui::Selectable(label, is_selected))
                                    gui::filter_country = code;
                        }
                    }

                    // TODO: add language filter

                    /********************
                     * Filter by codec. *
                     ********************/
                    if (ImGui::RAII::Combo codec_combo{
                            "Codec",
                            gui::filter_codec
                        }) {
                        // Add empty entry for removing filter.
                        if (ImGui::Selectable("##", gui::filter_codec.empty()))
                            gui::filter_codec = "";
                        // The rest of codecs.
                        if (!codecs)
                            fetch_codecs();
                        for (const auto& codec : *codecs)
                            if (ImGui::Selectable(codec, gui::filter_codec == codec))
                                gui::filter_codec = codec;
                    }

                } // filters

                ImGui::SameLine();

                if (ImGui::RAII::Child sorting_child{
                        "sorting",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeX |
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_FrameStyle |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    ImGui::TextUnformatted(ICON_FA_SORT " Order");

                    ImGui::SetNextItemWidth(280);
                    if (ImGui::RAII::Combo order_combo{
                            "##order",
                            to_label(gui::order),
                            ImGuiComboFlags_HeightLargest}) {
                        if (ImGui::Selectable("##", !gui::order))
                            gui::order.reset();
                        for (unsigned i = 0; i < static_cast<unsigned>(SortOrder::count); ++i) {
                            SortOrder o{i};
                            if (ImGui::Selectable(to_label(o),
                                                  gui::order && *gui::order == o))
                                gui::order = o;
                        }
                    }

                } // sorting

                ImGui::SameLine();

                if (ImGui::RAII::Child buttons{
                        "buttons",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeX |
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    if (ImGui::Button("Reset"))
                        reset_options();
                    ImGui::SetItemTooltip("Reset browser options to default.");

                    if (ImGui::Button("Search")) {
                        search_stations();
                    }
                    ImGui::SetItemTooltip("Search with the selected options.");

                } // buttons

                ImGui::Unindent();

            }

        } // options_child
    }


    void
    show_navigation()
    {
        const float parent_width = ImGui::GetContentRegionAvail().x;
        const ImVec2 global_pos = ImGui::GetCursorScreenPos();
        ImGui::SetNextWindowPos({global_pos.x + parent_width / 2.0f, global_pos.y + 0.0f},
                                ImGuiCond_Always,
                                {0.5f, 0.0f});
        if (ImGui::RAII::Child navigation_child{
                "navigation",
                {0, 0},
                ImGuiChildFlags_AutoResizeX |
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_NavFlattened
            }) {

            const bool is_first_page = gui::page == 1;
            const bool is_last_page = stations.size() < cfg::state.browser_page_limit;
            const bool is_searching = RadioBrowserAPI::is_searching();

            {
                ImGui::RAII::Disabled disable_first_page{is_first_page};

                // 100⏪
                if (ImGui::Button("100" ICON_FA_ANGLE_DOUBLE_LEFT) && !is_searching) {
                    if (gui::page > 100)
                        gui::page -= 100;
                    else
                        gui::page = 1;
                    search_stations();
                }
                ImGui::SetItemTooltip("Go back 100 pages.");

                ImGui::SameLine();

                // 10⏪
                if (ImGui::Button("10" ICON_FA_ANGLE_DOUBLE_LEFT) && !is_searching) {
                    if (gui::page > 10)
                        gui::page -= 10;
                    else
                        gui::page = 1;
                    search_stations();
                }
                ImGui::SetItemTooltip("Go back 10 pages.");

                ImGui::SameLine();

                // ⏴
                if (ImGui::Button(" " ICON_FA_ANGLE_LEFT " ") && !is_searching) {
                    if (gui::page > 1)
                        --gui::page;
                    search_stations();
                }
                ImGui::SetItemTooltip("Go back one page.");
            }

            ImGui::SameLine();

            const float page_width = 200;
            ImGui::SetNextItemWidth(page_width);
            unsigned max_page_num = UINT_MAX;
            if (is_last_page)
                max_page_num = gui::page;
            ImGui::Drag<unsigned>("##page"s, gui::page, 0.05f, 1u, max_page_num);
            if (ImGui::IsItemDeactivatedAfterEdit())
                search_stations();

            ImGui::SameLine();

            {
                ImGui::RAII::Disabled disable_last_page{is_last_page};

                // ⏵
                if (ImGui::Button(" " ICON_FA_ANGLE_RIGHT " ") && !is_searching) {
                    ++gui::page;
                    search_stations();
                }
                ImGui::SetItemTooltip("Advance one page.");

                ImGui::SameLine();

                // ⏩10
                if (ImGui::Button(ICON_FA_ANGLE_DOUBLE_RIGHT "10") && !is_searching) {
                    gui::page += 10;
                    search_stations();
                }
                ImGui::SetItemTooltip("Advance 10 pages.");

                ImGui::SameLine();

                // ⏩100
                if (ImGui::Button(ICON_FA_ANGLE_DOUBLE_RIGHT "100") && !is_searching) {
                    gui::page += 100;
                    search_stations();
                }
                ImGui::SetItemTooltip("Advance 100 pages.");
            }

        }

    } // navigation_child


    void
    show_station(std::shared_ptr<Station>& station)
    {
        ImGui::RAII::ID station_id{station.get()};

        if (ImGui::RAII::Child station_child{
                "station",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened
            }) {

            if (ImGui::RAII::Child actions_child{
                    "actions",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeX |
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened
                }) {

                UI::show_play_button(station);

                UI::show_favorite_button(*station);

                ImGui::SameLine();

                if (StationDetailsPopup::show_button(station->stationuuid))
                    StationDetailsPopup::open(station->stationuuid);

                auto vote_record = votes_cast.find(station->stationuuid);
                const bool voted = vote_record != votes_cast.end();
                bool ok = voted ? vote_record->second.ok : false;
                std::string vote_label = (ok
                                          ? ICON_FA_THUMBS_UP " "
                                          : ICON_FA_THUMBS_O_UP " ")
                                         + humanize::value(station->votes);

                {
                    ImGui::RAII::Disabled disable_voting{voted || !cfg::state.send_clicks};
                    if (ImGui::Button(vote_label))
                        send_vote(station);
                    if (voted)
                        ImGui::SetItemTooltip("%s", vote_record->second.message.data());
                    else
                        ImGui::SetItemTooltip("Vote for this station.");
                }

            } // actions_child

            ImGui::SameLine();

            if (ImGui::RAII::Child details_child{
                    "details",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened
                }) {

                UI::show_favicon(*station);

                ImGui::SameLine();

                UI::show_station_basic_info(*station);

                if (ImGui::RAII::Child extra_info_child{
                        "extra_info",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    char click_text[32];
                    std::snprintf(click_text, sizeof click_text,
                                  ICON_FA_BAR_CHART " %" PRIu64 " (%+d)",
                                  station->click_count,
                                  station->click_trend);
                    UI::show_boxed(click_text,
                                   "Daily total clicks and trend.");

                    if (station->bitrate) {
                        ImGui::SameLine();
                        UI::show_boxed(ICON_FA_HEADPHONES " "
                                       + std::to_string(station->bitrate) + " kbps",
                                       "The advertised stream quality.");
                    }

                    ImGui::SameLine();

                    UI::show_boxed(ICON_FA_FLASK " " + station->codec,
                                   "The codec used in this broadcast.");

                    UI::show_tags(station->tags);

                } // extra_info_child

            } // details_child

        } // station_child
    }


    void
    process_ui()
    {
        ImGui::RAII::Disabled disable_searching{RadioBrowserAPI::is_searching()};

        show_status();

        show_options();

        show_navigation();

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::RAII::Child stations_child{"stations"}) {

            if (gui::scroll_to_top) {
                ImGui::SetScrollY(0);
                gui::scroll_to_top = false;
            }

#if 0
            // Disabled until ImGui fixes navigation.
            const float content_width = ImGui::GetContentRegionAvail().x;
            if (page_index > 0)
                if (ImGui::Button("⏴ Go to page " + std::to_string(page_index - 1 + 1),
                                  {content_width, 0.0f})) {
                    --page_index;
                    reload_stations();
                }
#endif

            for (auto& station_ptr : stations)
                show_station(station_ptr);

#if 0
            // Disabled until ImGui fixes navigation.
            if (stations.size() == cfg::state.browser_page_limit)
                if (ImGui::Button("Go to page " + std::to_string(page_index + 1 + 1) + " ⏵",
                                  {content_width, 0.0f})) {
                    ++page_index;
                    reload_stations();
                }
#endif
        } // stations_child

        StationDetailsPopup::process_ui();
    }


    void
    send_click(std::shared_ptr<Station>& station_ptr)
    {
        TRACE_FUNC;

        if (!station_ptr || station_ptr->stationuuid.empty())
            return;

        RadioBrowserAPI::send_click(
            station_ptr->stationuuid,
            [station_ptr](ClickResult /*result*/)
            {
                update_station(station_ptr);
            },
            common_error_handler);
    }


    void
    send_vote(std::shared_ptr<Station>& station_ptr)
    {
        TRACE_FUNC;

        if (!station_ptr || station_ptr->stationuuid.empty())
            return;

        // TODO: this is causing errors, investigate why

        RadioBrowserAPI::send_vote(
            station_ptr->stationuuid,
            [station_ptr](VoteResult /*result*/)
            {
                update_station(station_ptr);
            },
            common_error_handler);
    }


    // TODO: should display errors to the user
    void
    common_error_handler(const std::exception& e)
    {
        // TODO: should show a notification-like message, that goes away after a while.
        cout << "Browser ERROR: " << e.what() << '\n';
        if (auto ee = dynamic_cast<const rest::error*>(&e)) {
            cout << "Content-Type: " << ee->content_type << '\n';
            cout << "<response>\n" << ee->response << "\n</response>\n";
        }
        cout << std::flush;
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
                cout << "Got " << codecs->size() << " codecs" << endl;
            },
            common_error_handler);
    }


    void
    fetch_countries()
    {
        TRACE_FUNC;

        // TODO: when RB errors out, it should be possible to try again
        if (countries)
            return;

        countries.emplace();

        RadioBrowserAPI::CountryParams params;
        params.order = RadioBrowserAPI::CountryParams::Order::name;
        params.hidebroken = true;
        params.limit = 1000;

        RadioBrowserAPI::get_countries(
            {},
            [](RadioBrowserAPI::CountryVec rb_countries)
            {
                for (auto& [name, code, count] : rb_countries)
                    countries->emplace_back(std::move(code),
                                            std::move(name));
                cout << "Got " << countries->size()
                     << " countries" << endl;
                std::ranges::sort(*countries, {}, by_code);
            },
            common_error_handler);
    }


    void
    fetch_server_stats()
    {
        TRACE_FUNC;

        server_stats_result.reset();
        server_stats_error.clear();

        RadioBrowserAPI::get_server_stats(
            [](ServerStats stats)
            {
                server_stats_result = std::move(stats);
            },
            [](const std::exception& e)
            {
                server_stats_error = e.what();
            });
    }


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
                        // cout << "Ignored tag: " << name << endl;
                        // cout << "RE match: " << matches.str() << endl;
                        continue;
                    }
                    tags->push_back(std::move(name));
                }
                cout << "Got " << tags->size() << " tags" << endl;
            },
            common_error_handler);
    }


    void
    update_station(std::shared_ptr<Station> station_ptr)
    {
        TRACE_FUNC;

        if (!station_ptr || station_ptr->stationuuid.empty())
            return;

        RadioBrowserAPI::get_station(
            station_ptr->stationuuid,
            [station_ptr = std::move(station_ptr)](RadioBrowserAPI::Station rb_station)
                mutable
            {
                *station_ptr = Station::from_radio_browser(rb_station);
            },
            common_error_handler);
    }


    void
    search_stations()
    {
        TRACE_FUNC;

        gui::options_visible = false;
        gui::scroll_to_top = true;

        RadioBrowserAPI::SearchStationParams params;
        params.offset = (gui::page - 1u) * cfg::state.browser_page_limit;
        params.limit = cfg::state.browser_page_limit;
        params.hidebroken = true;

        if (gui::order) {
            auto [order, reverse] = get_order_reverse(*gui::order);
            params.order = order;
            params.reverse = reverse;
        }

        if (!gui::filter_name.empty())
            params.name = gui::filter_name;
        if (!gui::filter_tag.empty())
            params.tag = gui::filter_tag;
        if (!gui::filter_country.empty())
            params.countrycode = gui::filter_country;
        if (!gui::filter_codec.empty())
            params.codec = gui::filter_codec;

        RadioBrowserAPI::search_stations(
            params,
            [](RadioBrowserAPI::StationVec rb_stations)
            {
                cout << "Received " << rb_stations.size() << " stations" << endl;
                stations.clear();
                for (auto& st : rb_stations) {
                    // ensure the page size limit is respected
                    if (stations.size() >= cfg::state.browser_page_limit)
                        break;
                    stations.push_back(std::make_shared<Station>(Station::from_radio_browser(st)));
                }
            },
            common_error_handler);
    }


    std::string
    get_country_name(const std::string& code)
    {
        if (code.empty())
            return {};

        // TODO: when RB errors out, it should be possible to try again
        if (!countries) {
            fetch_countries();
            return {};
        }

        auto it = std::ranges::lower_bound(*countries, code, {}, by_code);
        if (it == countries->end())
            return {};
        if (it->code != code)
            return {};
        return it->name;
    }


    const std::string&
    by_code(const Country& c)
    {
        return c.code;
    }


    const std::string&
    by_name(const Country& c)
    {
        return c.name;
    }

} // namespace Browser
