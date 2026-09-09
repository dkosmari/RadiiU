/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <exception>
#include <filesystem>
#include <limits>
#include <utility>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "BrowserTab.hpp"

#include "App.hpp"
#include "BrowserSearchPopup.hpp"
#include "ButtonHBox.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "rest.hpp"
#include "ServerStatsPopup.hpp"
#include "Settings.hpp"
#include "StationDetailsPopup.hpp"
#include "StationVoting.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using namespace std::literals;

using sdl::vec2;

using Settings::cfg;


namespace BrowserTab {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        struct OrderAndDir {
            RadioBrowserAPI::SearchStationParams::Order orer;
            bool reverse;
        };


        /*---------*/
        /* Aliases */
        /*---------*/

        using BrowserSearchPopup::SearchParams;


        /*-----------*/
        /* Variables */
        /*-----------*/

        std::vector<StationPtr> stations;
        unsigned page = 1;
        bool scroll_to_top = false;
        SearchParams search_params;
        std::string error_message;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        action_start_search(const SearchParams& params);

        void
        common_error_handler(const std::exception& e);

        OrderAndDir
        get_order_dir(BrowserSearchPopup::Order o);

        void
        show_empty();

        void
        show_error_message();

        void
        show_navigation();

        void
        show_station(StationPtr& station);

        void
        show_stations();

        void
        show_toolbar();


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        action_start_search(const SearchParams& params)
        {
            TRACE_FUNC;

            search_params = params;
            page = 1;
            perform_search();
        }



        void
        common_error_handler(const std::exception& e)
        {
            LOG_ERROR("{}", e.what());
            if (auto ee = dynamic_cast<const rest::error*>(&e)) {
                LOG_ERROR("Content-Type: {}", ee->content_type);
                LOG_ERROR("<content>\n{}\n</content>", ee->content);
            }

            error_message = e.what();
        }


        OrderAndDir
        get_order_dir(BrowserSearchPopup::Order o)
        {
            using RBOrder = RadioBrowserAPI::SearchStationParams::Order;

            switch (o) {
                using enum BrowserSearchPopup::Order;

                default:
                case name_asc:
                    return {RBOrder::name, false};
                case name_desc:
                    return {RBOrder::name, true};
                case country_asc:
                    return {RBOrder::country, false};
                case country_desc:
                    return {RBOrder::country, true};
                case language_asc:
                    return {RBOrder::language, false};
                case language_desc:
                    return {RBOrder::language, true};
                case clicks_asc:
                    return {RBOrder::clickcount, false};
                case clicks_desc:
                    return {RBOrder::clickcount, true};
                case votes_asc:
                    return {RBOrder::votes, false};
                case votes_desc:
                    return {RBOrder::votes, true};
                case random:
                    return {RBOrder::random, false};
            }
        }


        void
        show_empty()
        {
            using namespace ImGui::RAII;

            if (Child content{"content",
                              {0, 0},
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoSavedSettings}) {

                ImGui::TextAligned(0.5f, -1, "Use the search button to find stations.");

                ButtonHBox buttons;
                buttons.add(
                    ICON_FA_BINOCULARS " Search...",
                    true,
                    []
                    {
                        BrowserSearchPopup::open(action_start_search);
                    }
                );
                buttons.show();
            }
        }


        void
        show_error_message()
        {
            using namespace ImGui::RAII;

            if (Child content{"content",
                              {0, 0},
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoSavedSettings}) {

                ImGui::Text("ERROR!");

                ImGui::TextWrapped(error_message);

            } // content
        }


