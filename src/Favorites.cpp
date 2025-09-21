/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>
#include <unordered_set>

#include <imgui.h>
#include <imgui_internal.h>

#include "Favorites.hpp"

#include "cfg.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "Player.hpp"
#include "Station.hpp"


using sdl::vec2;

using std::cout;
using std::endl;


namespace Favorites {

    std::vector<Station> stations;
    std::unordered_set<std::string> uuids;


    namespace {

        struct MoveOp {
            std::size_t src;
            std::size_t dst;
        };

        std::optional<MoveOp> move_operation;

        std::optional<std::size_t> scroll_to_station;

    } // namespace



    void
    load()
    {
        try {
            auto root = json::load(cfg::base_dir / "favorites.json");
            const auto& list = root.as<json::array>();
            stations.clear();
            uuids.clear();
            for (auto& elem : list) {
                stations.push_back(Station::from_json(elem.as<json::object>()));
                const auto& st = stations.back();
                if (!st.uuid.empty())
                    uuids.insert(st.uuid);
            }
            cout << "Loaded " << stations.size() << " favorites" << endl;
        }
        catch (std::exception& e) {
            cout << "Error loading favorites: " << e.what() << endl;
        }
    }


    void
    save()
    {
        try {
            json::array list;
            for (const Station& st : stations)
                list.push_back(st.to_json());
            const std::filesystem::path old_favorites = cfg::base_dir / "favorites.json";
            const std::filesystem::path new_favorites = cfg::base_dir / "favorites.json.new";

            json::value root = std::move(list);
#ifdef __WIIU__
            if (exists(new_favorites))
                remove(new_favorites);
#endif
            save(root, new_favorites);
#ifdef __WIIU__
            if (exists(old_favorites))
                remove(old_favorites);
#endif
            rename(new_favorites, old_favorites);
        }
        catch (std::exception& e) {
            cout << "Error saving favorites: " << e.what() << endl;
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
        ImGui::PushID(std::to_string(index) + ":" + station.uuid);

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
                const vec2 play_size = {96, 96};
                if (ImGui::ImageButton("play_button",
                                       *IconManager::get("ui/play-button.png"),
                                       play_size)) {
                    Player::play(station);
                }

                ImGui::BeginDisabled(index == 0);
                if (ImGui::Button("▲")) {
                    move_operation.emplace();
                    move_operation->src = index;
                    move_operation->dst = index - 1;
                }
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::BeginDisabled(index + 1 >= stations.size());
                if (ImGui::Button("▼")) {
                    move_operation.emplace();
                    move_operation->src = index;
                    move_operation->dst = index + 1;
                }
                ImGui::EndDisabled();

                if (ImGui::Button("🖊")) {
                    cout << "TODO: edit favorite" << endl;
                }

                ImGui::SameLine();

                if (ImGui::Button("🗑")) {
                    cout << "TODO: confirm delete favorites" << endl;
                }

            }
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild(); // actions

            ImGui::SameLine();

            if (!station.favicon.empty()) {
                auto icon = IconManager::get(station.favicon);
                auto icon_size = icon->get_size();
                vec2 size = {128, 128};
                size.x = icon_size.x * size.y / icon_size.y;
                ImGui::Image(*IconManager::get(station.favicon), size);
                ImGui::SameLine();
            }

            // ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));

            if (ImGui::BeginChild("station_info",
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
            ImGui::EndChild(); // station_info

            // ImGui::PopStyleColor();

        }
        ImGui::HandleDragScroll(scroll_target);
        ImGui::EndChild(); // station

        ImGui::PopID();
    }


    void
    process_ui()
    {
        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::BeginChild("favorites")) {
            auto scroll_target = ImGui::GetCurrentWindow()->ID;
            for (std::size_t index = 0; index < stations.size(); ++index) {
                show_station(stations[index], index, scroll_target);
                if (scroll_to_station && *scroll_to_station == index) {
                    ImGui::SetScrollHereY();
                    scroll_to_station.reset();
                }
            }
        }
        ImGui::HandleDragScroll();
        ImGui::EndChild();

        // Handle any pending move
        if (move_operation) {
            auto [src, dst] = *move_operation;
            assert(src < stations.size());
            assert(dst < stations.size());
            Station tmp = std::move(stations[src]);
            stations.erase(stations.begin() + src);
            stations.insert(stations.begin() + dst, std::move(tmp));
            scroll_to_station = dst;
            move_operation.reset();
        }
    }


    bool
    contains(const std::string& uuid)
    {
        if (uuid.empty())
            return false;
        return uuids.contains(uuid);
    }


    void
    add(const Station& st)
    {
        stations.push_back(st);
        if (!st.uuid.empty())
            uuids.insert(st.uuid);
    }


    namespace {

        const std::string&
        by_id(const Station& st)
        {
            return st.uuid;
        }

    } // namespace


    void
    remove(const std::string& uuid)
    {
        if (uuid.empty())
            return;
        if (!uuids.contains(uuid))
            return;
        uuids.erase(uuid);
        auto it = std::ranges::find(stations, uuid, by_id);
        if (it != stations.end())
            stations.erase(it);
    }

} // namespace Favorites
