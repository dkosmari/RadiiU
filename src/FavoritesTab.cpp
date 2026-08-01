/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
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
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "Serializer.hpp"
#include "Station.hpp"
#include "StationGlaze.hpp"
#include "string_utils.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using string_utils::from_csv;
using string_utils::to_csv;

namespace FavoritesTab {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        struct MoveOp {
            std::size_t src;
            std::size_t dst;
        };


        // When editing we need a string, not a vector of strings.
        struct EditFields {

            std::string old_uuid;
            std::string language;
            std::string tags;


            EditFields();

            EditFields(const Station& station);

        }; // struct EditFields

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        const std::string&
        get_id(const StationPtr& st);

        void
        process_popup_create();

        void
        process_popup_delete(const Station& station,
                             std::size_t index);

        void
        process_popup_edit(Station& station);

        void
        show_row(const std::string& label,
                 std::string& value);

        void
        show_station(StationPtr& station,
                     std::size_t index);

        void
        show_station_fields(Station& st);

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_delete_title = "Delete station?";
        const std::string popup_edit_title = "Edit station";
        const std::string popup_create_title = "Create station";

        /*-----------*/
        /* Variables */
        /*-----------*/

        std::vector<StationPtr> stations;
        std::unordered_multiset<std::string> uuids;
        std::optional<MoveOp> move_operation;
        std::optional<std::size_t> scroll_to_station;
        std::optional<std::size_t> station_index_to_remove;
        std::optional<EditFields> edit_fields;
        std::optional<Station> created_station;

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        EditFields::EditFields() = default;


        EditFields::EditFields(const Station& station) :
            old_uuid{station.stationuuid},
            language{to_csv(station.language)},
            tags{to_csv(station.tags)}
        {}


        const std::string&
        get_id(const StationPtr& st)
        {
            return st->stationuuid;
        }