        void
        show_navigation()
        {
            using namespace ImGui::RAII;

            const float parent_width = ImGui::GetContentRegionAvail().x;
            const ImVec2 global_pos = ImGui::GetCursorScreenPos();
            ImGui::SetNextWindowPos({global_pos.x + parent_width / 2.0f, global_pos.y + 0.0f},
                                    ImGuiCond_Always,
                                    {0.5f, 0.0f});
            if (Child navigation{
                    "navigation",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeX |
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_NoSavedSettings
                }) {

                const bool is_first_page = page == 1;
                const bool is_last_page = stations.size() < cfg.browser_page_limit;
                const bool is_busy = RadioBrowserAPI::is_busy();

                {
                    Disabled disable_first_page{is_first_page};

                    // 100⏪
                    if (ImGui::Button("100" ICON_FA_ANGLE_DOUBLE_LEFT) && !is_busy) {
                        if (page > 100)
                            page -= 100;
                        else
                            page = 1;
                        perform_search();
                    }
                    ImGui::SetItemTooltip("Go back 100 pages.");

                    ImGui::SameLine();

                    // 10⏪
                    if (ImGui::Button("10" ICON_FA_ANGLE_DOUBLE_LEFT) && !is_busy) {
                        if (page > 10)
                            page -= 10;
                        else
                            page = 1;
                        perform_search();
                    }
                    ImGui::SetItemTooltip("Go back 10 pages.");

                    ImGui::SameLine();

                    // ⏴
                    if (ImGui::Button(" " ICON_FA_ANGLE_LEFT " ") && !is_busy) {
                        if (page > 1)
                            --page;
                        perform_search();
                    }
                    ImGui::SetItemTooltip("Go back one page.");
                }

                ImGui::SameLine();

                const float page_width = 200;
                ImGui::SetNextItemWidth(page_width);
                unsigned max_page_num = std::numeric_limits<unsigned>::max();
                if (is_last_page)
                    max_page_num = page;
                ImGui::Drag<unsigned>("##page"s, page, 0.05f, 1u, max_page_num);
                if (ImGui::IsItemDeactivatedAfterEdit())
                    perform_search();

                ImGui::SameLine();

                {
                    Disabled disable_last_page{is_last_page};

                    // ⏵
                    if (ImGui::Button(" " ICON_FA_ANGLE_RIGHT " ") && !is_busy) {
                        ++page;
                        perform_search();
                    }
                    ImGui::SetItemTooltip("Advance one page.");

                    ImGui::SameLine();

                    // ⏩10
                    if (ImGui::Button(ICON_FA_ANGLE_DOUBLE_RIGHT "10") && !is_busy) {
                        page += 10;
                        perform_search();
                    }
                    ImGui::SetItemTooltip("Advance 10 pages.");

                    ImGui::SameLine();

                    // ⏩100
                    if (ImGui::Button(ICON_FA_ANGLE_DOUBLE_RIGHT "100") && !is_busy) {
                        page += 100;
                        perform_search();
                    }
                    ImGui::SetItemTooltip("Advance 100 pages.");
                }

            } // navigation
        }


        void
        show_station(StationPtr& station)
        {
            using namespace ImGui::RAII;

            ID station_id{station.get()};

            if (Child station_frame{
                    "station",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_FrameStyle |
                    ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_NoSavedSettings
                }) {

                if (Child actions{
                        "actions",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeX |
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened,
                        ImGuiWindowFlags_NoSavedSettings
                    }) {

                    UI::PlayButton(station);

                    UI::FavoriteButton(*station);

                    ImGui::SameLine();

                    if (StationDetailsPopup::Button(station->stationuuid))
                        StationDetailsPopup::open(station->stationuuid);

                    StationVoting::Button(station);

                } // actions

                ImGui::SameLine();

                if (Child details{
                        "details",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened,
                        ImGuiWindowFlags_NoSavedSettings
                    }) {

                    UI::StationInfo(*station, true);

                } // details

            } // station_frame
        }


