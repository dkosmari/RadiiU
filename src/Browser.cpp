/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <exception>
#include <filesystem>
#include <iostream>
#include <random>
#include <span>
#include <thread>
#include <unordered_set>
#include <utility>

#ifdef __WIIU__
#include <coreinit/time.h>
#endif

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <curlxx/easy.hpp>

#include "Browser.hpp"

#include "cfg.hpp"
#include "constants.hpp"
#include "Favorites.hpp"
#include "humanize.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "net/address.hpp"
#include "net/resolver.hpp"
#include "Player.hpp"
#include "rest.hpp"
#include "Station.hpp"
#include "thread_safe.hpp"
#include "ui.hpp"
#include "utils.hpp"


using std::cout;
using std::endl;

using sdl::vec2;


namespace Browser {

    // Note: if safe_server is not empty, we are "connected".
    thread_safe<std::string> safe_server;
    std::atomic_bool pending_connect{false};

    thread_safe<std::vector<std::string>> safe_mirrors;
    std::jthread fetch_mirrors_thread;


    bool busy = false;
    bool options_visible = false;
    std::string filter_name;
    std::string filter_tag;
    std::string filter_country;

    enum class Order : unsigned {
        name,
        country,
        language,
        votes,
        random,
    };

    const std::array order_strings = {
        "name",
        "country",
        "language",
        "votes",
        "random",
    };

    Order order = Order::name;
    bool reverse = false;

    unsigned page_index = 0;
    std::vector<Station> stations;

    bool scroll_to_top = false;
    bool need_refresh = false;


    // TODO: allow votes to expire after 24 hours.
    std::unordered_set<std::string> votes_cast;


    const std::string server_info_popup_id = "info";

    struct ServerInfo {

        std::string software_version;
        unsigned stations = 0;
        unsigned stations_broken = 0;
        unsigned tags = 0;
        unsigned clicks_last_hour = 0;
        unsigned clicks_last_day = 0;
        unsigned languages = 0;
        unsigned countries = 0;

        static
        ServerInfo
        from_json(const json::object& obj)
        {
            ServerInfo result;
            result.software_version = obj.at("software_version").as<json::string>();
            result.stations         = obj.at("stations").as<json::integer>();
            result.stations_broken  = obj.at("stations_broken").as<json::integer>();
            result.tags             = obj.at("tags").as<json::integer>();
            result.clicks_last_hour = obj.at("clicks_last_hour").as<json::integer>();
            result.clicks_last_day  = obj.at("clicks_last_day").as<json::integer>();
            result.languages        = obj.at("languages").as<json::integer>();
            result.countries        = obj.at("countries").as<json::integer>();
            return result;
        }

    }; // struct ServerInfo

    std::optional<ServerInfo> server_info_result;
    std::string server_info_error;


    void
    refresh();

    void
    load();

    void
    save();

    void
    apply_options();

    void
    request_server_info();


    std::string
    to_string(Order o)
    {
        auto idx = static_cast<unsigned>(o);
        if (idx >= order_strings.size())
            idx = 0;
        return order_strings[idx];
    }


    Order
    to_order(const std::string& s)
    {
        for (unsigned i = 0; i < order_strings.size(); ++i)
            if (s == order_strings[i])
                return Order{i};
        return Order::name;
    }


