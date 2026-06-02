/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
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

#include <glaze/json.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include <sdl2xx/audio.hpp>

#include "Player.hpp"

#include "App.hpp"
#include "Browser.hpp"
#include "cfg.hpp"
#include "Favorites.hpp"
#include "humanize.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
#include "radio_client.hpp"
#include "Recent.hpp"
#include "Station.hpp"
#include "StationDetailsPopup.hpp"
#include "UI.hpp"


using std::cout;
using std::endl;
using std::chrono::system_clock;

using namespace std::literals;
using namespace std::placeholders;

using sdl::vec2;


namespace Player {

    struct TrackInfo {
        system_clock::time_point when{};
        std::string title{};
    };

    struct State {
        bool details_expanded{};
        bool history_expanded{};
        std::deque<TrackInfo> history{};
    };

    State state;

    // TODO: the radio client keeps track of the state.
    enum class PlaybackState {
        stopped,
        playing,
        stopping,
    };

    PlaybackState playback_state = PlaybackState::stopped;

    std::shared_ptr<Station> station;

    void
    history_add(const std::string& title);


    /*
     * RAII-managed resources are stored here.
     * Note that they're only allocated while playback is active.
     */
    struct Resources {

        radio_client radio;
        sdl::audio::device audio_dev;

        Resources(const std::string& url,
                  const std::string& url_resolved) :
            radio{url, url_resolved, App::get_user_agent()}
        {
            if (cfg::state.disable_apd) {
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
            // if (cfg::state.player_buffer_size == 0)
            //     return false;

            // return total_bytes_fed < cfg::state.player_buffer_size * 1024u;
            return false;
        }


        void
        process()
        {
            try {
                radio.process();
                if (auto meta = radio.get_metadata())
                    if (meta->title) {
                        if (meta->artist)
                            history_add(*meta->artist + " - " + *meta->title);
                        else
                            history_add(*meta->title);
                    }

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
        auto filename = App::get_config_path() / "player.json";
        glz::ex::read_file_json(state, filename.c_str(), std::string{});
    }
    catch (std::exception& e) {
        cout << "ERROR: Player::load(): " << e.what() << endl;
    }


    void
    save()
    try {
        auto filename = App::get_config_path() / "player.json";
        glz::ex::write_file_json(state, filename.c_str(), std::string{});
    }
    catch (std::exception& e) {
        cout << "ERROR: Player::save(): " << e.what() << endl;
    }


    void
    play()
    {
        if (!station)
            return;

        if (playback_state == PlaybackState::playing)
            stop();

        cout << "Starting playback of station \"" << station->name << "\"" << endl;

        Recent::queue_add(station);

        cout << "Playing url=\"" << station->url
             << "\", url_resolved=\"" << station->url_resolved
             << endl;

        Browser::send_click(station);

        // allocate and initialize resources here
        res.emplace(station->url, station->url_resolved);

        playback_state = PlaybackState::playing;
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
        if (playback_state == PlaybackState::stopped)
            return;
        playback_state = PlaybackState::stopped;

        res.reset();
    }


    void
    process_logic()
    {
        if (playback_state == PlaybackState::stopped)
            return;

        if (playback_state == PlaybackState::stopping) {
            stop();
            return;
        }

        if (res)
            res->process();
    }


    void
    show_station()
    {
        if (!station) {
            if (ImGui::RAII::Child no_station_child{
                    "no_station",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_FrameStyle |
                    ImGuiChildFlags_NavFlattened
                }) {

                ImGui::TextDisabled("No station set");

            } // no_station_child

            return;
        }

        if (ImGui::RAII::Child station_child{
                "station",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened
            }) {

            if (ImGui::RAII::Child actions_child{
                    "actions",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeX |
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened
                }) {

                UI::show_play_button(station);

                UI::show_favorite_button(*station);

                ImGui::SameLine();

                if (StationDetailsPopup::show_button(station->stationuuid))
                    StationDetailsPopup::open(station->stationuuid);

            } // actions_child

            ImGui::SameLine();

            if (ImGui::RAII::Child details_child{
                    "details",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened
                }) {

                UI::show_favicon(*station);

                ImGui::SameLine();

                UI::show_station_basic_info(*station);

            } // details_child

        } // station_child

    }


    void
    show_stream()
    {
        if (ImGui::RAII::Child stream_child{
                "stream",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened
            }) {

            ImGui::SetNextItemOpen(state.details_expanded);
            if ((state.details_expanded = ImGui::CollapsingHeader("Stream details"))) {

                if (!res)
                    return;

                ImGui::RAII::Indent indenter;
                if (ImGui::RAII::Table metadata_table{"metadata", 2}) {

                    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    if (const auto meta = res->radio.get_metadata()) {
                        if (meta->title)
                            UI::show_info_row("Title", *meta->title);
                        if (meta->artist)
                            UI::show_info_row("Artist", *meta->artist);
                        if (meta->album)
                            UI::show_info_row("Album", *meta->album);
                        if (meta->genre)
                            UI::show_info_row("Genre", *meta->genre);
                        if (meta->cover_art && !meta->cover_art->empty()) {
                            auto art = IconManager::get(*meta->cover_art);
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            UI::show_label("Cover art");
                            ImGui::TableNextColumn();
                            UI::show_image(*art);
                            ImGui::SetItemTooltip("%s", meta->cover_art->data());
                        }
                        for (auto& [k, v] : meta->extra)
                            UI::show_info_row(k, v);
                        // station metadata
                        if (meta->station_name && !meta->station_name->empty())
                            UI::show_info_row("Name", *meta->station_name);
                        if (meta->station_genre && !meta->station_genre->empty())
                            UI::show_info_row("Genre", *meta->station_genre);
                        if (meta->station_description && !meta->station_description->empty())
                            UI::show_info_row("Description", *meta->station_description);
                        if (meta->station_url && !meta->station_url->empty())
                            UI::show_link_row("URL", *meta->station_url);
                    }

                    if (const auto info = res->radio.get_decoder_info()) {
                        if (!info->codec.empty())
                            UI::show_info_row("Codec", info->codec);
                        if (!info->bitrate.empty())
                            UI::show_info_row("Bitrate", info->bitrate);
                    }

                }

            }

        } // stream_child

    }


    void
    show_history()
    {
        auto now = system_clock::now();

        if (ImGui::RAII::Child history_child{
                "history",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened
            }) {

            ImGui::SetNextItemOpen(state.history_expanded);
            if ((state.history_expanded = ImGui::CollapsingHeader("Track history"))) {

                ImGui::RAII::Indent indenter;

                if (ImGui::RAII::Table table{"table",
                                            2,
                                            ImGuiTableFlags_BordersInnerH}) {

                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    for (const auto& [when, title] : state.history | std::views::reverse) {

                        auto t = duration_cast<std::chrono::seconds>(now - when);
#if 0
                        std::string label = humanize::duration(t) + " ago";
#else
                        std::string label = humanize::duration_brief(t);
#endif
                        UI::show_info_row(label, title);

                    }

                } // table

            }

        } // history_child
    }


    void
    process_ui()
    {
        if (ImGui::RAII::Child player_child{
                "player",
                {0, 0},
                ImGuiChildFlags_NavFlattened
            }) {

            show_station();
            show_stream();
            show_history();

        } // player_child

        StationDetailsPopup::process_ui();
    }


    bool
    is_playing(const Station& st)
    {
        if (playback_state != PlaybackState::playing || !station)
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
        if (!state.history.empty() && state.history.back().title == title)
            return;

        state.history.emplace_back(system_clock::now(), title);

        if (state.history.size() > cfg::state.player_history_limit)
            state.history.erase(state.history.begin(),
                                state.history.begin() + cfg::state.player_history_limit);
    }

} // namespace Player
