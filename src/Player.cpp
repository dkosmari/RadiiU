/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <chrono>
#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

#ifdef __WUT__
#include <coreinit/energysaver.h>
#endif

#include <curlxx/curl.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <sdl2xx/audio.hpp>

#include "Player.hpp"

#include "Browser.hpp"
#include "cfg.hpp"
#include "Favorites.hpp"
#include "humanize.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
#include "imgui_extras.hpp"
#include "json.hpp"
#include "radio_client.hpp"
#include "Recent.hpp"
#include "Station.hpp"
#include "ui.hpp"
#include "utils.hpp"


using std::cout;
using std::endl;
using std::chrono::system_clock;

using namespace std::literals;
using namespace std::placeholders;

using sdl::vec2;


namespace Player {

    enum class State {
        stopped,
        playing,
        stopping,
    };

    State state = State::stopped;

    std::shared_ptr<Station> station;


    struct TrackInfo {
        system_clock::time_point when;
        std::string title;
    };
    std::deque<TrackInfo> history;

    bool details_expanded;
    bool history_expanded;


    void
    history_add(const std::string& title);


    /*
     * RAII-managed resources are stored here.
     * Note that they're only allocated while playback is active.
     */
    struct Resources {

        radio_client radio;
        sdl::audio::device audio_dev;

        Resources(const std::string& url) :
            radio{url}
        {
            if (cfg::disable_apd) {
#ifdef __WUT__
                IMDisableAPD();
#else
                // TODO: write similar code for desktop, to prevent computer from
                // suspending.
#endif
            }
        }


        ~Resources()
        {
#ifdef __WUT__
            IMEnableAPD();
#endif
        }


        // Disallow moving.
        Resources(Resources&&) = delete;


        bool
        is_buffer_too_empty()
        {
            // if (cfg::player_buffer_size == 0)
            //     return false;

            // return total_bytes_fed < cfg::player_buffer_size * 1024u;
            return false;
        }


        void
        process_meta()
        {

        }


        void
        process()
        {
            try {
                radio.process();
                if (auto meta = radio.get_metadata())
                    if (meta->title)
                        history_add(*meta->title);

                if (is_buffer_too_empty()) {
                    // cout << "buffer too empty" << endl;
                    return;
                }

                if (!audio_dev) {
                    // see if we have enough bytes to initialize audio_dev properly.
                    if (auto radio_spec = radio.get_spec()) {
                        sdl::audio::spec spec;
                        spec.freq     = radio_spec->rate;
                        spec.channels = radio_spec->channels;
                        spec.format   = radio_spec->format;
                        spec.samples  = 4096;
                        audio_dev.create(nullptr, false, spec);
                        audio_dev.unpause();
                    } else
                        return;
                }

                if (!audio_dev) {
                    cout << "no audio dev yet" << endl;
                    return;
                }

                for (auto samples = radio.get_samples();
                     !samples.empty();
                     samples = radio.get_samples())
                    audio_dev.play(samples);

                // if (!is_buffer_too_full())
                //     easy.unpause();

            }
            catch (std::exception& e) {
                cout << "ERROR: Player::Resources::process(): " << e.what() << endl;
            }
        }

    }; // struct Resources

    std::optional<Resources> res;


    void
    load();

    void
    save();


    void
    initialize()
    {
        load();
    }


    void
    finalize()
    {
        save();
        res.reset();
    }


    void
    load()
    try {

        const auto root = json::load(cfg::base_dir / "player.json").as<json::object>();
        try_get(root, "details_expanded", details_expanded);
        try_get(root, "history_expanded", history_expanded);
    }
    catch (std::exception& e) {
        cout << "ERROR: Player::load(): " << e.what() << endl;
    }


    void
    save()
    try {
        json::object root;
        root["details_expanded"] = details_expanded;
        root["history_expanded"] = history_expanded;

        json::save(std::move(root), cfg::base_dir / "player.json");
    }
    catch (std::exception& e) {
        cout << "ERROR: Player::save(): " << e.what() << endl;
    }


    void
    play()
    {
        if (!station)
            return;

        if (state == State::playing)
            stop();

        cout << "Starting playback of station \"" << station->name << "\"" << endl;

        Recent::add(station);

        std::string url;
        if (!station->url_resolved.empty())
            url = station->url_resolved;

        if (url.empty())
            if (!station->url.empty())
                url = station->url; // TODO: implement parsing m3u playlist

        if (url.empty()) {
            cout << "No usable URL found" << endl;
            return;
        }
        cout << "Playing URL: " << url << endl;

        Browser::send_click(station);

        state = State::playing;

        res.emplace(url); // allocate and initialize resources here
    }


    void
    play(std::shared_ptr<Station>& st)
    {
        station = st;
        play();
    }