    bool
    fetch_mirrors(std::stop_token stopper)
    {
        try {
            std::vector<net::address> addresses;
            {
                net::resolver::address_resolver ar;
                ar.param.type = net::socket::type::tcp;
                ar.process("all.api.radio-browser.info");

                if (stopper.stop_requested())
                    return false;

                if (ar.error.message)
                    throw std::runtime_error{"failed resolving all.api.radio-browser.info: "
                                             + *ar.error.message};
                for (const auto& entry : ar.result.entries)
                    addresses.push_back(entry.addr);
            }
            cout << "Found " << addresses.size() << " mirrors" << endl;
            std::unordered_set<std::string> new_mirrors;
            {
                net::resolver::name_resolver nr;
                for (const auto& addr : addresses) {
                    try {
                        nr.process(addr);

                        if (stopper.stop_requested())
                            return false;

                        if (nr.error.message)
                            throw std::runtime_error{"failed to look up name for "
                                                     + to_string(addr)};
                        if (nr.result.name)
                            new_mirrors.insert(std::move(*nr.result.name));
                    }
                    catch (std::exception& e) {
                        cout << "Failed to look up name for " << addr << endl;
                    }
                }
            }

            if (stopper.stop_requested())
                return false;

            auto mirrors = safe_mirrors.lock();
            mirrors->clear();
            for (const auto& name : new_mirrors)
                mirrors->push_back(name);

            return true;
        }
        catch (std::exception& e) {
            cout << "error fetching mirrors: " << e.what() << endl;
            return false;
        }
    }


    void
    fetch_mirrors_and_select_random(std::stop_token stopper)
    {
        if (!fetch_mirrors(stopper))
            return;
        std::vector<std::string> local_mirrors = safe_mirrors.load();

#ifdef __WIIU__
        std::uint64_t now = OSGetTime();
        std::seed_seq rnd_seq{
            static_cast<std::uint32_t>(now >> 32),
            static_cast<std::uint32_t>(now >> 0 )
        };
#else
        std::random_device rnd_dev;
        std::seed_seq rnd_seq{
            rnd_dev(),
            rnd_dev()
        };
#endif
        std::minstd_rand rnd_engine{rnd_seq};
        std::ranges::shuffle(local_mirrors, rnd_engine);
        // try each mirror until one that works
        for (auto name : local_mirrors) {
            if (stopper.stop_requested())
                return;
            try {
                auto result = rest::get_json_sync("https://" + name + "/json/stats");
                cout << "Mirror " << name << " returned:\n";
                dump(result, cout);
                safe_server.store(name);
                pending_connect = true;
                break;
            }
            catch (std::exception& e) {
                cout << "Mirror " << name << " failed to respond: " << e.what() << endl;
            }
        }
    }


    void
    initialize()
    {
        connect();
        load();
    }


    void
    finalize()
    {
        fetch_mirrors_thread = {};
        save();
    }


    void
    load()
    {
        try {
            const auto root = json::load(cfg::base_dir / "browser.json").as<json::object>();
            if (root.contains("filter")) {
                const auto& filter = root.at("filter").as<json::object>();
                if (filter.contains("name"))
                    filter_name = filter.at("name").as<json::string>();
                if (filter.contains("tag"))
                    filter_tag = filter.at("tag").as<json::string>();
                if (filter.contains("country"))
                    filter_country = filter.at("country").as<json::string>();
            }

            if (root.contains("order"))
                order = to_order(root.at("order").as<json::string>());

            if (root.contains("reverse"))
                reverse = root.at("reverse").as<bool>();

            if (root.contains("page"))
                page_index = root.at("page").as<json::integer>() - 1;

            queue_update_stations();
        }
        catch (std::exception& e) {
            cout << "Error loading browser: " << e.what() << endl;
        }
    }


    void
    save()
    {
        try {
            json::object filter;
            if (!filter_name.empty())
                filter["name"] = filter_name;
            if (!filter_tag.empty())
                filter["tag"] = filter_tag;
            if (!filter_country.empty())
                filter["country"] = filter_country;

            json::object root;
            if (!filter.empty())
                root["filter"] = std::move(filter);

            root["order"] = to_string(order);
            root["reverse"] = reverse;
            root["page"] = 1 + page_index;

            json::save(std::move(root), cfg::base_dir / "browser.json");
        }
        catch (std::exception& e) {
            cout << "Error saving browser: " << e.what() << endl;
        }
    }


    void
    process_logic()
    {
        if (pending_connect) {
            pending_connect = false;
            busy = false;
            cout << "Finished selecting mirror: " << safe_server.load() << endl;
            need_refresh = true;
        }
        if (need_refresh)
            refresh();
    }


