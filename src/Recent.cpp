/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <deque>
#include <optional>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include "Recent.hpp"

#include "cfg.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "json.hpp"
#include "Player.hpp"
#include "Station.hpp"


using std::cout;
using std::endl;


namespace Recent {

    namespace {

        const std::size_t max_stations = 6;

        std::deque<Station> stations;

        std::optional<Station> to_add;
        std::optional<std::size_t> to_remove;

    } // namespace


    void
    real_add(Station&& station);


    void
    load()
    {
        try {
            auto root = json::load(cfg::base_dir / "recent.json");
            const auto& list = root.as<json::array>();
            stations.clear();
            for (auto& elem : list)
                real_add(Station::from_json(elem.as<json::object>()));
        }
        catch (std::exception& e) {
            cout << "Error loading recent: " << e.what() << endl;
        }
    }


    void
    save()
    {
        try {
            json::array list;
            for (const Station& station : stations)
                list.push_back(station.to_json());
            json::save(std::move(list), cfg::base_dir / "recent.json");
        }
        catch (std::exception& e) {
            cout << "Error saving recent: " << e.what() << endl;
        }
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
    show_station(const Station& station,
                 std::size_t index,
                 ImGuiID scroll_target)
    {
        ImGui::PushID(&station);

        if (ImGui::BeginChild("station",
                              {0, 0},
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_FrameStyle |
                              ImGuiChildFlags_NavFlattened)) {

            if (ImGui::BeginChild("actions",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeX |
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {
                const sdl::vec2 play_size = {96, 96};
                if (ImGui::ImageButton("play_button",
                                       *IconManager::get("ui/play-button.png"),
                                       play_size)) {
                    Player::play(station);
                }

                if (ImGui::Button("🗑"))
                    to_remove = index;

            }
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild(); // actions

            ImGui::SameLine();

            if (!station.favicon.empty()) {
                auto icon = IconManager::get(station.favicon);
                auto icon_size = icon->get_size();
                sdl::vec2 size = {128, 128};
                size.x = icon_size.x * size.y / icon_size.y;
                ImGui::Image(*IconManager::get(station.favicon), size);
                ImGui::SameLine();
            }

            if (ImGui::BeginChild("details",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {
                ImGui::TextUnformatted(station.name);
                if (!station.homepage.empty()) {
                    ImVec4 link_color = ImGui::GetStyle().Colors[ImGuiCol_TextLink];
                    ImGui::PushStyleColor(ImGuiCol_Text, link_color);
                    ImGui::TextUnformatted(station.homepage);
                    ImGui::PopStyleColor();
                }
                if (!station.country_code.empty())
                    ImGui::Text("🏳 %s", station.country_code.data());
                if (!station.tags.empty())
                    ImGui::TextWrapped("🏷 %s", station.tags.data());

                // WORKAROUND to bad layout from ImGui
                ImGui::Spacing();
            }
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild(); // details


        }
        ImGui::HandleDragScroll(scroll_target);
        ImGui::EndChild(); // station

        ImGui::PopID();
    }


    void
    process_ui()
    {
        if (ImGui::BeginChild("toolbar",
                              {0, 0},
                              ImGuiChildFlags_NavFlattened |
                              ImGuiChildFlags_AutoResizeY)) {
            if (ImGui::Button("Clear"))
                stations.clear();

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::TextRight("%zu stations", stations.size());
        }
        ImGui::EndChild();

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::BeginChild("recent")) {
            auto scroll_target = ImGui::GetCurrentWindow()->ID;
            for (std::size_t index = stations.size() - 1; index + 1 > 0; --index)
                show_station(stations[index], index, scroll_target);
        }
        ImGui::HandleDragScroll();
        ImGui::EndChild();

        // Handle any addition
        if (to_add) {
            real_add(std::move(*to_add));
            to_add.reset();
        }

        // Handle any removal
        if (to_remove) {
            std::size_t index = *to_remove;
            if (index < stations.size())
                stations.erase(stations.begin() + index);
            to_remove.reset();
        }
    }


    void
    real_add(Station&& station)
    {
        if (!stations.empty() && station == stations.back())
            return;

        if (max_stations > 0)
            stations.push_back(std::move(station));

        if (stations.size() > max_stations) {
            std::size_t to_remove = stations.size() - max_stations;
            stations.erase(stations.begin(), stations.begin() + to_remove);
        }
    }

    void
    add(const Station& station)
    {
        to_add = station;
    }

} // Recent
