/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "FavoritesTab.hpp"

#include "App.hpp"
#include "ConfirmDeleteStationPopup.hpp"
#include "EditStationPopup.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "Serializer.hpp"
#include "Station.hpp"
#include "StationGlaze.hpp"
#include "string_utils.hpp"
#include "task_queue.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using namespace std::literals;


namespace FavoritesTab {

    namespace {

        /*-----------*/
        /* Variables */
        /*-----------*/

        std::vector<StationPtr> stations;
        std::unordered_multiset<std::string> uuids;
        std::optional<std::size_t> scroll_to_station;
        std::string name_filter;
        std::string tag_filter;
        task_queue pending_tasks;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        const std::string&
        get_id(const StationPtr& st);

        void
        handle_station_add(Station station);

        void
        handle_station_delete(std::size_t index);

        void
        handle_station_edit(StationPtr old_station,
                            Station new_station);

        void
        move_down(std::size_t index);

        void
        move_up(std::size_t index);

        void
        show_station(std::size_t index,
                     StationPtr& station);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        const std::string&
        get_id(const StationPtr& st)
        {
            return st->stationuuid;
        }


        void
        handle_station_add(Station station)
        {
            if (!station.stationuuid.empty()) {
                if (uuids.contains(station.stationuuid)) {
                    // TODO: show error popup
                    LOG_ERROR("Duplicated UUID: {}", station.stationuuid);
                    return;
                }
                uuids.insert(station.stationuuid);
            }
            stations.push_back(std::make_shared<Station>(std::move(station)));
        }


        void
        handle_station_delete(std::size_t index)
        {
            LOG_DEBUG("Removing favorite station index: {}", index);
            remove(index);
        }


        void
        handle_station_edit(StationPtr station,
                            Station new_station)
        {
            if (station->stationuuid != new_station.stationuuid) {
                // UUID changed
                if (uuids.contains(new_station.stationuuid)) {
                    // TODO: show error popup
                    LOG_ERROR("Duplicated UUID: {}", new_station.stationuuid);
                    return;
                }
                if (!station->stationuuid.empty())
                    uuids.erase(station->stationuuid);
            }
            *station = std::move(new_station);
            if (!station->stationuuid.empty())
                uuids.insert(station->stationuuid);
        }


        void
        move_down(std::size_t index)
        {
            if (index >= stations.size() || index + 1 >= stations.size())
                return;
            std::swap(stations[index], stations[index + 1]);
            scroll_to_station = index + 1;
        }


        void
        move_up(std::size_t index)
        {
            if (index < 1 || index >= stations.size())
                return;
            std::swap(stations[index], stations[index - 1]);
            scroll_to_station = index - 1;
        }


        void
        show_station(std::size_t index,
                     StationPtr& station)
        {
            using namespace ImGui::RAII;

            ID station_id{static_cast<const void*>(station.get())};

            const auto& style = ImGui::GetStyle();

            float frame_height = station->expanded
                ? 0
                : 2 * style.FramePadding.y
                + UI::play_button_size.y
                + 2 * style.ItemSpacing.y
                + 2 * UI::get_small_button_size().y;
            ImGuiChildFlags frame_flags =
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened;
            if (station->expanded)
                frame_flags |= ImGuiChildFlags_AutoResizeY;

            if (Child station_frame{"station_frame", {0, frame_height}, frame_flags}) {

                if (Child actions{
                        "actions",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeX |
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    UI::PlayButton(station);

                    {
                        Disabled disable_first_index{index == 0};
                        // ▲
                        if (ImGui::Button(ICON_FA_CHEVRON_UP,
                                          UI::get_small_button_size())) {
                            pending_tasks.add(move_up, index);
                        }
                        ImGui::SetItemTooltip("Move this station up.");
                    }

                    ImGui::SameLine();

                    {
                        Disabled disable_last_index{index + 1 >= stations.size()};
                        // ▼
                        if (ImGui::Button(ICON_FA_CHEVRON_DOWN,
                                          UI::get_small_button_size())) {
                            pending_tasks.add(move_down, index);
                        }
                        ImGui::SetItemTooltip("Move this station down.");
                    }

                    // ✎
                    if (ImGui::Button(ICON_FA_PENCIL, UI::get_small_button_size()))
                        EditStationPopup::open(*station,
                                               std::bind_front(handle_station_edit, station));
                    ImGui::SetItemTooltip("Edit this station.");

                    ImGui::SameLine();

                    // 🗑
                    if (ImGui::Button(ICON_FA_TRASH_O, UI::get_small_button_size()))
                        ConfirmDeleteStationPopup::open(
                            station,
                            std::bind_front(handle_station_delete, index)
                        );
                    ImGui::SetItemTooltip("Remove this station from favorites.");

                } // actions

                ImGui::SameLine();

                ImGuiChildFlags details_flags = ImGuiChildFlags_NavFlattened;
                if (station->expanded)
                    details_flags = ImGuiChildFlags_AutoResizeY;

                // StyleColor details_bg{ImGuiCol_ChildBg, {0.0f, 0.75f, 0.0f, 0.5f}};
                if (Child details{
                        "details",
                        {0, 0},
                        details_flags}) {

                    UI::StationInfo(*station);

                } // details

            } // station_frame

        }

    } // namespace



