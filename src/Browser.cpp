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
#include <misc/cpp/imgui_stdlib.h>
#include <curlxx/easy.hpp>

#include "Browser.hpp"

#include "cfg.hpp"
#include "Favorites.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "Player.hpp"
#include "rest.hpp"
#include "Station.hpp"
#include "net/address.hpp"
#include "net/resolver.hpp"
#include "utils.hpp"
#include "thread_safe.hpp"


using std::cout;
using std::endl;

using sdl::vec2;


namespace Browser {

    thread_safe<std::string> safe_server;
    std::atomic_bool pending_connect{false};

    thread_safe<std::vector<std::string>> safe_mirrors;
    std::jthread fetch_mirrors_thread;


    bool busy = false;
    bool filters_visible = false;
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


    void
    refresh();

    void
    load();

    void
    save();

    void
    apply_options();


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

            update_list();
        }
        catch (std::exception& e) {
            cout << "Error loading browser state: " << e.what() << endl;
        }
    }


    void
    save()
    {
        try {
            json::object obj;

            json::object fobj;
            if (!filter_name.empty())
                fobj["name"] = filter_name;
            if (!filter_tag.empty())
                fobj["tag"] = filter_tag;
            if (!filter_country.empty())
                fobj["country"] = filter_country;
            if (!fobj.empty())
                obj["filter"] = std::move(fobj);

            obj["order"] = to_string(order);
            obj["reverse"] = reverse;

            obj["page"] = 1 + page_index;

            const std::filesystem::path old_browser = cfg::base_dir / "browser.json";
            const std::filesystem::path new_browser = cfg::base_dir / "browser.json.new";
#ifdef __WIIU__
            if (exists(new_browser))
                remove(new_browser);
#endif
            json::save(std::move(obj), new_browser);
#ifdef __WIIU__
            if (exists(old_browser))
                remove(old_browser);
#endif
            rename(new_browser, old_browser);
        }
        catch (std::exception& e) {
            cout << "Error saving browser state: " << e.what() << endl;
        }
    }


    void
    process()
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
        params["offset"] = std::to_string(cfg::browser_page_size * page_index);
        params["limit"] = std::to_string(cfg::browser_page_size);
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
                               if (stations.size() >= cfg::browser_page_size)
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
    update_list()
    {
        need_refresh = true;
        scroll_to_top = true;
    }


    void
    apply_options()
    {
        page_index = 0;
        update_list();
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


    void
    show_navigation()
    {
        // ImGui::AlignTextToFramePadding();
        if (ImGui::Button("100⏪") && !busy) {
            if (page_index >= 100)
                page_index -= 100;
            else
                page_index = 0;
            update_list();
        }
        ImGui::SameLine();

        // ImGui::AlignTextToFramePadding();
        if (ImGui::Button("10⏪") && !busy) {
            if (page_index >= 10)
                page_index -= 10;
            else
                page_index = 0;
            update_list();
        }
        ImGui::SameLine();

        // ImGui::AlignTextToFramePadding();
        if (ImGui::Button("⏴") && !busy) {
            if (page_index > 0)
                --page_index;
            update_list();
        }
        ImGui::SameLine();

        ImGui::AlignTextToFramePadding();
        ImGui::Value("Page", page_index + 1);
        ImGui::SameLine();

        // ImGui::AlignTextToFramePadding();
        if (ImGui::Button("⏵") && !busy) {
            ++page_index;
            update_list();
        }
        ImGui::SameLine();

        // ImGui::AlignTextToFramePadding();
        if (ImGui::Button("⏩10") && !busy) {
            page_index += 10;
            update_list();
        }
        ImGui::SameLine();

        // ImGui::AlignTextToFramePadding();
        if (ImGui::Button("⏩100") && !busy) {
            page_index += 100;
            update_list();
        }
    }


    void
    show_station(const Station& station,
                 ImGuiID drag_target)
    {
        if (ImGui::BeginChild(station.uuid.data(), {0, 0},
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_FrameStyle |
                              ImGuiChildFlags_NavFlattened)) {

            if (ImGui::BeginChild("actions", {0, 0},
                                  ImGuiChildFlags_AutoResizeX |
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {
                if (ImGui::ImageButton("play_button",
                                       *IconManager::get("ui/play-button.png"),
                                       vec2{64, 64})) {
                    Player::play(station);
                }
                bool is_favorited = Favorites::contains(station.uuid);
                if (is_favorited) {
                    if (ImGui::Button("♥"))
                        Favorites::remove(station.uuid);
                } else {
                    if (ImGui::Button("♡"))
                        Favorites::add(station);
                }
            }
            ImGui::HandleDragScroll(drag_target);
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("details", {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {

                if (ImGui::BeginChild("first_line", {0, 0},
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_NavFlattened)) {

                    if (!station.favicon.empty()) {
                        auto icon = IconManager::get(station.favicon);
                        auto icon_size = icon->get_size();
                        vec2 size = {128, 128};
                        size.x = icon_size.x * size.y / icon_size.y;
                        ImGui::Image(*IconManager::get(station.favicon), size);
                        ImGui::SameLine();
                    }

                    ImGui::TextWrapped("%s", station.name.data());
                    // WORKAROUND: ImGui cuts off the text.
                    ImGui::Spacing();
                }
                ImGui::HandleDragScroll(drag_target);
                ImGui::EndChild();

                if (ImGui::BeginChild("second_line", {0, 0},
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_NavFlattened)) {

                    bool voted = votes_cast.contains(station.uuid);
                    std::string vote_label = "👍" + std::to_string(station.votes);
                    ImGui::BeginDisabled(voted);
                    if (ImGui::Button(vote_label))
                        send_vote(station.uuid);
                    ImGui::EndDisabled();

                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::BulletText("Clicks: %" PRIu64 " (%+" PRId64 ")",
                                      station.click_count,
                                      station.click_trend);
                    if (station.bitrate) {
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
                        ImGui::BulletText("%u kbps", station.bitrate);
                    }
                    if (!station.country_code.empty()) {
                        ImGui::SameLine();
                        ImGui::AlignTextToFramePadding();
#if 0
                        std::string region_glyph;
                        for (char c : station.country_code) {
                            if ('A' <= c && c <= 'Z') {
                                region_glyph += "🇦";
                                region_glyph.back() += c - 'A';
                            }
                        }
                        ImGui::BulletText("%s", region_glyph.data());
#else
                        ImGui::BulletText("%s", station.country_code.data());
                    }
#endif
                    ImGui::Spacing();
                }
                ImGui::HandleDragScroll(drag_target);
                ImGui::EndChild();
            }
            ImGui::HandleDragScroll(drag_target);
            ImGui::EndChild();
        }
        ImGui::HandleDragScroll(drag_target);
        ImGui::EndChild();
    }


    void
    process_ui()
    {
        ImGui::BeginDisabled(busy);

        ImGui::SetNextItemOpen(filters_visible);
        if (ImGui::CollapsingHeader("Options")) {
            filters_visible = true;
            if (ImGui::BeginChild("filters", {0, 0},
                                  ImGuiChildFlags_FrameStyle |
                                  ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) {

                ImGui::TextUnformatted("Filters");

                ImGui::SetNextItemWidth(400);
                ImGui::InputText("Name", &filter_name);

                // TODO: should use list of tags
                ImGui::SetNextItemWidth(400);
                ImGui::InputText("Tag", &filter_tag);

                // TODO: should use list of countries
                ImGui::SetNextItemWidth(400);
                ImGui::InputText("Country", &filter_country);

            }
            ImGui::HandleDragScroll();
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("sorting", {0, 0},
                                  ImGuiChildFlags_FrameStyle |
                                  ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) {
                ImGui::TextUnformatted("Order");
                ImGui::SetNextItemWidth(250);
                if (ImGui::BeginCombo("##Order", to_string(order))) {
                    for (unsigned i = 0; i < order_strings.size(); ++i) {
                        Order o{i};
                        if (ImGui::Selectable(to_string(o), order == o)) {
                            order = o;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::Checkbox("Reverse", &reverse);
            }
            ImGui::HandleDragScroll();
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("buttons", {0, 0},
                                  ImGuiChildFlags_FrameStyle |
                                  ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY)) {
                if (ImGui::Button("Reset"))
                    reset_options();
                if (ImGui::Button("Apply")) {
                    filters_visible = false;
                    apply_options();
                }
            }
            ImGui::HandleDragScroll();
            ImGui::EndChild();


        } else
            filters_visible = false;

        show_navigation();

        if (ImGui::BeginChild("stations")) {
            auto drag_target = ImGui::GetCurrentWindow()->ID;
            if (scroll_to_top) {
                ImGui::SetScrollY(0);
                scroll_to_top = false;
            }
            for (const auto& station : stations)
                show_station(station, drag_target);
        }

        ImGui::HandleDragScroll();
        ImGui::EndChild();

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
                           auto obj = response.as<json::object>();
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



} // namespace Browser