        void
        show_stations()
        {
            using namespace ImGui::RAII;

            // Note: flat navigation doesn't work well on child windows that scroll.
            if (Child stations_list{
                    "stations_list",
                    {0, 0},
                    ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoSavedSettings
                }) {

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

                for (auto& station : stations)
                    show_station(station);

#if 0
                // Disabled until ImGui fixes navigation.
                if (stations.size() == cfg.browser_page_limit)
                    if (ImGui::Button("Go to page " + std::to_string(page_index + 1 + 1) + " ⏵",
                                      {content_width, 0.0f})) {
                        ++page_index;
                        reload_stations();
                    }
#endif

                if (scroll_to_top) {
                    scroll_to_top = false;
                    UI::SmoothScroll(-1, 0);
                }
                UI::DoSmoothScroll();

            } // stations_list
        }


        void
        show_toolbar()
        {
            using namespace ImGui::RAII;

            if (Child toolbar{
                    "toolbar",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_NoSavedSettings
                }) {

                Disabled if_busy{RadioBrowserAPI::is_busy()};

                if (ImGui::Button(ICON_FA_BINOCULARS " Search..."))
                    BrowserSearchPopup::open(action_start_search);

                ImGui::SameLine();

                ImGui::FormatText("Server: {}",
                                  cfg.server.empty() ? "(random)"s : cfg.server);
                if (cfg.server.empty()) {
                    std::string current_server = RadioBrowserAPI::get_server();
                    ImGui::SetItemTooltip(current_server);
                }

                ImGui::SameLine();

                {
                    Disabled if_preferred_server{!cfg.server.empty()};
                    if (ImGui::Button(ICON_FA_REFRESH))
                        RadioBrowserAPI::update_mirrors_and_select_random();
                    ImGui::SetItemTooltip("Switch to random mirror.");
                }

                ImGui::SameLine();

                if (ImGui::Button(ICON_FA_INFO_CIRCLE))
                    ServerStatsPopup::open();
                ImGui::SetItemTooltip("Show server details.");

            }
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize()
    {
        TRACE_FUNC;

        error_message.clear();

        BrowserSearchPopup::initialize();

        // Changed after 0.3.0: no more browser.json
        auto browser_json = App::get_config_path() / "browser.json";
        try {
            if (exists(browser_json))
                remove(browser_json);
        }
        catch (std::exception& e) {
            LOG_ERROR("Failed to remove {:?}: {}",
                      browser_json.string(),
                      e.what());
        }
    }


    void
    finalize()
    {
        TRACE_FUNC;

        BrowserSearchPopup::finalize();

        error_message.clear();
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        Disabled if_busy{RadioBrowserAPI::is_busy()};

        show_toolbar();

        ImGui::Separator();

        if (!error_message.empty()) {
            show_error_message();
        } else if (stations.empty()) {
            show_empty();
        } else {
            show_navigation();
            show_stations();
        }

        BrowserSearchPopup::process_ui();
        StationDetailsPopup::process_ui();
        ServerStatsPopup::process_ui();
    }


    void
    perform_search()
    {
        TRACE_FUNC;

        error_message.clear();

        scroll_to_top = true;

        RadioBrowserAPI::SearchStationParams params;
        params.offset = (page - 1u) * cfg.browser_page_limit;
        params.limit = cfg.browser_page_limit;
        params.hidebroken = true;

        auto [order, reverse] = get_order_dir(search_params.order);
        params.order = order;
        params.reverse = reverse;

        const auto& filter = search_params.filter;
        if (!filter.name.empty())
            params.name = filter.name;
        if (!filter.tag.empty())
            params.tag = filter.tag;
        if (!filter.country.empty())
            params.countrycode = filter.country;
        if (!filter.codec.empty())
            params.codec = filter.codec;

        RadioBrowserAPI::search_stations(
            params,
            [](RadioBrowserAPI::StationVec rb_stations)
            {
                LOG_INFO("Received {} stations.", rb_stations.size());
                stations.clear();
                for (auto& st : rb_stations) {
                    // ensure the page size limit is respected
                    if (stations.size() >= cfg.browser_page_limit)
                        break;
                    stations.push_back(
                        std::make_shared<Station>(Station::from_radio_browser(st))
                    );
                }
            },
            common_error_handler
        );
    }

} // namespace BrowserTab