    void
    add(const Station& st)
    {
        stations.push_back(std::make_shared<Station>(st));
        if (!st.stationuuid.empty())
            uuids.insert(st.stationuuid);
    }


    bool
    contains(const Station& station)
    {
        if (!station.stationuuid.empty())
            return contains(station.stationuuid);

        for (const auto& st : stations)
            if (station == *st)
                return true;
        return false;
    }


    bool
    contains(const std::string& uuid)
    {
        if (uuid.empty())
            return false;
        return uuids.contains(uuid);
    }


    void
    finalize()
    {
        save();
    }


    void
    initialize()
    {
        load();
    }


    void
    load()
    try {
        TRACE_FUNC;

        stations.clear();

        auto filename = App::get_config_path() / "favorites.json";
        Serializer::load(stations, filename);

        uuids.clear();
        for (auto& st : stations) {
            if (!st->stationuuid.empty())
                uuids.insert(st->stationuuid);
        }

        LOG_INFO("Loaded {} favorites.", stations.size());
    }
    catch (std::exception& e) {
        LOG_ERROR("{}", e.what());
    }


    void
    process_logic()
    {
        pending_tasks.dispatch_all();
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        if (Child toolbar{
                "toolbar",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_NavFlattened
            }) {

            // ➕
            if (ImGui::Button(ICON_FA_PLUS " Add"))
                EditStationPopup::open(handle_station_add);
            ImGui::SetItemTooltip("Add a new station to favorites.");

            ImGui::SameLine();

            ImGui::SetNextItemWidth(400);
            ImGui::InputTextWithHint("##name_filter"s, "Filter by name..."s, name_filter);

            ImGui::SameLine();

            ImGui::SetNextItemWidth(400);
            ImGui::InputTextWithHint("##tag_filter"s, "Filter by tag..."s, tag_filter);

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::FormatTextAligned(1, -1, "{} stations", stations.size());

        } // toolbar

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (Child favorites{"favorites"}) {

            for (std::size_t index = 0; index < stations.size(); ++index) {
                auto& station = stations[index];

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

                if (scroll_to_station == index) {
                    scroll_to_station.reset();
                    UI::SmoothScrollItem();
                }
                UI::DoSmoothScroll();

            }

        } // favorites

        EditStationPopup::process_ui();
        ConfirmDeleteStationPopup::process_ui();
    }


    void
    remove(std::size_t index)
    {
        if (index >= stations.size()) {
            LOG_ERROR("BUG: attempting to remove invalid index: {}", index);
            return;
        }

        auto it = uuids.find(stations[index]->stationuuid);
        if (it != uuids.end())
            uuids.erase(it);

        stations.erase(stations.begin() + index);
    }


    void
    remove(const std::string& uuid)
    {
        if (uuid.empty())
            return;
        if (!uuids.contains(uuid))
            return;

        auto u = uuids.find(uuid);
        if (u != uuids.end())
            uuids.erase(u);

        auto s = std::ranges::find(stations, uuid, get_id);
        if (s != stations.end())
            stations.erase(s);
    }


    void
    remove(const Station& station)
    {
        if (!station.stationuuid.empty())
            return remove(station.stationuuid);

        std::erase_if(stations,
                      [&station](const StationPtr& st)
                      {
                          return station.stationuuid == st->stationuuid;
                      });
    }


    void
    save()
    try {
        TRACE_FUNC;

        auto filename = App::get_config_path() / "favorites.json";
        Serializer::save(stations, filename);
    }
    catch (std::exception& e) {
        LOG_ERROR("{}", e.what());
    }

} // namespace FavoritesTab
