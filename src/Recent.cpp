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
#include "IconsFontAwesome4.h"
#include "imgui_extras.hpp"
#include "json.hpp"
#include "Player.hpp"
#include "Station.hpp"
#include "ui.hpp"
#include "utils.hpp"


using std::cout;
using std::endl;


// TODO: allow inserting/removing station into Favorites from Recent

namespace Recent {

    namespace {

        std::deque<std::shared_ptr<Station>> stations;

        std::shared_ptr<Station> pending_add;
        std::optional<std::size_t> pending_remove;

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
            for (auto& elem : list) {
                auto& obj = elem.as<json::object>();
                auto st = std::make_shared<Station>(Station::from_json(obj));
                stations.push_back(std::move(st));
            }
        }
        catch (std::exception& e) {
            cout << "ERROR: Recent::load(): " << e.what() << endl;
        }
    }


    void
    save()
    {
        try {
            json::array list;
            for (const auto& station : stations)
                list.push_back(station->to_json());
            json::save(std::move(list), cfg::base_dir / "recent.json");
        }
        catch (std::exception& e) {
            cout << "ERROR: Recent::save(): " << e.what() << endl;
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
    show_station(std::shared_ptr<Station>& station,
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

                ui::show_play_button(station);

                if (ImGui::Button(ICON_FA_TRASH_O /*🗑*/))
                    pending_remove = index;

                ImGui::SameLine();

                ImGui::BeginDisabled(station->uuid.empty());
                if (ImGui::Button(ICON_FA_INFO_CIRCLE /*🛈*/))
                    ui::open_station_info_popup(station->uuid);
                ImGui::EndDisabled();
                ui::process_station_info_popup();

            } // actions
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("details",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {

                ui::show_favicon(station->favicon);

                ImGui::SameLine();

                ui::show_station_basic_info(*station, scroll_target);

                if (ImGui::BeginChild("extra_info",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_NavFlattened)) {

                    ui::show_tags(station->tags, scroll_target);

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
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_NavFlattened)) {

            if (ImGui::Button("Clear"))
                stations.clear();

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::TextRight("%zu stations", stations.size());

        } // toolbar
        ImGui::EndChild();

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::BeginChild("recent")) {

            auto scroll_target = ImGui::GetCurrentWindow()->ID;
            for (std::size_t index = stations.size() - 1; index + 1 > 0; --index)
                show_station(stations[index], index, scroll_target);

        } // recent
        ImGui::HandleDragScroll();
        ImGui::EndChild();
    }


    void
    process_pending_add()
    {
        if (!pending_add)
            return;
        if (!stations.empty() && *pending_add == *stations.back())
            return;
        stations.push_back(std::move(pending_add));
    }


    void
    process_pending_remove()
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
    prune()
    {
        if (stations.size() > cfg::recent_limit) {
            std::size_t pending_remove = stations.size() - cfg::recent_limit;
            stations.erase(stations.begin(), stations.begin() + pending_remove);
        }
    }


    void
    process_logic()
    {
        process_pending_add();
        process_pending_remove();
        prune();
    }


    void
    add(std::shared_ptr<Station>& station)
    {
        pending_add = station;
    }

} // namespace Recent
