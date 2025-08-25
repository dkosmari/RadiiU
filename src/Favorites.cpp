/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

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


    void
    load()
    {
        try {
            auto root = json::load(cfg::base_dir / "favorites.json");
            const auto& list = root.as<json::array>();
            stations.clear();
            for (auto& elem : list)
                stations.push_back(Station::from_json(elem.as<json::object>()));
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
            const std::filesystem::path old_name = cfg::base_dir / "favorites.json";
            const std::filesystem::path new_name = cfg::base_dir / "favorites.json.new";

            json::value root = std::move(list);
            save(root, new_name);
#ifdef __WIIU__
            remove(old_name);
#endif
            rename(new_name, old_name);
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
    process_ui()
    {
        if (ImGui::BeginChild("favorites")) {

            auto target_id = ImGui::GetCurrentWindow()->ID;

            for (const auto& station : stations) {

                if (ImGui::BeginChild(ImGui::GetID(reinterpret_cast<const void*>(&station)), {0, 0},
                                      ImGuiChildFlags_Borders
                                      | ImGuiChildFlags_AutoResizeY
                                      )) {

                    const vec2 play_size = {64, 64};

                    if (ImGui::ImageButton("play_button",
                                           *IconManager::get("ui/play-button.png"),
                                           play_size)) {
                        Player::play(station);
                    }

                    // TODO: add edit/remove button


                    ImGui::SameLine();

                    if (!station.favicon.empty()) {
                        const vec2 icon_size = {128, 128};
                        ImGui::Image(*IconManager::get(station.favicon), icon_size);

                        ImGui::SameLine();
                    }

                    // ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));

                    if (ImGui::BeginChild("station_info",
                                          {0, 0},
                                          ImGuiChildFlags_AutoResizeY)) {
                        ImGui::TextUnformatted(station.name);
                        if (!station.homepage.empty()) {
                            ImVec4 link_color = ImGui::GetStyle().Colors[ImGuiCol_TextLink];
                            ImGui::PushStyleColor(ImGuiCol_Text, link_color);
                            ImGui::TextUnformatted(station.homepage);
                            ImGui::PopStyleColor();
                        }
                        if (!station.country_code.empty())
                            ImGui::TextUnformatted(station.country_code);
                        if (!station.tags.empty())
                            ImGui::Text("Tags: %s", station.tags.data());

                    }

                    // WORKAROUND to bad layout from ImGui
                    ImGui::Spacing();

                    // ImGui::PopStyleColor();

                    ImGui::HandleDragScroll(target_id);

                    ImGui::EndChild();


                }

                ImGui::HandleDragScroll(target_id);

                ImGui::EndChild();

            }

        }

        ImGui::HandleDragScroll();

        ImGui::EndChild();
    }

} // namespace Favorites