    void
    refresh_mirrors()
    {
        fetch_mirrors_thread = std::jthread{fetch_mirrors};
    }


    std::vector<std::string>
    get_mirrors()
    {
        return safe_mirrors.load();
    }


    std::string
    get_server()
    {
        return safe_server.load();
    }


    void
    refresh()
    {
        if (busy)
            return;

        std::string server = safe_server.load();
        if (server.empty())
            return;

        cout << "Refreshing page index " << page_index << " ..." << endl;

        busy = true;

        std::map<std::string, std::string> params;
        params["offset"] = std::to_string(cfg::browser_page_limit * page_index);
        params["limit"] = std::to_string(cfg::browser_page_limit);
        params["codec"] = "MP3";
        params["hidebroken"] = "true";

        if (!filter_name.empty())
            params["name"] = filter_name;
        if (!filter_tag.empty())
            params["tag"] = filter_tag;
        if (!filter_country.empty())
            params["countrycode"] = filter_country;

        if (order != Order::name)
            params["order"] = to_string(order);

        if (reverse)
            params["reverse"] = "true";

        rest::get_json("https://" + server + "/json/stations/search",
                       params,
                       [](curl::easy&,
                          const json::value& response)
                       {
                           stations.clear();
                           auto& list = response.as<json::array>();
                           cout << "Received " << list.size() << " stations" << endl;
                           for (auto& entry : list) {
                               // ensure the page size limit is respected
                               if (stations.size() >= cfg::browser_page_limit)
                                   break;
                               stations.push_back(Station::from_json(entry.as<json::object>()));
                           }
                           busy = false;
                       },
                       [](curl::easy&,
                          const std::exception& error)
                       {
                           cout << "JSON request failed: " << error.what() << endl;
                           busy = false;
                       });

        need_refresh = false;
    }


    void
    connect()
    {
        busy = true;
        auto server = safe_server.lock();
        if (*server != cfg::server) {
            *server = cfg::server;
            need_refresh = true;
        }
        if (server->empty())
            need_refresh = true;

        if (server->empty())
            fetch_mirrors_thread = std::jthread{fetch_mirrors_and_select_random};
        else
            busy = false;
    }


    void
    queue_update_stations()
    {
        need_refresh = true;
        scroll_to_top = true;
    }


    void
    apply_options()
    {
        page_index = 0;
        queue_update_stations();
    }


    void
    reset_options()
    {
        filter_name = "";
        filter_tag = "";
        filter_country = "";
        order = Order::name;
        reverse = false;
    }


    // DEBUG
    void
    show_last_bounding_box()
    {
        {
            auto min = ImGui::GetItemRectMin();
            auto max = ImGui::GetItemRectMax();
            ImU32 col = ImGui::GetColorU32(ImVec4{1.0f, 0.0f, 0.0f, 1.0f});
            auto draw_list = ImGui::GetForegroundDrawList();
            draw_list->AddRect(min, max, col);
        }
    }


    void
    process_server_info_popup()
    {
        // ImGui::SetNextWindowSize({550, 0}, ImGuiCond_Always);
        if (ImGui::BeginPopup(server_info_popup_id,
                              ImGuiWindowFlags_NoSavedSettings)) {

            auto server = safe_server.load();
            ImGui::SeparatorText("Server status for " + server);

            if (!server_info_result) {

                if (!server_info_error.empty())
                    ImGui::ValueWrapped("Error: ", server_info_error);
                else
                    ImGui::Text("Fetching server info...");

            } else {

                if (ImGui::BeginTable("fields", 2,
                                      ImGuiTableFlags_None)) {
                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    using ui::show_info_row;
                    show_info_row("software_version", server_info_result->software_version);
                    show_info_row("stations",         server_info_result->stations);
                    show_info_row("stations_broken",  server_info_result->stations_broken);
                    show_info_row("tags",             server_info_result->tags);
                    show_info_row("clicks_last_hour", server_info_result->clicks_last_hour);
                    show_info_row("clicks_last_day",  server_info_result->clicks_last_day);
                    show_info_row("languages",        server_info_result->languages);
                    show_info_row("countries",        server_info_result->countries);

                    ImGui::EndTable();
                }
            }

            ImGui::HandleDragScroll();
            ImGui::EndPopup();
        }
    }


