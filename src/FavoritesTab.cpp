/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <functional>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
#include <unordered_set>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "FavoritesTab.hpp"

#include "App.hpp"
#include "cfg.hpp"
#include "ConfirmDeleteStationPopup.hpp"
#include "EditStationPopup.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "Serializer.hpp"
#include "Station.hpp"
#include "StationGlaze.hpp"
#include "string_utils.hpp"
#include "tracer.hpp"
#include "UI.hpp"


namespace FavoritesTab {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        struct MoveOp {
            std::size_t src;
            std::size_t dst;
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_delete_title = "Delete station?";


        /*-----------*/
        /* Variables */
        /*-----------*/

        std::vector<StationPtr> stations;
        std::unordered_multiset<std::string> uuids;
        std::optional<MoveOp> move_operation;
        std::optional<std::size_t> scroll_to_station;


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
        show_station(StationPtr& station,
                     std::size_t index);


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
        show_station(StationPtr& station,
                     std::size_t index)
        {
            using namespace ImGui::RAII;

            ID station_id{std::to_string(index) + ":" + station->stationuuid};

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

                    UI::PlayButton(station);

                    {
                        Disabled disable_first_index{index == 0};
                        // ▲
                        if (ImGui::Button(ICON_FA_CHEVRON_UP,
                                          UI::get_small_button_size())) {
                            move_operation.emplace();
                            move_operation->src = index;
                            move_operation->dst = index - 1;
                        }
                        ImGui::SetItemTooltip("Move this station up.");
                    }

                    ImGui::SameLine();

                    {
                        Disabled disable_last_index{index + 1 >= stations.size()};
                        // ▼
                        if (ImGui::Button(ICON_FA_CHEVRON_DOWN,
                                          UI::get_small_button_size())) {
                            move_operation.emplace();
                            move_operation->src = index;
                            move_operation->dst = index + 1;
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

                if (Child details{
                        "details",
                        {0, 0},
                        ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_NavFlattened
                    }) {

                    UI::FavIcon(*station);

                    ImGui::SameLine();

                    UI::show_station_basic_info(*station);

                    if (Child extra_info_child{
                            "extra_info",
                            {0, 0},
                            ImGuiChildFlags_AutoResizeY |
                            ImGuiChildFlags_NavFlattened
                        }) {

                        UI::TagsList(station->tags);

                    } // extra_info

                } // details

            } // station

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
        // Handle any pending move
        if (move_operation) {
            auto [src, dst] = *move_operation;
            assert(src < stations.size());
            assert(dst < stations.size());
            auto tmp = std::move(stations[src]);
            stations.erase(stations.begin() + src);
            stations.insert(stations.begin() + dst, std::move(tmp));
            scroll_to_station = dst;
            move_operation.reset();
        }
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

            ImGui::AlignTextToFramePadding();
            ImGui::FormatTextAligned(1, -1, "{} stations", stations.size());

        } // toolbar

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (Child favorites{"favorites"}) {

            for (std::size_t index = 0; index < stations.size(); ++index) {
                show_station(stations[index], index);
                if (scroll_to_station && *scroll_to_station == index) {
                    ImGui::SetScrollHereY();
                    scroll_to_station.reset();
                }
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
