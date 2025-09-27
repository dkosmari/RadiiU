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
#include <imgui_stdlib.h>

#include "Favorites.hpp"

#include "cfg.hpp"
#include "constants.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "json.hpp"
#include "Player.hpp"
#include "Station.hpp"
#include "ui_utils.hpp"
#include "utils.hpp"


using std::cout;
using std::endl;


using constants::label_color;


namespace Favorites {

    namespace {

        std::vector<Station> stations;
        std::unordered_multiset<std::string> uuids;


        struct MoveOp {
            std::size_t src;
            std::size_t dst;
        };
        std::optional<MoveOp> move_operation;


        std::optional<std::size_t> scroll_to_station;


        const std::string popup_delete_title = "Delete station?";
        std::optional<std::size_t> station_to_remove;


        const std::string popup_edit_title = "Edit station";
        std::optional<Station> edited_station;
        std::optional<std::string> edited_tags;

        const std::string popup_create_title = "Create station";
        std::optional<Station> created_station;

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
                const auto& station = stations.back();
                if (!station.uuid.empty())
                    uuids.insert(station.uuid);
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
            for (const Station& station : stations)
                list.push_back(station.to_json());
            json::save(std::move(list), cfg::base_dir / "favorites.json");
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
    process_popup_delete(const Station& station,
                         std::size_t index)
    {
        ImGui::SetNextWindowSize({800, 300}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({ 400, 250 },
                                            { FLT_MAX, FLT_MAX });
        if (ImGui::BeginPopupModal(popup_delete_title,
                                   nullptr,
                                   ImGuiWindowFlags_NoSavedSettings)) {
            auto window_size = ImGui::GetContentRegionAvail();

            if (ImGui::BeginChild("content", {0, -ImGui::GetFrameHeightWithSpacing()})) {
                ImGui::TextWrapped("%s", station.name.data());
            }
            ImGui::EndChild();

            // Cancel button
            {
                if (ImGui::Button("🗙 Cancel"))
                    ImGui::CloseCurrentPopup();
                ImGui::SetItemDefaultFocus();
            }

            ImGui::SameLine();

            // Delete button
            {
                // Line it up to the right of the window
                auto& style = ImGui::GetStyle();
                const std::string label = "🗑 Delete";
                ImVec2 btn_size = ImGui::CalcTextSize(label, true) + style.FramePadding * 2;
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
    show_row_for(const std::string& label,
                 std::string& value)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextRightColored(label_color, "%s", label.data());
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##" + label, value);
    }


    void
    show_station_fields(Station& station)
    {
        if (ImGui::BeginTable("fields", 2,
                              ImGuiTableFlags_None)) {

            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

            show_row_for("name", station.name);
            show_row_for("url", station.url);
            show_row_for("url_resolved", station.url_resolved);
            show_row_for("homepage", station.homepage);
            show_row_for("favicon", station.favicon);
            show_row_for("tags", *edited_tags);
            show_row_for("country_code", station.country_code);
            show_row_for("language", station.language);
            show_row_for("uuid", station.uuid);

            ImGui::EndTable();
        }
    }


    void
    process_popup_edit(Station& station)
    {
        if (!edited_station)
            return;

        ImGui::SetNextWindowSize({1100, 700}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({ 400, 400 },
                                            { FLT_MAX, FLT_MAX });
        if (ImGui::BeginPopupModal(popup_edit_title,
                                   nullptr,
                                   ImGuiWindowFlags_NoSavedSettings)) {

            if (ImGui::BeginChild("content",
                                  {0, -ImGui::GetFrameHeightWithSpacing()}))
                show_station_fields(*edited_station);

            ImGui::HandleDragScroll();
            ImGui::EndChild();

            auto window_size = ImGui::GetContentRegionAvail();

            // Cancel button
            {
                if (ImGui::Button("🗙 Cancel")) {
                    ImGui::CloseCurrentPopup();
                    edited_station.reset();
                    edited_tags.reset();
                }
                ImGui::SetItemDefaultFocus();
            }

            ImGui::SameLine();

            // Apply button
            {
                // Line it up to the right of the window
                auto& style = ImGui::GetStyle();
                const std::string label = "✔ Apply";
                ImVec2 btn_size = ImGui::CalcTextSize(label, true) + style.FramePadding * 2;
                float new_x = window_size.x - btn_size.x;
                float cur_x = ImGui::GetCursorPosX();
                if (new_x > cur_x) // avoid overlapping with Cancel button
                    ImGui::SetCursorPosX(new_x);
                if (ImGui::Button(label)) {
                    ImGui::CloseCurrentPopup();
                    if (edited_station) {
                        auto it = uuids.find(station.uuid);
                        if (it != uuids.end())
                            uuids.erase(it);

                        // copy the tags string into the tags vector
                        auto tags = utils::split(*edited_tags, ",");
                        for (auto& t : tags)
                            t = utils::trimmed(t, ' ');
                        edited_station->tags = std::move(tags);

                        station = std::move(*edited_station);
                        if (!station.uuid.empty())
                            uuids.insert(station.uuid);
                    }
                    edited_station.reset();
                    edited_tags.reset();
                }
            }

            ImGui::EndPopup();
        }
    }


    void
    process_popup_create()
    {
        if (!created_station)
            return;

        ImGui::SetNextWindowSize({1100, 700}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({ 400, 400 },
                                            { FLT_MAX, FLT_MAX });
        if (ImGui::BeginPopupModal(popup_create_title,
                                   nullptr,
                                   ImGuiWindowFlags_NoSavedSettings)) {

            if (ImGui::BeginChild("content",
                                  {0, -ImGui::GetFrameHeightWithSpacing()})) {
                show_station_fields(*created_station);
            }
            ImGui::HandleDragScroll();
            ImGui::EndChild();

            auto window_size = ImGui::GetContentRegionAvail();

            // Cancel button
            {
                if (ImGui::Button("🗙 Cancel")) {
                    ImGui::CloseCurrentPopup();
                    created_station.reset();
                }
                ImGui::SetItemDefaultFocus();
            }

            ImGui::SameLine();

            // Create button
            {
                // Line it up to the right of the window
                auto& style = ImGui::GetStyle();
                const std::string label = "✔ Create";
                ImVec2 btn_size = ImGui::CalcTextSize(label, true) + style.FramePadding * 2;
                float new_x = window_size.x - btn_size.x;
                float cur_x = ImGui::GetCursorPosX();
                if (new_x > cur_x) // avoid overlapping with Cancel button
                    ImGui::SetCursorPosX(new_x);
                if (ImGui::Button(label)) {
                    ImGui::CloseCurrentPopup();
                    if (created_station)
                        add(*created_station);
                    created_station.reset();
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
                const sdl::vec2 button_size = {96, 96};
                if (ImGui::ImageButton("play_button",
                                       *IconManager::get("ui/play-button.png"),
                                       button_size)) {
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

                if (ImGui::Button("✎")) {
                    edited_station = station;
                    edited_tags = utils::join(edited_station->tags, ", ");
                    ImGui::OpenPopup(popup_edit_title);
                }
                process_popup_edit(station);

                ImGui::SameLine();

                if (ImGui::Button("🗑"))
                    ImGui::OpenPopup(popup_delete_title);
                process_popup_delete(station, index);
            } // actions
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("details",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {

                ui_utils::show_favicon(station.favicon);

                ImGui::SameLine();

                if (ImGui::BeginChild("basic_info",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_NavFlattened)) {

                    ImGui::TextWrapped("%s", station.name.data());

                    if (!station.homepage.empty()) {
                        if (ImGui::TextLink(station.homepage)) {
                            // TODO: show QR code
                        }
                    }

                    if (!station.country_code.empty())
                        ImGui::Text("🏳 %s", station.country_code.data());
                } // basic_info
                ImGui::HandleDragScroll(scroll_target);
                ImGui::EndChild();

                if (ImGui::BeginChild("extra_info",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_NavFlattened)) {

                    ui_utils::show_tags(station.tags, scroll_target);
                } // extra_info
                ImGui::HandleDragScroll(scroll_target);
                ImGui::EndChild();

            } // details
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild();
        } // station
        ImGui::HandleDragScroll(scroll_target);
        ImGui::EndChild();

        ImGui::PopID();
    }


    void
    process_ui()
    {
        if (ImGui::BeginChild("toolbar",
                              {0, 0},
                              ImGuiChildFlags_NavFlattened |
                              ImGuiChildFlags_AutoResizeY)) {

            if (ImGui::Button("➕")) {
                ImGui::OpenPopup(popup_create_title);
                created_station.emplace();
            }
            process_popup_create();

            ImGui::SameLine();

            if (ImGui::Button("💾 Save"))
                save();

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::TextRight("%zu stations", stations.size());

        }
        ImGui::EndChild();


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
    }


    void
    process_logic()
    {
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


    bool
    contains(const Station& station)
    {
        if (!station.uuid.empty())
            return contains(station.uuid);

        for (const Station& st : stations)
            if (station == st)
                return true;
        return false;
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
        {
            auto it = uuids.find(uuid);
            if (it != uuids.end())
                uuids.erase(it);
        }
        {
            auto it = std::ranges::find(stations, uuid, by_id);
            if (it != stations.end())
                stations.erase(it);
        }
    }


    void
    remove(std::size_t index)
    {
        if (index >= stations.size())
            return;
        {
            auto it = uuids.find(stations[index].uuid);
            if (it != uuids.end())
                uuids.erase(it);
        }

        stations.erase(stations.begin() + index);
    }


    void
    remove(const Station& station)
    {
        if (!station.uuid.empty())
            return remove(station.uuid);

        std::erase(stations, station);
    }


} // namespace Favorites
