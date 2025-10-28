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
#include <compare>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <regex>
#include <span>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#ifdef __WIIU__
#include <coreinit/time.h>
#endif

#include <SDL_stdinc.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <curlxx/easy.hpp>

#include "Browser.hpp"

#include "cfg.hpp"
#include "Favorites.hpp"
#include "humanize.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
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

using namespace std::literals;

using sdl::vec2;


namespace Browser {

    // Note: if safe_server is not empty, we are "connected".
    thread_safe<std::string> safe_server;
    std::atomic_bool finished_background_connect{false};

    thread_safe<std::vector<std::string>> safe_mirrors;
    std::jthread fetch_mirrors_thread;

    bool busy = false;
    bool options_visible = false;
    std::string filter_name;
    std::string filter_tag;
    std::string filter_country;


    std::regex tags_regex;


    enum class Order : unsigned {
        name_asc,
        name_desc,
        country_asc,
        country_desc,
        language_asc,
        language_desc,
        votes_desc,
        votes_asc,
        random,
    };

    const std::array order_strings {
        "name_asc",
        "name_desc",
        "country_asc",
        "country_desc",
        "language_asc",
        "language_desc",
        "votes_desc",
        "votes_asc",
        "random",
    };

    Order order = Order::name_asc;

    unsigned page_index = 0;
    std::vector<std::shared_ptr<Station>> stations;

    bool scroll_to_top = false;
    bool station_refresh_requested = false;

    // TODO: allow votes to expire after 10 min.
    struct VoteStatus {
        bool ok;
        std::string message;
    };
    std::unordered_map<std::string, VoteStatus> votes_cast;


    const std::string server_details_popup_id = "info";

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

    std::optional<ServerInfo> server_details_result;
    std::string server_details_error;


    struct Country {
        std::string code;
        std::string name;

        bool
        operator ==(const Country& other)
            const noexcept = default;

        std::strong_ordering
        operator <=>(const Country& other)
            const noexcept = default;
    };

    std::vector<Country> countries;

    std::vector<std::string> tags;


    // Tasks to run during process_logic(), only if server is connected.
    std::vector<std::function<void()>> pending_tasks;

    template<typename F>
    void
    queue_task(F&& func)
    {
        pending_tasks.push_back(std::forward<F>(func));
        assert(pending_tasks.size() < 10);
    }


    void
    dispatch_tasks()
    {
        auto server = safe_server.load();
        if (server.empty())
            return;

        for (auto& task : pending_tasks)
            task();
        pending_tasks.clear();
    }


    void
    fetch_stations();

    void
    load();

    void
    save();

    void
    apply_options();

    void
    request_server_details();


    void
    queue_refresh_countries();

    void
    queue_refresh_tags();


    void
    fetch_countries();

    void
    fetch_tags();


    const std::string&
    by_code(const Country& c);

