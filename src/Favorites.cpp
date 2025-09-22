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
#include <misc/cpp/imgui_stdlib.h>

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

    namespace {

        const ImVec4 label_color = {1.0, 1.0, 0.25, 1.0};

        std::vector<Station> stations;
        std::unordered_set<std::string> uuids;


        struct MoveOp {
            std::size_t src;
            std::size_t dst;
        };
        std::optional<MoveOp> move_operation;


        std::optional<std::size_t> scroll_to_station;


        const std::string delete_popup_title = "Delete station?";
        std::optional<std::size_t> station_to_remove;


        const std::string edit_popup_title = "Edit station";
        std::optional<Station> editing_station;

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
    process_delete_popup(const Station& station,
                         std::size_t index)
    {
        ImGui::SetNextWindowSize({800, 300}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({ 400, 250 },
                                            { FLT_MAX, FLT_MAX });
        if (ImGui::BeginPopupModal(delete_popup_title,
                                   nullptr,
                                   ImGuiWindowFlags_NoSavedSettings)) {
            auto window_size = ImGui::GetContentRegionAvail();

            if (ImGui::BeginChild("content", {0, -ImGui::GetFrameHeightWithSpacing()})) {
                ImGui::TextWrapped("%s", station.name.data());
            }
            ImGui::EndChild();

            // Cancel button
            {
                if (ImGui::Button("Cancel"))
                    ImGui::CloseCurrentPopup();
                ImGui::SetItemDefaultFocus();
            }

            ImGui::SameLine();

            // Delete button
            {
                // Line it up to the right of the window
                auto& style = ImGui::GetStyle();
                const std::string label = "Delete";
                ImVec2 btn_size = ImGui::CalcTextSize(label) + style.FramePadding * 2;
                float new_x = window_size.x - btn_size.x;
                float cur_x = ImGui::GetCursorPosX();
                if (new_x > cur_x) // avoid overlapping with Cancel button
                    ImGui::SetCursorPosX(new_x);
                if (ImGui::Button(label)) {
                    ImGui::CloseCurrentPopup();
                    station_to_remove = index;
                }
            }
            ImGui::HandleDragScroll();
            ImGui::EndPopup();
        }
    }


    void
    process_edit_popup(Station& station)
    {
        if (!editing_station)
            return;

        ImGui::SetNextWindowSize({1100, 700}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({ 400, 400 },
                                            { FLT_MAX, FLT_MAX });
        if (ImGui::BeginPopupModal(edit_popup_title,
                                   nullptr,
                                   ImGuiWindowFlags_NoSavedSettings)) {

            if (ImGui::BeginChild("content",
                                  {0, -ImGui::GetFrameHeightWithSpacing()})) {

                // auto scroll_target = ImGui::GetCurrentWindow()->ID;

                if (ImGui::BeginTable("fields", 2,
                                      ImGuiTableFlags_None)) {

                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "name");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##name", &editing_station->name);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "url");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##url", &editing_station->url);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "url_resolved");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##url_resolved", &editing_station->url_resolved);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "homepage");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##homepage", &editing_station->homepage);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "favicon");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##favicon", &editing_station->favicon);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "tags");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##tags", &editing_station->tags);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "country_code");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##country_code", &editing_station->country_code);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "language");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##language", &editing_station->language);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextAlignedColored(1.0, -FLT_MIN, label_color, "uuid");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##uuid", &editing_station->uuid);

                    ImGui::EndTable();
                }

            }
            ImGui::HandleDragScroll();
            ImGui::EndChild();

            auto window_size = ImGui::GetContentRegionAvail();

            // Cancel button
            {
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                    editing_station.reset();
                }
                ImGui::SetItemDefaultFocus();
            }

            ImGui::SameLine();

            // Apply button
            {
                // Line it up to the right of the window
                auto& style = ImGui::GetStyle();
                const std::string label = "Apply";
                ImVec2 btn_size = ImGui::CalcTextSize(label) + style.FramePadding * 2;
                float new_x = window_size.x - btn_size.x;
                float cur_x = ImGui::GetCursorPosX();
                if (new_x > cur_x) // avoid overlapping with Cancel button
                    ImGui::SetCursorPosX(new_x);
                if (ImGui::Button(label)) {
                    ImGui::CloseCurrentPopup();
                    if (editing_station)
                        station = *editing_station;
                    editing_station.reset();
                }
            }

            ImGui::EndPopup();
        }
    }


    void
    show_station(Station& station,
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
                    editing_station = station;
                    ImGui::OpenPopup(edit_popup_title);
                }
                process_edit_popup(station);

                ImGui::SameLine();

                if (ImGui::Button("🗑"))
                    ImGui::OpenPopup(delete_popup_title);
                process_delete_popup(station, index);
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

        // Handle pending delete
        if (station_to_remove) {
            remove(*station_to_remove);
            station_to_remove.reset();
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


    void
    remove(std::size_t index)
    {
        if (index >= stations.size())
            return;
        uuids.erase(stations[index].uuid);
        stations.erase(stations.begin() + index);
    }

} // namespace Favorites
