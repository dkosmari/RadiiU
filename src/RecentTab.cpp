/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <deque>
#include <optional>
#include <queue>
#include <ranges>
#include <utility>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "RecentTab.hpp"

#include "App.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "Serializer.hpp"
#include "Settings.hpp"
#include "StationDetailsPopup.hpp"
#include "StationGlaze.hpp"
#include "string_utils.hpp"
#include "task_queue.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using namespace std::literals;
using Settings::cfg;


namespace RecentTab {

    namespace {

        /*-----------*/
        /* Variables */
        /*-----------*/

        std::deque<ConstStationPtr> stations;
        task_queue pending_tasks;
        std::string name_filter;
        std::string tag_filter;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        load();

        void
        real_add(ConstStationPtr station);

        void
        real_remove(ConstStationPtr station);

        void
        remove_excess();

        void
        save();

        void
        show_station(std::size_t index,
                     ConstStationPtr& station);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        load()
        try {
            stations.clear();
            auto filename = App::get_config_path() / "recent.json";
            Serializer::load(stations, filename);
        }
        catch (std::exception& e) {
            LOG_ERROR("{}", e.what());
        }


        void
        real_add(ConstStationPtr station)
        {
            stations.push_back(std::move(station));
        }


        void
        real_remove(ConstStationPtr station)
        {
            std::erase(stations, station);
        }


        void
        remove_excess()
        {
            if (stations.size() > cfg.recent_limit) {
                std::size_t excess = stations.size() - cfg.recent_limit;
                stations.erase(stations.begin(),
                               stations.begin() + excess);
            }
        }


        void
        save()
        try {
            auto filename = App::get_config_path() / "recent.json";
            Serializer::save(stations, filename);
        }
        catch (std::exception& e) {
            LOG_ERROR("{}", e.what());
        }


        void
        show_station(std::size_t index,
                     ConstStationPtr& station)
        {
            using namespace ImGui::RAII;

            ID station_id{static_cast<int>(index)};

            if (Child station_frame{
                    "station",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_FrameStyle |
                    ImGuiChildFlags_NavFlattened
                }) {

                if (Child actions_frame{
                        "actions",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeX |
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    UI::PlayButton(station);

                    UI::FavoriteButton(*station);

                    ImGui::SameLine();

                    if (StationDetailsPopup::Button(station->stationuuid))
                        StationDetailsPopup::open(station->stationuuid);

                    if (ImGui::Button(ICON_FA_TRASH_O, UI::get_small_button_size())) // 🗑
                        pending_tasks.add(real_remove, station);
                    ImGui::SetItemTooltip("Remove station from recent history.");

                } // actions_frame

                ImGui::SameLine();

                if (Child details{
                        "details",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    UI::StationInfo(*station, true);

                }

            } // station_frame
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize()
    {
        TRACE_FUNC;
        load();
    }


    void
    finalize()
    {
        TRACE_FUNC;
        save();
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        if (Child toolbar_child{
                "toolbar",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_NavFlattened
            }) {

            if (ImGui::Button(ICON_FA_ERASER " Clear"))
                stations.clear();
            ImGui::SetItemTooltip("Clear entire recent history.");

            ImGui::SameLine();

            ImGui::SetNextItemWidth(400);
            ImGui::InputTextWithHint("##name_filter"s, "Filter by name..."s, name_filter);

            ImGui::SameLine();

            ImGui::SetNextItemWidth(400);
            ImGui::InputTextWithHint("##tag_filter"s, "Filter by tag..."s, tag_filter);

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::FormatTextAligned(1, -1, "{} stations", stations.size());
        }

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (Child list{"recent"})
            for (auto [index, station] : stations | std::views::enumerate) {
                using string_utils::to_upper;
                if (!name_filter.empty())
                    if (!to_upper(station->name).contains(to_upper(name_filter)))
                        continue;

                if (!tag_filter.empty()) {
                    bool match = false;
                    for (const auto& tag : station->tags)
                        if (to_upper(tag).contains(to_upper(tag_filter))) {
                            match = true;
                            break;
                        }
                    if (!match)
                        continue;
                }

                show_station(index, station);
            }

        StationDetailsPopup::process_ui();
    }


    void
    process_logic()
    {
        try {
            pending_tasks.dispatch_all();
        }
        catch (std::exception& e) {
            LOG_ERROR("Dispatching RecentTab tasks: {}", e.what());
        }

        remove_excess();
    }


    void
    add(ConstStationPtr station)
    {
        pending_tasks.add(real_add, std::move(station));
    }

} // namespace RecentTab
