/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <deque>
#include <optional>
#include <utility>

#include <glaze/json.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "Recent.hpp"

#include "App.hpp"
#include "cfg.hpp"
#include "Favorites.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
#include "Player.hpp"
#include "Station.hpp"
#include "StationDetailsPopup.hpp"
#include "UI.hpp"


using std::cout;
using std::endl;


namespace Recent {

    std::deque<std::shared_ptr<Station>> stations;


    namespace {

        std::shared_ptr<Station> pending_add;
        std::optional<std::size_t> pending_remove;

    } // namespace


    void
    load()
    try {
        auto filename = App::get_config_path() / "recent.json";
        stations.clear();
        glz::ex::read_file_json(stations, filename.c_str(), std::string{});
    }
    catch (std::exception& e) {
        cout << "ERROR: Recent::load(): " << e.what() << endl;
    }


    void
    save()
    try {
        auto filename = App::get_config_path() / "recent.json";
        glz::ex::write_file_json(stations, filename.c_str(), std::string{});
    }
    catch (std::exception& e) {
        cout << "ERROR: Recent::save(): " << e.what() << endl;
    }


    void
    initialize()
    {
        load();
    }


    void
    finalize()
    {
        save();
    }


    void
    show_station(std::shared_ptr<Station>& station,
                 std::size_t index)
    {
        ImGui::RAII::ID station_id{static_cast<int>(index)};

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

                if (ImGui::Button(ICON_FA_TRASH_O)) // 🗑
                    pending_remove = index;
                ImGui::SetItemTooltip("Remove station from recent history.");

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

                    UI::show_tags(station->tags);

                } // extra_info_child

            } // details_child

        } // station_child
    }


    void
    process_ui()
    {
        if (ImGui::RAII::Child toolbar_child{
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
            UI::show_text_right("%zu stations", stations.size());

        } // toolbar_child

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::RAII::Child recent_child{"recent"}) {

            for (std::size_t index = stations.size() - 1; index + 1 > 0; --index)
                show_station(stations[index], index);

        } // recent_child

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
    queue_add(std::shared_ptr<Station>& station)
    {
        pending_add = station;
    }

} // namespace Recent