    void
    show_status()
    {
        if (ImGui::BeginChild("status",
                              {0, 0},
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_NavFlattened)) {

            if (ImGui::Button("🔃"))
                connect();

            ImGui::SameLine();

            std::string server = safe_server.load();
            if (!server.empty()) {

                if (ImGui::Button("🛈")) {
                    request_server_info();
                    ImGui::OpenPopup(server_info_popup_id);
                }

                ImGui::SameLine();

                process_server_info_popup();
                ImGui::Text("%s", server.data());

            }

        }
        ImGui::EndChild();
    }


    void
    show_options()
    {
        if (ImGui::BeginChild("options",
                              {0, 0},
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_FrameStyle |
                              ImGuiChildFlags_NavFlattened)) {

            ImGui::SetNextItemOpen(options_visible);
            if (ImGui::CollapsingHeader("Options")) {

                options_visible = true;

                ImGui::Indent();

                if (ImGui::BeginChild("filters",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeX |
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_FrameStyle |
                                      ImGuiChildFlags_NavFlattened)) {

                    ImGui::TextUnformatted("Filters");

                    ImGui::SetNextItemWidth(400);
                    ImGui::InputText("Name", &filter_name);

                    // TODO: should use list of tags
                    ImGui::SetNextItemWidth(400);
                    ImGui::InputText("Tag", &filter_tag);

                    // TODO: should use list of countries
                    ImGui::SetNextItemWidth(400);
                    ImGui::InputText("Country", &filter_country);

                } // filters
                ImGui::EndChild();

                ImGui::SameLine();

                if (ImGui::BeginChild("sorting",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeX |
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_FrameStyle |
                                      ImGuiChildFlags_NavFlattened)) {

                    ImGui::TextUnformatted("Order");

                    ImGui::SetNextItemWidth(220);
                    if (ImGui::BeginCombo("##Order", to_string(order))) {
                        for (unsigned i = 0; i < order_strings.size(); ++i) {
                            Order o{i};
                            if (ImGui::Selectable(to_string(o), order == o))
                                order = o;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Checkbox("Reverse", &reverse);

                } // sorting
                ImGui::EndChild();

                ImGui::SameLine();

                if (ImGui::BeginChild("buttons",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeX |
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_NavFlattened)) {

                    if (ImGui::Button("Reset"))
                        reset_options();

                    if (ImGui::Button("Apply")) {
                        options_visible = false;
                        apply_options();
                    }

                } // buttons
                ImGui::EndChild();

                ImGui::Unindent();

            } else
                options_visible = false;

        } // options
        ImGui::EndChild();

    }


    void
    show_navigation()
    {
        if (ImGui::Button("100⏪") && !busy) {
            if (page_index >= 100)
                page_index -= 100;
            else
                page_index = 0;
            queue_update_stations();
        }
        ImGui::SameLine();

        if (ImGui::Button("10⏪") && !busy) {
            if (page_index >= 10)
                page_index -= 10;
            else
                page_index = 0;
            queue_update_stations();
        }
        ImGui::SameLine();

        if (ImGui::Button("⏴") && !busy) {
            if (page_index > 0)
                --page_index;
            queue_update_stations();
        }
        ImGui::SameLine();

        ImGui::AlignTextToFramePadding();
        ImGui::Value("Page", page_index + 1);
        ImGui::SameLine();

        if (ImGui::Button("⏵") && !busy) {
            ++page_index;
            queue_update_stations();
        }
        ImGui::SameLine();

        if (ImGui::Button("⏩10") && !busy) {
            page_index += 10;
            queue_update_stations();
        }
        ImGui::SameLine();

        if (ImGui::Button("⏩100") && !busy) {
            page_index += 100;
            queue_update_stations();
        }
    }


    void
    show_station(const Station& station,
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

                if (Favorites::contains(station.uuid)) {
                    if (ImGui::Button("♥"))
                        Favorites::remove(station.uuid);
                } else {
                    if (ImGui::Button("♡"))
                        Favorites::add(station);
                }

                ImGui::SameLine();

                if (ImGui::Button("🛈"))
                    ui::open_station_info_popup(station.uuid);
                ui::process_station_info_popup();

                bool voted = votes_cast.contains(station.uuid);
                std::string vote_label = "👍" + humanize::value(station.votes);
                ImGui::BeginDisabled(voted);
                if (ImGui::Button(vote_label))
                    send_vote(station.uuid);
                ImGui::EndDisabled();

            } // actions
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("details",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {

                ui::show_favicon(station.favicon);

                ImGui::SameLine();

                ui::show_station_basic_info(station, scroll_target);

                if (ImGui::BeginChild("extra_info",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_NavFlattened)) {

                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::BulletText("🎧 %" PRIu64 " (%+" PRId64 ")",
                                      station.click_count,
                                      station.click_trend);
                    if (station.bitrate) {
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::BulletText("👂 %u kbps", station.bitrate);
                    }

                    ui::show_tags(station.tags, scroll_target);
                }
                ImGui::HandleDragScroll(scroll_target);
                ImGui::EndChild();
            } // extra_info
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
        ImGui::BeginDisabled(busy);

        show_status();

        show_options();

        show_navigation();

        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::BeginChild("stations")) {
            auto scroll_target = ImGui::GetCurrentWindow()->ID;
            if (scroll_to_top) {
                ImGui::SetScrollY(0);
                scroll_to_top = false;
            }
            for (const auto& station : stations)
                show_station(station, scroll_target);
        }
        ImGui::HandleDragScroll();
        ImGui::EndChild(); // stations

        ImGui::EndDisabled();
    }


    void
    send_click(const std::string& uuid)
    {
        if (uuid.empty())
            return;
        auto server = safe_server.load();
        if (server.empty())
            return;
        rest::get_json("https://" + server + "/json/url/" + uuid,
                       [](curl::easy&,
                          const json::value& response)
                       {
                           auto obj = response.as<json::object>();
                           if (obj.contains("name"))
                               cout << "Clicked on "
                                    << obj.at("name").as<json::string>()
                                    << endl;
                       });
    }


    void
    send_vote(const std::string& uuid)
    {
        if (uuid.empty())
            return;

        auto server = safe_server.load();
        if (server.empty())
            return;

        rest::get_json("https://" + server + "/json/vote/" + uuid,
                       [uuid](curl::easy&,
                          const json::value& response)
                       {
                           const auto& obj = response.as<json::object>();
                           if (obj.contains("message"))
                                            cout << obj.at("message").as<json::string>()
                                                 << endl;
                           try {
                               if (obj.at("ok").as<bool>())
                                   votes_cast.insert(uuid);
                           }
                           catch (std::exception& e) {
                               cout << "Unexpected error reading status of vote: "
                                    << e.what()
                                    << endl;
                           }
                       });
    }


    void
    request_server_info()
    {
        server_info_result.reset();
        server_info_error.clear();

        auto server = safe_server.load();
        if (server.empty())
            return;

        rest::get_json("https://" + server + "/json/stats",
                       [](curl::easy&,
                          const json::value& response)
                       {
                           try {
                               server_info_result = ServerInfo::from_json(response.as<json::object>());
                           }
                           catch (std::exception& e) {
                               server_info_error = e.what();
                               cout << "Failed to read server stats: "
                                    << server_info_error
                                    << endl;
                           }
                       });
    }

} // namespace Browser