        void
        process_popup_create()
        {
            using namespace ImGui::RAII;

            ImGui::SetNextWindowSize({1100, 700}, ImGuiCond_Appearing);
            ImGui::SetNextWindowSizeConstraints({ 400, 400 },
                                                { FLT_MAX, FLT_MAX });
            if (PopupModal popup_create{
                    popup_create_title,
                    nullptr,
                    ImGuiWindowFlags_NoSavedSettings
                }) {

                if (!created_station)
                    created_station.emplace();

                if (!edit_fields)
                    edit_fields.emplace();

                // Note: use a helper child window to push the response buttons to the bottom.
                if (Child content_child{
                        "content",
                        {0, -ImGui::GetFrameHeightWithSpacing()},
                        ImGuiChildFlags_NavFlattened
                    })
                    show_station_fields(*created_station);

                auto content_size = ImGui::GetContentRegionAvail();

                // Cancel button
                {
                    if (ImGui::Button(ICON_FA_TIMES " Cancel")) {
                        ImGui::CloseCurrentPopup();
                        edit_fields.reset();
                        created_station.reset();
                    }
                    ImGui::SetItemTooltip("Cancel creating a new station.");
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::SameLine();

                // Create button
                {
                    // Line it up to the right of the window
                    auto& style = ImGui::GetStyle();
                    const std::string label = ICON_FA_CHECK " Create";
                    ImVec2 btn_size = ImGui::CalcTextSize(label, true) + style.FramePadding * 2;
                    float new_x = content_size.x - btn_size.x;
                    float cur_x = ImGui::GetCursorPosX();
                    if (new_x > cur_x) // avoid overlapping with Cancel button
                        ImGui::SetCursorPosX(new_x);
                    if (ImGui::Button(label)) {
                        ImGui::CloseCurrentPopup();
                        if (edit_fields) {
                            created_station->language = from_csv(edit_fields->language);
                            created_station->tags = from_csv(edit_fields->tags);
                            edit_fields.reset();
                            add(*created_station);
                            created_station.reset();
                        }
                    }
                    ImGui::SetItemTooltip("Confirm creating a new station.");
                }

            }
        }


        void
        process_popup_delete(const Station& station,
                             std::size_t index)
        {
            using namespace ImGui::RAII;

            ImGui::SetNextWindowSize({800, 300}, ImGuiCond_Appearing);
            ImGui::SetNextWindowSizeConstraints({ 400, 250 },
                                                { FLT_MAX, FLT_MAX });
            if (PopupModal popup_delete{
                    popup_delete_title,
                    nullptr,
                    ImGuiWindowFlags_NoSavedSettings
                }) {

                auto window_size = ImGui::GetContentRegionAvail();

                // Note: we use a helper child window to push the response buttons to the bottom.
                if (Child content_child{
                        "content",
                        {0, -ImGui::GetFrameHeightWithSpacing()
                        }})
                    ImGui::TextWrapped(station.name);

                // Cancel button
                {
                    if (ImGui::Button(ICON_FA_TIMES " Cancel"))
                        ImGui::CloseCurrentPopup();
                    ImGui::SetItemTooltip("Cancel deleting this station.");
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::SameLine();

                // Delete button
                {
                    // Line it up to the right of the window
                    auto& style = ImGui::GetStyle();
                    const std::string label = ICON_FA_TRASH_O " Delete";
                    ImVec2 btn_size = ImGui::CalcTextSize(label, true) + style.FramePadding * 2;
                    float new_x = window_size.x - btn_size.x;
                    float cur_x = ImGui::GetCursorPosX();
                    if (new_x > cur_x) // avoid overlapping with Cancel button
                        ImGui::SetCursorPosX(new_x);
                    if (ImGui::Button(label)) {
                        ImGui::CloseCurrentPopup();
                        station_index_to_remove = index;
                    }
                    ImGui::SetItemTooltip("Confirm deleting this station.");
                }

            } // popup_delete
        }


        void
        process_popup_edit(Station& station)
        {
            using namespace ImGui::RAII;

            // TODO: add button for updating from Browser, if uuid is present

            ImGui::SetNextWindowSize({1100, 700}, ImGuiCond_Appearing);
            ImGui::SetNextWindowSizeConstraints({ 400, 400 },
                                                { FLT_MAX, FLT_MAX });
            if (PopupModal popup_edit{
                    popup_edit_title,
                    nullptr,
                    ImGuiWindowFlags_NoSavedSettings
                }) {

                if (!edit_fields)
                    edit_fields.emplace(station);

                // Note: use a helper child window to push the response buttons to the bottom.
                if (Child content_child{
                        "content",
                        {0, -ImGui::GetFrameHeightWithSpacing()},
                        ImGuiChildFlags_NavFlattened
                    })
                    show_station_fields(station);

                auto content_size = ImGui::GetContentRegionAvail();

                // Cancel button
                {
                    if (ImGui::Button(ICON_FA_TIMES " Cancel")) {
                        ImGui::CloseCurrentPopup();
                        edit_fields.reset();
                    }
                    ImGui::SetItemTooltip("Cancel editing this station.");
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::SameLine();

                // Apply button
                {
                    // Line it up to the right of the window
                    auto& style = ImGui::GetStyle();
                    const std::string label = ICON_FA_CHECK " Apply";
                    ImVec2 btn_size = ImGui::CalcTextSize(label, true) + style.FramePadding * 2;
                    float new_x = content_size.x - btn_size.x;
                    float cur_x = ImGui::GetCursorPosX();
                    if (new_x > cur_x) // avoid overlapping with Cancel button
                        ImGui::SetCursorPosX(new_x);
                    if (ImGui::Button(label)) {
                        ImGui::CloseCurrentPopup();
                        if (edit_fields) {
                            // If user changed UUID, update uuids set.
                            if (edit_fields->old_uuid != station.stationuuid) {
                                auto it = uuids.find(edit_fields->old_uuid);
                                if (it != uuids.end())
                                    uuids.erase(it);
                                if (!station.stationuuid.empty())
                                    uuids.insert(station.stationuuid);
                            }
                            station.language = from_csv(edit_fields->language);
                            station.tags = from_csv(edit_fields->tags);
                            edit_fields.reset();
                        }
                    }
                    ImGui::SetItemTooltip("Confirm editing this station.");
                }

            }
        }


        void
        show_row(const std::string& label,
                 std::string& value)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            UI::show_label(label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText("##" + label, value);
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

                    UI::show_play_button(station);

                    {
                        Disabled disable_first_index{index == 0};
                        // ▲
                        if (ImGui::Button(ICON_FA_CHEVRON_UP)) {
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
                        if (ImGui::Button(ICON_FA_CHEVRON_DOWN)) {
                            move_operation.emplace();
                            move_operation->src = index;
                            move_operation->dst = index + 1;
                        }
                        ImGui::SetItemTooltip("Move this station down.");
                    }

                    // ✎
                    if (ImGui::Button(ICON_FA_PENCIL))
                        ImGui::OpenPopup(popup_edit_title);
                    ImGui::SetItemTooltip("Edit this station.");
                    process_popup_edit(*station);

                    ImGui::SameLine();

                    // 🗑
                    if (ImGui::Button(ICON_FA_TRASH_O))
                        ImGui::OpenPopup(popup_delete_title);
                    ImGui::SetItemTooltip("Remove this station from favorites.");
                    process_popup_delete(*station, index);

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

                    } // extra_info

                } // details

            } // station

        }


        void
        show_station_fields(Station& st)
        {
            using namespace ImGui::RAII;

            if (!edit_fields)
                return;

            if (Table fields_table{"fields", 2}) {
                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                show_row("name",         st.name);
                show_row("url",          st.url);
                show_row("url_resolved", st.url_resolved);
                show_row("homepage",     st.homepage);
                show_row("favicon",      st.favicon);
                show_row("tags",         edit_fields->tags);
                show_row("countrycode",  st.countrycode);
                show_row("language",     edit_fields->language);
                show_row("stationuuid",  st.stationuuid);
            }
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

        // Handle pending delete
        if (station_index_to_remove) {
            remove(*station_index_to_remove);
            station_index_to_remove.reset();
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

            // ➕
            if (ImGui::Button(ICON_FA_PLUS " Add"))
                ImGui::OpenPopup(popup_create_title);
            ImGui::SetItemTooltip("Add a new station to favorites.");
            process_popup_create();

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            UI::show_text_right("%zu stations", stations.size());

        } // toolbar_child

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (Child favorites_child{"favorites"}) {

            for (std::size_t index = 0; index < stations.size(); ++index) {
                show_station(stations[index], index);
                if (scroll_to_station && *scroll_to_station == index) {
                    ImGui::SetScrollHereY();
                    scroll_to_station.reset();
                }
            }

        }
    }


    void
    remove(std::size_t index)
    {
        if (index >= stations.size())
            return;

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
