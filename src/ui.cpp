/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <sdl2xx/vec2.hpp>

#include "ui.hpp"

#include "Browser.hpp"
#include "Favorites.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
#include "imgui_extras.hpp"
#include "Player.hpp"
#include "rest.hpp"
#include "Station.hpp"
#include "string_utils.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace ui {

    const std::string station_details_popup_id = "details";
    std::string station_details_uuid;
    std::optional<StationEx> station_details_result;
    std::string station_details_error;

    void
    open_station_details_popup(const std::string& uuid);

    void
    process_station_details_popup(const std::string& uuid);



    const ImVec4&
    get_label_color()
        noexcept
    {
        const ImVec4* colors = ImGui::GetStyle().Colors;
        return colors[ImGuiCol_SeparatorActive];
    }


    void
    show_details_button(const Station& station)
    {

        {
            ImGui::DisabledGuard disable_no_uuid{station.uuid.empty()};
            if (ImGui::Button(ICON_FA_INFO_CIRCLE)) // 🛈
                open_station_details_popup(station.uuid);
            if (!station.uuid.empty())
                ImGui::SetItemTooltip("Show station details.");
        }
        process_station_details_popup(station.uuid);
    }


    void
    show_favicon(const Station& station)
    {
        if (station.favicon.empty())
            return;

        auto icon = IconManager::get(station.favicon);
        auto icon_size = icon->get_size();
        sdl::vec2 size = {128, 128};
        size.x = icon_size.x * size.y / icon_size.y;
        ImGui::Image(*IconManager::get(station.favicon), size);
        ImGui::SetItemTooltip(station.favicon);
    }


    void
    show_favorite_button(const Station& station)
    {
        if (Favorites::contains(station)) {
            if (ImGui::Button(ICON_FA_HEART)) // ♥
                Favorites::remove(station);
        } else {
            if (ImGui::Button(ICON_FA_HEART_O)) // ♡
                Favorites::add(station);
        }
    }


    void
    show_info_row(const std::string& label,
                  const std::string& value)
    {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextRightColored(get_label_color(), label);
        // show_last_bounding_box();

        ImGui::TableNextColumn();
        ImGui::TextWrapped(value);
        // show_last_bounding_box();
    }


    template<std::integral T>
    void
    show_info_row(const std::string& label,
                  T value)
    {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextRightColored(get_label_color(), label);
        // show_last_bounding_box();

        ImGui::TableNextColumn();
        const std::string fmt = "%" + string_utils::format(value);
        ImGui::TextWrapped(fmt.data(), value);
        // show_last_bounding_box();
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
        ImGui::TextRightColoredV(get_label_color(), fmt, args);
        va_end(args);
    }


    void
    show_link_row(const std::string& label,
                  const std::string& url)
    {
        ImGui::TableNextRow();

        ImGui::IDGuard label_id{label};

        ImGui::TableNextColumn();
        ImGui::TextRightColored(get_label_color(), label);

        ImGui::TableNextColumn();
        if (ImGui::TextLinkOpenURL(/*ICON_FA_LINK " " +*/ url, url)) {
#ifdef __WIIU__
            // TODO: show QR code
#endif
        }
    }


    void
    show_play_button(std::shared_ptr<Station>& station)
    {
        const sdl::vec2 button_size = {96, 96};
        if (Player::is_playing(station)) {
            if (ImGui::ImageButton("stop_button",
                                   *IconManager::get("ui/stop-button.svg"),
                                   button_size))
                Player::stop();
        } else {
            if (ImGui::ImageButton("play_button",
                                   *IconManager::get("ui/play-button.svg"),
                                   button_size))
                Player::play(station);
        }
    }


    void
    show_station_basic_info(const Station& station)
    {
        if (ImGui::ChildGuard basic_info_child{"basic_info",
                                               {0, 0},
                                               ImGuiChildFlags_AutoResizeY |
                                               ImGuiChildFlags_NavFlattened}) {

            ImGui::TextWrapped(station.name);

            if (!station.homepage.empty()) {
                if (ImGui::TextLinkOpenURL(station.homepage, station.homepage)) {
#ifdef __WIIU__
                    // TODO: show QR code
#endif
                }
            }

            bool has_country = false;
            if (!station.country_code.empty()) {
                has_country = true;
                auto name = Browser::get_country_name(station.country_code);
                show_boxed(ICON_FA_FLAG_O " " + station.country_code,
                           name ? *name : ""s);
            }

            if (!station.languages.empty()) {
                if (has_country)
                    ImGui::SameLine();
                for (auto& lang : station.languages) {
                    show_boxed(ICON_FA_LANGUAGE " " + lang,
                               "Language spoken in this broadcast.");
                    ImGui::SameLine();
                }
                ImGui::NewLine();
            }

        } // basic_info_child
    }


    void
    show_tags(const std::vector<std::string>& tags)
    {
        if (tags.empty())
            return;

        for (const auto& tag : tags) {
            show_boxed(ICON_FA_TAG " " + tag, {});
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }


    void
    request_station_details(const std::string& uuid)
    {
        if (uuid.empty())
            throw std::logic_error{"no UUID to request details."};

        auto server = Browser::get_server();
        if (server.empty())
            throw std::runtime_error{"not connected to a server."};

        station_details_uuid = uuid;

        rest::get_json("https://" + server + "/json/stations/byuuid",
                       {{"uuids", uuid}},
                       [](curl::easy&,
                          const json::value& response)
                       {
                           try {
                               const auto& list = response.as<json::array>();
                               if (list.empty())
                                   throw std::runtime_error{"station not found."};
                               const auto& obj = list.front().as<json::object>();
                               cout << "Station details:\n";
                               json::dump(obj, cout);
                               cout << endl;
                               station_details_result = Station::from_json(obj);
                           }
                           catch (std::exception& e) {
                               station_details_error = e.what();
                               cout << "ERROR: Failed to read station details: "
                                    << station_details_error
                                    << endl;
                           }
                       },
                       [](curl::easy&,
                          const std::exception& error)
                       {
                           station_details_error = error.what();
                           cout << "ERROR: requesting station details: "
                                << station_details_error
                                << endl;
                       });
    }


    void
    open_station_details_popup(const std::string& uuid)
    {
        if (uuid.empty())
            return;

        station_details_uuid.clear();
        station_details_result.reset();
        station_details_error.clear();

        try {
            request_station_details(uuid);
        }
        catch (std::exception& e) {
            station_details_error = e.what();
            cout << "ERROR: requesting station details: " << station_details_error << endl;
        }

        ImGui::OpenPopup(station_details_popup_id);
    }


    void
    process_station_details_popup(const std::string& uuid)
    {
        if (uuid.empty() || uuid != station_details_uuid)
            return;

        ImGui::SetNextWindowSize({1100, 600}, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                ImGuiCond_Always,
                                {0.5f, 0.5f});
        if (ImGui::PopupGuard station_details_popup{station_details_popup_id,
                                                    ImGuiWindowFlags_NoSavedSettings}) {

            if (!station_details_error.empty()) {

                ImGui::Text("Error: " + station_details_error);

            } else if (station_details_result) {

                if (ImGui::TableGuard fields_table{"fields", 2,
                                                   ImGuiTableFlags_None}) {

                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    show_info_row("name",         station_details_result->name);
                    show_link_row("url",          station_details_result->url);
                    show_link_row("url_resolved", station_details_result->url_resolved);
                    show_link_row("homepage",     station_details_result->homepage);
                    show_link_row("favicon",      station_details_result->favicon);
                    show_info_row("countrycode",  station_details_result->country_code);
                    show_info_row("language",     station_details_result->languages_str);
                    show_info_row("tags",         station_details_result->tags_str);
                    show_info_row("uuid",         station_details_result->uuid);

                    show_info_row("votes",        station_details_result->votes);
                    show_info_row("clickcount",   station_details_result->click_count);
                    show_info_row("clicktrend",   station_details_result->click_trend);
                    show_info_row("bitrate",      station_details_result->bitrate);
                    show_info_row("codec",        station_details_result->codec);

                } // fields_table

            } else {

                ImGui::Text("Retrieving station details...");

            }

        } else {

            station_details_uuid.clear();
            station_details_result.reset();
            station_details_error.clear();

        }
    }


    void
    show_boxed(const std::string& text,
               const std::string& tooltip)
    {
        ImGui::IDGuard text_id{text};

        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 size = ImGui::CalcTextSize(text)
            + 2 * style.FramePadding
            + style.FrameBorderSize * ImVec2{2, 2};
        // if (ImGui::GetCursorPosX() == ImGui::GetCursorStartPos().x)
        // const float spacing = style.ItemSpacing.x;
        const ImVec2 available = ImGui::GetContentRegionAvail();
        if (size.x > available.x)
            ImGui::NewLine();

        if (ImGui::ChildGuard boxed_child{"boxed",
                                          size,
                                          ImGuiChildFlags_FrameStyle}) {

            ImGui::Text(text);
            if (!tooltip.empty())
                ImGui::SetItemTooltip(tooltip);

        } // boxed_child
    }


    void
    show_boxed(const std::string& text)
    {
        show_boxed(text, {});
    }


    // DEBUG
    void
    show_last_bounding_box()
    {
        {
            auto min = ImGui::GetItemRectMin();
            auto max = ImGui::GetItemRectMax();
            ImU32 col = ImGui::GetColorU32(ImVec4{1.0f, 0.0f, 0.0f, 0.5f});
            auto draw_list = ImGui::GetForegroundDrawList();
            draw_list->AddRect(min, max, col);
        }
    }

} // namespace ui
