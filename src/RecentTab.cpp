/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <deque>
#include <optional>
#include <utility>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "RecentTab.hpp"

#include "App.hpp"
#include "cfg.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "Serializer.hpp"
#include "StationDetailsPopup.hpp"
#include "StationGlaze.hpp"
#include "tracer.hpp"
#include "UI.hpp"

// TODO: process add and remove using future

namespace RecentTab {

    std::deque<ConstStationPtr> stations;


    namespace {

        StationPtr pending_add;
        std::optional<std::size_t> pending_remove;

    } // namespace


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
    save()
    try {
        auto filename = App::get_config_path() / "recent.json";
        Serializer::save(stations, filename);
    }
    catch (std::exception& e) {
        LOG_ERROR("{}", e.what());
    }


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
    show_station(ConstStationPtr& station,
                 std::size_t index)
    {
        using namespace ImGui::RAII;

        ID station_id{static_cast<int>(index)};

        if (Child station_child{
                "station",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened
            }) {

            if (Child actions_child{
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

                if (ImGui::Button(ICON_FA_TRASH_O)) // 🗑
                    pending_remove = index;
                ImGui::SetItemTooltip("Remove station from recent history.");

            } // actions_child

            ImGui::SameLine();

            if (Child details_child{
                    "details",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened
                }) {

                UI::show_favicon(*station);

                ImGui::SameLine();

                UI::show_station_basic_info(*station);

                if (Child extra_info_child{
                        "extra_info",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    UI::show_tags(station->tags);

                }

            }

        }
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

            if (ImGui::Button("Clear"))
                stations.clear();
            ImGui::SetItemTooltip("Clear entire recent history.");

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::FormatTextAligned(1, -1, "{} stations", stations.size());
        }

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (Child recent_child{"recent"})
            for (std::size_t index = stations.size() - 1; index + 1 > 0; --index)
                show_station(stations[index], index);

        StationDetailsPopup::process_ui();
    }


    void
    process_add()
    {
        if (!pending_add)
            return;
        if (!stations.empty() && *pending_add == *stations.back())
            return;
        stations.push_back(std::move(pending_add));
    }


    void
    process_remove()
    {
        // Handle any pending removal
        if (!pending_remove)
            return;

        std::size_t index = *pending_remove;
        if (index < stations.size())
            stations.erase(stations.begin() + index);
        pending_remove.reset();
    }


    void
    remove_excess()
    {
        if (stations.size() > cfg::state.recent_limit) {
            std::size_t pending_remove = stations.size() - cfg::state.recent_limit;
            stations.erase(stations.begin(),
                           stations.begin() + pending_remove);
        }
    }


    void
    process_logic()
    {
        process_add();
        process_remove();
        remove_excess();
    }


    void
    queue_add(ConstStationPtr& station)
    {
        pending_add = station;
    }

} // namespace RecentTab
