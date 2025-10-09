/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <sdl2xx/vec2.hpp>

#include <imgui_internal.h>

#include "ui.hpp"

#include "Browser.hpp"
#include "constants.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "Player.hpp"
#include "rest.hpp"
#include "Station.hpp"
#include "utils.hpp"


using std::cout;
using std::endl;


namespace ui {

    void
    show_favicon(const std::string& favicon)
    {
        if (favicon.empty())
            return;

        auto icon = IconManager::get(favicon);
        auto icon_size = icon->get_size();
        sdl::vec2 size = {128, 128};
        size.x = icon_size.x * size.y / icon_size.y;
        ImGui::Image(*IconManager::get(favicon), size);
    }


    void
    show_info_row(const std::string& label,
                  const std::string& value)
    {
        ImGui::TableNextRow();

        // ImGui::PushID(label);

        ImGui::TableNextColumn();
        ImGui::TextRightColored(constants::label_color, "%s", label.data());

        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", value.data());

        // ImGui::PopID();
    }


    template<std::integral T>
    void
    show_info_row(const std::string& label,
                  T value)
    {
        ImGui::TableNextRow();

        // ImGui::PushID(label);

        ImGui::TableNextColumn();
        ImGui::TextRightColored(constants::label_color, "%s", label.data());

        ImGui::TableNextColumn();
        const std::string fmt = "%" + utils::format(value);
        ImGui::TextWrapped(fmt.data(), value);

        // ImGui::PopID();
    }

    /* ----------------------------------------------- */
    /* Explicit instantiations for show_info_row<T>() */
    /* ----------------------------------------------- */

    template
    void show_info_row<std::int8_t>(const std::string& label, std::int8_t value);
    template
    void show_info_row<std::int16_t>(const std::string& label, std::int16_t value);
    template
    void show_info_row<std::int32_t>(const std::string& label, std::int32_t value);
    template
    void show_info_row<std::int64_t>(const std::string& label, std::int64_t value);

    template
    void show_info_row<std::uint8_t>(const std::string& label, std::uint8_t value);
    template
    void show_info_row<std::uint16_t>(const std::string& label, std::uint16_t value);
    template
    void show_info_row<std::uint32_t>(const std::string& label, std::uint32_t value);
    template
    void show_info_row<std::uint64_t>(const std::string& label, std::uint64_t value);


    void
    show_label(const char* fmt,
               ...)
    {
        std::va_list args;
        va_start(args, fmt);
        ImGui::TextRightColoredV(constants::label_color, fmt, args);
        va_end(args);
    }


    void
    show_link_row(const std::string& label,
                  const std::string& url)
    {
        ImGui::TableNextRow();

        ImGui::PushID(label);

        ImGui::TableNextColumn();
        ImGui::TextRightColored(constants::label_color, "%s", label.data());

        ImGui::TableNextColumn();
        if (ImGui::TextLinkOpenURL(url, url)) {
#ifdef __WIIU__
            // TODO: show QR code
#endif
        }

        ImGui::PopID();
    }


    void
    show_play_button(const Station& station)
    {
        const sdl::vec2 button_size = {96, 96};
        if (Player::is_playing(station)) {
            if (ImGui::ImageButton("stop_button",
                                   *IconManager::get("ui/stop-button.png"),
                                   button_size))
                Player::stop();
        } else {
            if (ImGui::ImageButton("play_button",
                                   *IconManager::get("ui/play-button.png"),
                                   button_size))
                Player::play(station);
        }
    }


    void
    show_station_basic_info(const Station& station,
                            ImGuiID scroll_target)
    {
        if (ImGui::BeginChild("basic_info",
                              {0, 0},
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_NavFlattened)) {

            ImGui::TextWrapped("%s", station.name.data());

            if (!station.homepage.empty()) {
                if (ImGui::TextLinkOpenURL(station.homepage, station.homepage)) {
#ifdef __WIIU__
                    // TODO: show QR code
#endif
                }
            }

            if (!station.country_code.empty()) {
                ImGui::Text("🏳 %s", station.country_code.data());
                auto name = Browser::get_country_name(station.country_code);
                if (name)
                    ImGui::SetItemTooltip("%s", name->data());
            }

        } // basic_info
        ImGui::HandleDragScroll(scroll_target);
        ImGui::EndChild();
    }