    void
    stop()
    {
        if (state == State::stopped)
            return;
        state = State::stopped;

        res.reset();
    }


    void
    process_logic()
    {
        if (state == State::stopped)
            return;

        if (state == State::stopping) {
            stop();
            return;
        }

        if (res)
            res->process();
    }


    void
    show_station(ImGuiID scroll_target)
    {
        if (!station) {
            if (ImGui::BeginChild("no_station",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_FrameStyle |
                                  ImGuiChildFlags_NavFlattened)) {
                ImGui::TextDisabled("No station set");
            } // no_station
            ImGui::EndChild();
            return;
        }

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

            } // actions
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("details",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {

                ui::show_favicon(*station);

                ImGui::SameLine();

                ui::show_station_basic_info(*station, scroll_target);

            } // details
            ImGui::HandleDragScroll(scroll_target);
            ImGui::EndChild();

        } // station
        ImGui::HandleDragScroll(scroll_target);
        ImGui::EndChild();
    }


    void
    show_stream(ImGuiID scroll_target)
    {

        if (ImGui::BeginChild("stream",
                              {0, 0},
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_FrameStyle |
                              ImGuiChildFlags_NavFlattened)) {

            ImGui::SetNextItemOpen(details_expanded);
            if (ImGui::CollapsingHeader("Stream details")) {

                details_expanded = true;

                ImGui::Indent();

                if (res && ImGui::BeginTable("metadata", 2)) {

                    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    if (const auto meta = res->radio.get_metadata()) {
                        if (meta->title)
                            ui::show_info_row("Title", *meta->title);
                        if (meta->artist)
                            ui::show_info_row("Artist", *meta->artist);
                        if (meta->album)
                            ui::show_info_row("Album", *meta->album);
                        if (meta->genre)
                            ui::show_info_row("Genre", *meta->genre);
                        for (auto& [k, v] : meta->extra)
                            ui::show_info_row(k, v);
                        // station metadata
                        if (meta->station_name)
                            ui::show_info_row("Name", *meta->station_name);
                        if (meta->station_genre)
                            ui::show_info_row("Genre", *meta->station_genre);
                        if (meta->station_description)
                            ui::show_info_row("Description", *meta->station_description);
                        if (meta->station_url)
                            ui::show_link_row("URL", *meta->station_url);
                    }

                    if (const auto info = res->radio.get_decoder_info()) {
                        if (!info->codec.empty())
                            ui::show_info_row("Codec", info->codec);
                        if (!info->bitrate.empty())
                            ui::show_info_row("Bitrate", info->bitrate);
                    }

                    ImGui::EndTable();

                    ImGui::Unindent();

                }

            } else
                details_expanded = false;

        } // stream
        ImGui::HandleDragScroll(scroll_target);
        ImGui::EndChild();
    }


    void
    show_history(ImGuiID scroll_target)
    {
        auto now = system_clock::now();

        if (ImGui::BeginChild("history",
                              {0, 0},
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_FrameStyle |
                              ImGuiChildFlags_NavFlattened)) {

            ImGui::SetNextItemOpen(history_expanded);
            if (ImGui::CollapsingHeader("Track history")) {

                history_expanded = true;

                ImGui::Indent();

                if (ImGui::BeginTable("table",
                                      2,
                                      ImGuiTableFlags_BordersInnerH)) {

                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    for (const auto& [when, title] : history | std::views::reverse) {

                        auto t = duration_cast<std::chrono::seconds>(now - when);
#if 0
                        std::string label = humanize::duration(t) + " ago";
#else
                        std::string label = humanize::duration_brief(t);
#endif
                        ui::show_info_row(label, title);

                    }

                    ImGui::EndTable();
                }

                ImGui::Unindent();

            } else
                history_expanded = false;

        } // history
        ImGui::HandleDragScroll(scroll_target);
        ImGui::EndChild();
    }


    void
    process_ui()
    {
        if (ImGui::BeginChild("player",
                              {0, 0},
                              ImGuiChildFlags_NavFlattened)) {
            auto scroll_target = ImGui::GetCurrentWindow()->ID;
            show_station(scroll_target);
            show_stream(scroll_target);
            show_history(scroll_target);
        } // player
        ImGui::HandleDragScroll();
        ImGui::EndChild();
    }


    bool
    is_playing(const Station& st)
    {
        if (state != State::playing || !station)
            return false;
        return st == *station;
    }


    bool
    is_playing(std::shared_ptr<Station>& st)
    {
        if (!st)
            return false;
        return is_playing(*st);
    }


    void
    history_add(const std::string& title)
    {
        if (!history.empty() && history.back().title == title)
            return;

        history.emplace_back(system_clock::now(), title);

        if (history.size() > cfg::player_history_limit)
            history.erase(history.begin(), history.begin() + cfg::player_history_limit);
    }

} // namespace Player