    const std::string&
    by_name(const Country& c);


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
        return Order::name_asc;
    }


    std::tuple<const char*, const char*>
    to_args(Order o)
    {
        switch (o) {
            default:
            case Order::name_asc:
                return {"name", "false"};
            case Order::name_desc:
                return {"name", "true"};
            case Order::country_asc:
                return {"country", "false"};
            case Order::country_desc:
                return {"country", "true"};
            case Order::language_asc:
                return {"language", "false"};
            case Order::language_desc:
                return {"language", "true"};
            case Order::votes_desc:
                return {"votes", "true"};
            case Order::votes_asc:
                return {"votes", "false"};
            case Order::random:
                return {"random", nullptr};
        }
    }


    std::string
    to_label(Order o)
    {
        switch (o) {
            default:
            case Order::name_asc:
                return "Name " ICON_FA_SORT_ALPHA_ASC;
            case Order::name_desc:
                return "Name " ICON_FA_SORT_ALPHA_DESC;
            case Order::country_asc:
                return "Country " ICON_FA_SORT_ALPHA_ASC;
            case Order::country_desc:
                return "Country " ICON_FA_SORT_ALPHA_DESC;
            case Order::language_asc:
                return "Language " ICON_FA_SORT_ALPHA_ASC;
            case Order::language_desc:
                return "Language " ICON_FA_SORT_ALPHA_DESC;
            case Order::votes_desc:
                return "Votes " ICON_FA_SORT_AMOUNT_DESC;
            case Order::votes_asc:
                return "Votes " ICON_FA_SORT_AMOUNT_ASC;
            case Order::random:
                return "Random";
        }
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
                    throw std::runtime_error{"failed resolving \"all.api.radio-browser.info\": "
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
                            throw std::runtime_error{"failed to look up name for \""
                                                     + to_string(addr) + "\""};
                        if (nr.result.name)
                            new_mirrors.insert(std::move(*nr.result.name));
                    }
                    catch (std::exception& e) {
                        cout << "ERROR: " << e.what() << endl;
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
            cout << "ERROR: fetch_mirrors(): " << e.what() << endl;
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
                cout << "Mirror \"" << name << "\" returned:\n";
                dump(result, cout);
                safe_server.store(name);
                finished_background_connect = true;
                break;
            }
            catch (std::exception& e) {
                cout << "Mirror \"" << name << "\" failed to respond: " << e.what() << endl;
            }
        }
    }


    bool
    try_open_file(std::ifstream& stream,
                  const std::filesystem::path& filename)
    {
        if (!exists(filename))
            return false;
        stream.open(filename);
        return stream.is_open();
    }


    void
    load_tags_regex()
    try {
        std::ifstream input;
        if (!try_open_file(input, cfg::base_dir / "tags.ignore"))
            if (!try_open_file(input, utils::get_content_path() / "tags.ignore"))
                throw std::runtime_error{"could not find tags.ignore"};
        std::string line;
        std::string full_regex;
        unsigned counter = 0;
        while (getline(input, line)) {
            if (line.empty())
                continue;
            if (counter)
                full_regex += "|";
            full_regex += "(?:" + line + ")";
            ++counter;
        }
        tags_regex.assign(full_regex,
                          std::regex_constants::ECMAScript |
                          std::regex_constants::optimize);
        cout << "tags_regex has " << counter << " rules" << endl;
        cout << full_regex << endl;
    }
    catch (std::exception& e) {
        cout << "ERROR: load_tags_regex(): " << e.what() << endl;
    }


    void
    initialize()
    {
        load_tags_regex();
        load();
        connect();
    }


    void
    finalize()
    {
        fetch_mirrors_thread = {};
        pending_tasks.clear();
        save();
    }


    void
    load()
    try {
        const auto root = json::load(cfg::base_dir / "browser.json").as<json::object>();
        if (root.contains("filter")) {
            const auto& filter = root.at("filter").as<json::object>();
            try_get(filter, "name", filter_name);
            try_get(filter, "tag", filter_tag);
            try_get(filter, "country", filter_country);
        }

        if (auto v = try_get<json::string>(root, "order"))
            order = to_order(*v);

        if (auto v = try_get<json::integer>(root, "page"))
            page_index = *v - 1;
    }
    catch (std::exception& e) {
        cout << "ERROR: Browser::load(): " << e.what() << endl;
    }


    void
    save()
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
        root["page"] = page_index + 1;

        json::save(std::move(root), cfg::base_dir / "browser.json");
    }
    catch (std::exception& e) {
        cout << "ERROR: Browser::save(): " << e.what() << endl;
    }


    void
    process_logic()
    {
        if (finished_background_connect) {
            finished_background_connect = false;
            busy = false;
            cout << "Finished selecting mirror: " << safe_server.load() << endl;
            queue_refresh_stations();
        }

        dispatch_tasks();
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
    fetch_stations()
    {
        if (busy) {
            cout << "ERROR: fetch_stations() called while busy." << endl;
            return;
        }

        if (!station_refresh_requested) {
            cout << "ERROR: fetch_stations() called while not requested." << endl;
            return;
        }

        station_refresh_requested = false;

        std::string server = safe_server.load();
        if (server.empty()) {
            cout << "ERROR: fetch_stations() called when not conntected." << endl;
            return;
        }

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

        if (order != Order::name_asc) {
            auto [arg_order, arg_reverse] = to_args(order);
            params["order"] = arg_order;
            if (arg_reverse)
                params["reverse"] = arg_reverse;
        }

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
                               stations.push_back(std::make_shared<Station>(Station::from_json(entry.as<json::object>())));
                           }
                           busy = false;
                       },
                       [](curl::easy&,
                          const std::exception& error)
                       {
                           cout << "ERROR: JSON request failed: " << error.what() << endl;
                           busy = false;
                       });
    }


    void
    connect()
    {
        busy = true;
        auto server = safe_server.lock();
        if (*server != cfg::server)
            *server = cfg::server;

        if (server->empty())
            fetch_mirrors_thread = std::jthread{fetch_mirrors_and_select_random};
        else
            busy = false;

        queue_refresh_countries();
        queue_refresh_stations();
        queue_refresh_tags();
    }


    void
    queue_refresh_stations()
    {
        if (station_refresh_requested)
            return;

        station_refresh_requested = true;
        scroll_to_top = true;

        queue_task(fetch_stations);
    }


    void
    apply_options()
    {
        page_index = 0;
        queue_refresh_stations();
    }


    void
    reset_options()
    {
        filter_name = "";
        filter_tag = "";
        filter_country = "";
        order = Order::name_asc;
    }


    void
    process_server_details_popup()
    {
        // ImGui::SetNextWindowSize({550, 0}, ImGuiCond_Always);
        if (ImGui::BeginPopup(server_details_popup_id,
                              ImGuiWindowFlags_NoSavedSettings)) {

            auto server = safe_server.load();
            ImGui::SeparatorText("Server status for " + server);

            if (!server_details_result) {

                if (!server_details_error.empty())
                    ImGui::ValueWrapped("Error: ", server_details_error);
                else
                    ImGui::Text("Fetching server details...");

            } else {

                if (ImGui::BeginTable("fields", 2,
                                      ImGuiTableFlags_None)) {
                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    using ui::show_info_row;
                    show_info_row("software_version", server_details_result->software_version);
                    show_info_row("stations",         server_details_result->stations);
                    show_info_row("stations_broken",  server_details_result->stations_broken);
                    show_info_row("tags",             server_details_result->tags);
                    show_info_row("clicks_last_hour", server_details_result->clicks_last_hour);
                    show_info_row("clicks_last_day",  server_details_result->clicks_last_day);
                    show_info_row("languages",        server_details_result->languages);
                    show_info_row("countries",        server_details_result->countries);

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

            if (ImGui::Button(ICON_FA_REFRESH))
                connect();
            ImGui::SetItemTooltip("Reconnect to server, or try a different random mirror.");

            ImGui::SameLine();

            std::string server = safe_server.load();
            if (!server.empty()) {

                if (ImGui::Button(ICON_FA_INFO_CIRCLE)) {
                    request_server_details();
                    ImGui::OpenPopup(server_details_popup_id);
                }
                ImGui::SetItemTooltip("Show server details.");

                ImGui::SameLine();

                process_server_details_popup();
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

                    const float filters_width = 500;

                    ImGui::TextUnformatted(ICON_FA_FILTER " Filters");

                    ImGui::SetNextItemWidth(filters_width);
                    ImGui::InputText("Name", &filter_name);

                    ImGui::SetNextItemWidth(filters_width);
                    ImGui::SetNextWindowSizeConstraints({0.0f, 0.0f},
                                                        {1200.0f, FLT_MAX});
                    if (ImGui::BeginCombo("Tag",
                                          filter_tag,
                                          ImGuiComboFlags_HeightLargest)) {
                        static ImGuiTextFilter filter{filter_tag.data()};
                        if (ImGui::IsWindowAppearing()) {
                            ImGui::SetKeyboardFocusHere();
                            SDL_strlcpy(filter.InputBuf,
                                        filter_tag.data(),
                                        sizeof filter.InputBuf);
                            filter.Build();
                        }
                        filter.Draw("##tag_filter", 900);
                        if (ImGui::Selectable("(empty)", filter_tag.empty()))
                            filter_tag.clear();
                        for (auto& tag : tags) {
                            const bool is_selected = filter_tag == tag;
                            if (filter.PassFilter(tag.data()))
                                if (ImGui::Selectable(tag, is_selected))
                                    filter_tag = tag;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::SetNextItemWidth(filters_width);
                    std::string display_country;
                    if (!filter_country.empty()) {
                        display_country = filter_country;
                        auto country_name = get_country_name(filter_country);
                        if (country_name)
                            display_country += " - " + *country_name;
                    }
                    if (ImGui::BeginCombo("Country",
                                          display_country,
                                          ImGuiComboFlags_HeightLargest)) {
                        static ImGuiTextFilter filter{display_country.data()};
                        if (ImGui::IsWindowAppearing()) {
                            ImGui::SetKeyboardFocusHere();
                            SDL_strlcpy(filter.InputBuf,
                                        filter_country.data(),
                                        sizeof filter.InputBuf);
                            filter.Build();
                        }
                        filter.Draw("##country_filter");
                        if (ImGui::Selectable("(none)", filter_country.empty()))
                            filter_country.clear();
                        for (const auto& country : countries) {
                            const bool is_selected = filter_country == country.code;
                            const std::string entry_name = country.code + " - " + country.name;
                            if (filter.PassFilter(entry_name.data()))
                                if (ImGui::Selectable(entry_name, is_selected))
                                    filter_country = country.code;
                        }
                        ImGui::EndCombo();
                    }

                    // TODO: add language filter

                } // filters
                ImGui::EndChild();

                ImGui::SameLine();

                if (ImGui::BeginChild("sorting",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeX |
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_FrameStyle |
                                      ImGuiChildFlags_NavFlattened)) {

                    ImGui::TextUnformatted(ICON_FA_SORT " Order");

                    ImGui::SetNextItemWidth(280);
                    if (ImGui::BeginCombo("##Order",
                                          to_label(order),
                                          ImGuiComboFlags_HeightLargest)) {
                        for (unsigned i = 0; i < order_strings.size(); ++i) {
                            Order o{i};
                            if (ImGui::Selectable(to_label(o), order == o))
                                order = o;
                        }
                        ImGui::EndCombo();
                    }

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
                    ImGui::SetItemTooltip("Reset browser options to default.");

                    if (ImGui::Button("Apply")) {
                        options_visible = false;
                        apply_options();
                    }
                    ImGui::SetItemTooltip("Apply the current browser options.");

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
        const float parent_width = ImGui::GetContentRegionAvail().x;
        const ImVec2 global_pos = ImGui::GetCursorScreenPos();
        ImGui::SetNextWindowPos({global_pos.x + parent_width / 2.0f, global_pos.y + 0.0f},
                                ImGuiCond_Always,
                                {0.5f, 0.0f});
        if (ImGui::BeginChild("navigation",
                              {0, 0},
                              ImGuiChildFlags_AutoResizeX |
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_NavFlattened)) {

            const bool first_page = page_index == 0;
            const bool last_page = stations.size() < cfg::browser_page_limit;

            ImGui::BeginDisabled(first_page);

            // 100⏪
            if (ImGui::Button("100" ICON_FA_ANGLE_DOUBLE_LEFT) && !busy) {
                if (page_index >= 100)
                    page_index -= 100;
                else
                    page_index = 0;
                queue_refresh_stations();
            }
            ImGui::SetItemTooltip("Go back 100 pages.");

            ImGui::SameLine();

            // 10⏪
            if (ImGui::Button("10" ICON_FA_ANGLE_DOUBLE_LEFT) && !busy) {
                if (page_index >= 10)
                    page_index -= 10;
                else
                    page_index = 0;
                queue_refresh_stations();
            }
            ImGui::SetItemTooltip("Go back 10 pages.");

            ImGui::SameLine();

            // ⏴
            if (ImGui::Button(ICON_FA_ANGLE_LEFT) && !busy) {
                if (page_index > 0)
                    --page_index;
                queue_refresh_stations();
            }
            ImGui::SetItemTooltip("Go back one page.");

            ImGui::EndDisabled();

            ImGui::SameLine();

            static unsigned page_number;
            page_number = page_index + 1;
            const float page_width = 200;
            ImGui::SetNextItemWidth(page_width);
            unsigned max_page_num = UINT_MAX;
            if (last_page)
                max_page_num = page_number;
            ImGui::Drag("##page"s, page_number, 1u, max_page_num, 0.05f);
            page_index = page_number - 1;
            if (ImGui::IsItemDeactivatedAfterEdit())
                queue_refresh_stations();

            ImGui::SameLine();

            ImGui::BeginDisabled(last_page);

            // ⏵
            if (ImGui::Button(ICON_FA_ANGLE_RIGHT) && !busy) {
                ++page_index;
                queue_refresh_stations();
            }
            ImGui::SetItemTooltip("Advance one page.");

            ImGui::SameLine();

            // ⏩10
            if (ImGui::Button(ICON_FA_ANGLE_DOUBLE_RIGHT "10") && !busy) {
                page_index += 10;
                queue_refresh_stations();
            }
            ImGui::SetItemTooltip("Advance 10 pages.");

            ImGui::SameLine();

            // ⏩100
            if (ImGui::Button(ICON_FA_ANGLE_DOUBLE_RIGHT "100") && !busy) {
                page_index += 100;
                queue_refresh_stations();
            }
            ImGui::SetItemTooltip("Advance 100 pages.");

            ImGui::EndDisabled();
        }
        ImGui::EndChild();
    }


    void
    show_station(std::shared_ptr<Station>& station,
                 ImGuiID scroll_target)
    {
        ImGui::PushID(station.get());

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

                ui::show_favorite_button(*station);

                ImGui::SameLine();

                ui::show_details_button(*station);

                auto vote_record = votes_cast.find(station->uuid);
                const bool voted = vote_record != votes_cast.end();
                bool ok = voted ? vote_record->second.ok : false;
                std::string vote_label = (ok
                                          ? ICON_FA_THUMBS_UP " "
                                          : ICON_FA_THUMBS_O_UP " ")
                                         + humanize::value(station->votes);
                ImGui::BeginDisabled(voted);
                if (ImGui::Button(vote_label))
                    send_vote(station);
                if (voted)
                    ImGui::SetItemTooltip("%s", vote_record->second.message.data());
                else
                    ImGui::SetItemTooltip("Vote for this station.");

                ImGui::EndDisabled();

            } // actions
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("details",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {

                ui::show_favicon(*station);

                ImGui::SameLine();

                ui::show_station_basic_info(*station, scroll_target);

                if (ImGui::BeginChild("extra_info",
                                      {0, 0},
                                      ImGuiChildFlags_AutoResizeY |
                                      ImGuiChildFlags_NavFlattened)) {

                    char click_text[32];
                    std::snprintf(click_text, sizeof click_text,
                                  ICON_FA_BAR_CHART " %" PRIu64 " (%+" PRId64 ")",
                                  station->click_count,
                                  station->click_trend);
                    ui::show_boxed(click_text,
                                   "Daily total clicks and trend.",
                                   scroll_target);

                    if (station->bitrate) {
                        ImGui::SameLine();
                        ui::show_boxed(ICON_FA_HEADPHONES " "
                                       + std::to_string(station->bitrate) + " kbps",
                                       "The advertised stream quality.",
                                       scroll_target);
                    }

                    ImGui::SameLine();

                    ui::show_boxed("MP3",
                                   "The codec used in this broadcast.",
                                   scroll_target);

                    ui::show_tags(station->tags, scroll_target);
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

#if 0
            // Disabled until ImGui fixes navigation.
            const float content_width = ImGui::GetContentRegionAvail().x;
            if (page_index > 0)
                if (ImGui::Button("⏴ Go to page " + std::to_string(page_index - 1 + 1),
                                  {content_width, 0.0f})) {
                    --page_index;
                    queue_refresh_stations();
                }
#endif

            for (auto& station_ptr : stations)
                show_station(station_ptr, scroll_target);

#if 0
            // Disabled until ImGui fixes navigation.
            if (stations.size() == cfg::browser_page_limit)
                if (ImGui::Button("Go to page " + std::to_string(page_index + 1 + 1) + " ⏵",
                                  {content_width, 0.0f})) {
                    ++page_index;
                    queue_refresh_stations();
                }
#endif
        }
        ImGui::HandleDragScroll();
        ImGui::EndChild(); // stations

        ImGui::EndDisabled();
    }


    void
    send_click(const std::string& uuid,
               std::function<void()> on_success)
    {
        if (uuid.empty())
            return;

        auto server = safe_server.load();
        if (server.empty())
            return;

        rest::get_json("https://" + server + "/json/url/" + uuid,
                       [on_success=std::move(on_success)](curl::easy&,
                                                          const json::value& response)
                       {
                           try {
                               cout << "click response: ";
                               json::dump(response, cout);
                               auto obj = response.as<json::object>();
                               if (obj.contains("ok"))
                                   cout << "Click confirmed for "
                                        << obj.at("name").as<json::string>()
                                        << endl;
                               if (on_success)
                                   on_success();
                           }
                           catch (std::exception& e) {
                               cout << "ERROR: Browser::send_click(): " << e.what() << endl;
                           }
                       });
    }


    void
    send_click(std::shared_ptr<Station>& station_ptr)
    {
        if (!station_ptr || station_ptr->uuid.empty())
            return;

        send_click(station_ptr->uuid,
                   [station_ptr]
                   {
                       Browser::refresh_station_async(station_ptr);
                   });
    }


    void
    send_vote(const std::string& uuid,
              std::function<void()> on_success)
    {
        if (uuid.empty())
            return;

        auto server = safe_server.load();
        if (server.empty())
            return;

        rest::get_json("https://" + server + "/json/vote/" + uuid,
                       [uuid, on_success=std::move(on_success)](curl::easy&,
                                                                const json::value& response)
                       {
                           try {
                               const auto& obj = response.as<json::object>();
                               bool ok = obj.at("ok").as<bool>();
                               std::string message;
                               if (obj.contains("message")) {
                                   message = obj.at("message").as<json::string>();
                                   cout << message << endl;
                               }
                               votes_cast.emplace(uuid, VoteStatus{ok, std::move(message)});
                               if (on_success)
                                   on_success();
                           }
                           catch (std::exception& e) {
                               cout << "ERROR: Browser::send_vote(): " << e.what() << endl;
                           }
                       });
    }


    void
    send_vote(std::shared_ptr<Station>& station_ptr)
    {
        if (!station_ptr || station_ptr->uuid.empty())
            return;

        send_vote(station_ptr->uuid,
                  [station_ptr]
                  {
                      Browser::refresh_station_async(station_ptr);
                  });
    }


    void
    refresh_station_async(std::shared_ptr<Station> station_ptr)
    {
        if (!station_ptr || station_ptr->uuid.empty())
            return;

        auto server = safe_server.load();
        if (server.empty())
            return;

        rest::request_params_t params;
        params["uuids"] = station_ptr->uuid;

        rest::get_json("https://" + server + "/json/stations/byuuid",
                       params,
                       [station_ptr](curl::easy&,
                                     const json::value& response)
                       {
                           try {
                               const auto& list = response.as<json::array>();
                               if (list.size() != 1)
                                   throw std::runtime_error{"incorrect array size: "
                                                            + std::to_string(list.size())};
                               Station updated =
                                   Station::from_json(list.front().as<json::object>());
                               *station_ptr = std::move(updated);
                           }
                           catch (std::exception& e) {
                               cout << "ERROR: querying station: " << e.what() << endl;
                           }
                       });
    }


    void
    request_server_details()
    {
        server_details_result.reset();
        server_details_error.clear();

        auto server = safe_server.load();
        if (server.empty())
            return;

        rest::get_json("https://" + server + "/json/stats",
                       [](curl::easy&,
                          const json::value& response)
                       {
                           try {
                               server_details_result = ServerInfo::from_json(response.as<json::object>());
                           }
                           catch (std::exception& e) {
                               server_details_error = e.what();
                               cout << "ERROR: failed to read server stats: "
                                    << server_details_error
                                    << endl;
                           }
                       });
    }


    void
    queue_refresh_countries()
    {
        queue_task(fetch_countries);
    }


    void
    queue_refresh_tags()
    {
        queue_task(fetch_tags);
    }


    void
    fetch_countries()
    {
        auto server = safe_server.load();
        if (server.empty()) {
            cout << "ERROR: fetch_countries() called while not connected." << endl;
            return;
        }

        rest::get_json("https://" + server + "/json/countries",
                       [](curl::easy&,
                          const json::value& response)
                       {
                           try {
                               countries.clear();
                               const auto& list = response.as<json::array>();
                               for (const auto& entry : list) {
                                   const auto& obj = entry.as<json::object>();
                                   std::string code = obj.at("iso_3166_1").as<json::string>();
                                   std::string name = obj.at("name").as<json::string>();
                                   countries.emplace_back(std::move(code), std::move(name));
                               }
                               cout << "Got " << countries.size() << " countries" << endl;
                               std::ranges::sort(countries, {}, by_code);
                           }
                           catch (std::exception& e) {
                               cout << "ERROR: failed to read countries list: "
                                    << e.what()
                                    << endl;
                           }
                       });
    }


    void
    fetch_tags()
    {
        auto server = safe_server.load();
        if (server.empty()) {
            cout << "ERROR: fetch_tags() called while not connected." << endl;
            return;
        }

        rest::get_json("https://" + server + "/json/tags",
                       [](curl::easy&,
                          const json::value& response)
                       {
                           try {
                               tags.clear();
                               std::smatch matches;
                               const auto& list = response.as<json::array>();
                               for (const auto& entry : list) {
                                   const auto& obj = entry.as<json::object>();
                                   std::string name = obj.at("name").as<json::string>();
                                   // ignore some bogus tags
                                   if (name.size() < 2 || name.size() > 32)
                                       continue;
                                   if (regex_search(name, matches, tags_regex) &&
                                       matches.length() > 0) {
                                       // cout << "Ignored tag: " << name << endl;
                                       // cout << "RE match: " << matches.str() << endl;
                                       continue;
                                   }
                                   tags.push_back(std::move(name));
                               }
                               cout << "Got " << tags.size() << " tags" << endl;
                               std::ranges::sort(tags);
                           }
                           catch (std::exception& e) {
                               cout << "ERROR: failed to read tags list: "
                                    << e.what()
                                    << endl;
                           }
                       });
    }


    const std::string*
    get_country_name(const std::string& code)
    {
        if (code.empty())
            return nullptr;
        auto it = std::ranges::lower_bound(countries, code, {}, by_code);
        if (it == countries.end())
            return nullptr;
        if (it->code != code)
            return nullptr;
        return &it->name;
    }


    const std::string&
    by_code(const Country& c)
    {
        return c.code;
    }


    const std::string&
    by_name(const Country& c)
    {
        return c.name;
    }

} // namespace Browser