    void
    show_tags(const std::vector<std::string>& tags,
              ImGuiID scroll_target)
    {
        if (tags.empty())
            return;

        bool first_tag = true;
        const ImGuiStyle& style = ImGui::GetStyle();
        for (const auto& tag : tags) {
            ImGui::PushID(&tag);

            if (!first_tag)
                ImGui::SameLine();
            first_tag = false;

            std::string label = "🏷 " + tag;
            float width = ImGui::CalcTextSize(label).x
                + style.ItemSpacing.x
                + 2 * style.FramePadding.x
                + 2 * style.FrameBorderSize;
            float available = ImGui::GetContentRegionAvail().x;
            if (width > available)
                ImGui::NewLine();

            if (ImGui::BeginChild("tag",
                                  {0,0},
                                  ImGuiChildFlags_AutoResizeX |
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_FrameStyle)) {
                ImGui::Text("%s", label.data());
            } // tag
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild();
            ImGui::PopID();
        }
    }


    const std::string station_info_popup_id = "info";
    std::optional<Station> station_info_result;
    std::string station_info_error;


    void
    request_station_info(const std::string& uuid)
    {
        if (uuid.empty())
            throw std::logic_error{"no UUID to request info."};

        auto server = Browser::get_server();
        if (server.empty())
            throw std::runtime_error{"not connected to a server."};

        rest::get_json("https://" + server + "/json/stations/byuuid",
                       {{"uuids", uuid}},
                       [](curl::easy&,
                          const json::value& response)
                       {
                           try {
                               const auto& list = response.as<json::array>();
                               if (list.empty())
                                   throw std::runtime_error{"station not found."};
                               station_info_result =
                                   Station::from_json(list.front().as<json::object>());
                               cout << "station info result" << endl;
                           }
                           catch (std::exception& e) {
                               station_info_error = e.what();
                               cout << "Failed to read station info: "
                                    << station_info_error
                                    << endl;
                           }
                       },
                       [](curl::easy&,
                          const std::exception& error)
                       {
                           station_info_error = error.what();
                           cout << "Error requesting station info: "
                                << station_info_error
                                << endl;
                       });
    }


    void
    open_station_info_popup(const std::string& uuid)
    {
        if (uuid.empty())
            return;

        station_info_result.reset();
        station_info_error.clear();

        try {
            request_station_info(uuid);
        }
        catch (std::exception& e) {
            station_info_error = e.what();
            cout << "Error requesting station info: " << station_info_error << endl;
        }

        ImGui::OpenPopup(station_info_popup_id);
    }


    void
    process_station_info_popup()
    {
        ImGui::SetNextWindowSize({1100, 600}, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Always,
                                {0.5f, 0.5f});
        if (ImGui::BeginPopup(station_info_popup_id,
                              ImGuiWindowFlags_NoSavedSettings)) {

            if (!station_info_error.empty()) {

                ImGui::Text("Error: %s", station_info_error.data());

            } else if (station_info_result) {

                if (ImGui::BeginTable("fields", 2,
                                      ImGuiTableFlags_None)) {

                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    show_info_row("name",         station_info_result->name);
                    show_link_row("url",          station_info_result->url);
                    show_link_row("url_resolved", station_info_result->url_resolved);
                    show_link_row("homepage",     station_info_result->homepage);
                    show_link_row("favicon",      station_info_result->favicon);
                    show_info_row("tags", utils::join(station_info_result->tags, ", "));
                    show_info_row("country_code", station_info_result->country_code);
                    show_info_row("language",     station_info_result->language);
                    show_info_row("uuid",         station_info_result->uuid);

                    show_info_row("votes",        station_info_result->votes);
                    show_info_row("click_count",  station_info_result->click_count);
                    show_info_row("click_trend",  station_info_result->click_trend);
                    show_info_row("bitrate",      station_info_result->bitrate);

                    ImGui::EndTable();
                }

            } else {

                ImGui::Text("Retrieving station info...");

            }

            ImGui::HandleDragScroll();
            ImGui::EndPopup();
        }
    }

} // namespace ui
