/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <ranges>
#include <vector>

#ifdef __WUT__
#include <coreinit/energysaver.h>
#endif

#include <curlxx/curl.hpp>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include <sdl2xx/audio.hpp>

#include "PlayerTab.hpp"

#include "App.hpp"
#include "humanize.hpp"
#include "IconsFontAwesome4.h"
#include "ImageLoader.hpp"
#include "LogManager.hpp"
#include "radio_client.hpp"
#include "RecentTab.hpp"
#include "Serializer.hpp"
#include "Settings.hpp"
#include "StationClicking.hpp"
#include "StationDetailsPopup.hpp"
#include "StationVoting.hpp"
#include "UI.hpp"


using std::chrono::system_clock;

using namespace std::literals;
using namespace std::placeholders;

using Settings::cfg;


namespace PlayerTab {

    namespace {

        // Types

        struct TrackInfo {
            system_clock::time_point when{};
            std::string title{};
        };


        struct State {
            bool details_expanded{};
            bool history_expanded{};
            std::deque<TrackInfo> history{};
        };

    } // namespace



    State state;

    ConstStationPtr station;

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
            if (cfg.disable_apd) {
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
            // if (cfg.player_buffer_size == 0)
            //     return false;

            // return total_bytes_fed < cfg.player_buffer_size * 1024u;
            return false;
        }


        void
        process()
        {
            try {
                radio.process();

                if (auto meta = radio.get_metadata()) {
                    if (meta->title) {
                        if (meta->artist)
                            history_add(*meta->artist + " - " + *meta->title);
                        else
                            history_add(*meta->title);
                    } else
                        history_add({});
                }

                if (is_buffer_too_empty()) {
                    // LOG_DEBUG("buffer too empty");
                    return;
                }

                if (!audio_dev) {
                    // see if we have enough bytes to initialize audio_dev properly.
                    if (auto radio_spec = radio.get_spec()) {
                        sdl::audio::spec spec;
                        spec.freq     = radio_spec->rate;
                        spec.channels = radio_spec->channels;
                        spec.format   = radio_spec->format;
                        spec.samples  = 8192;
                        audio_dev.create(nullptr, false, spec);
                        audio_dev.unpause();
                    } else
                        return;
                }

                if (!audio_dev) {
                    LOG_DEBUG("no audio dev yet");
                    return;
                }

                for (auto samples = radio.get_samples();
                     !samples.empty();
                     samples = radio.get_samples())
                    audio_dev.play(samples);

            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
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
        Serializer::load(state, filename);
    }
    catch (std::exception& e) {
        LOG_ERROR("{}", e.what());
    }


    void
    save()
    try {
        auto filename = App::get_config_path() / "player.json";
        Serializer::save(state, filename);
    }
    catch (std::exception& e) {
        LOG_ERROR("{}", e.what());
    }


    void
    play()
    {
        if (!station)
            return;

        if (res && res->radio.current_state != radio_client::state::stopped)
            stop();

        LOG_INFO("Starting playback of station {:?}", station->name);

        RecentTab::add(station);

        LOG_INFO("Playing url={:?}, url_resolved={:?}",
                 station->url,
                 station->url_resolved);

        // allocate and initialize resources here
        res.emplace(station->url, station->url_resolved);

        StationClicking::click(station);
    }


    void
    play(StationPtr& st)
    {
        station = st;
        play();
    }


    void
    stop()
    {
        res.reset();
    }


    void
    process_logic()
    {
        if (res)
            res->process();
    }


    void
    show_station()
    {
        using namespace ImGui::RAII;

        if (!station) {
            if (Child no_station_child{
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

        if (Child station_frame{
                "station_frame",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened
            }) {

            if (Child actions{
                    "actions",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeX |
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened
                }) {

                UI::PlayButton(station);

                UI::FavoriteButton(*station);

                ImGui::SameLine();

                if (StationDetailsPopup::Button(station->stationuuid))
                    StationDetailsPopup::open(station->stationuuid);

                StationVoting::Button(station);

            } // actions

            ImGui::SameLine();

            if (Child details{
                    "details",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened
                }) {

                UI::StationInfo(*station, true);

            } // details

        } // station_frame

    }


    void
    show_stream()
    {
        using namespace ImGui::RAII;

        if (Child stream_child{
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

                Font smaller{nullptr, 0.8f * App::get_default_font_size()};

                Indent indenter;
                if (Table metadata_table{"metadata", 2}) {

                    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    if (const auto meta = res->radio.get_metadata()) {

                        UI::InfoRowOpt("Title", meta->title);
                        UI::InfoRowOpt("Artist", meta->artist);

                        if (meta->cover_art && !meta->cover_art->empty()) {
                            auto available = ImGui::GetContentRegionAvail();
                            const sdl::vec2 max_size = {
                                static_cast<int>(available.x),
                                0
                            };
                            auto art = ImageLoader::get(*meta->cover_art, max_size);
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            UI::Label("Cover art");
                            ImGui::TableNextColumn();
                            UI::Image(*art);
                            ImGui::SetItemTooltip(*meta->cover_art);
                        }

                        UI::InfoRowOpt("Album", meta->album);
                        UI::InfoRowOpt("Genre", meta->genre);
                        UI::InfoRowOpt("Date", meta->date);

                        for (auto& [k, v] : meta->extra)
                            UI::InfoRow(k, v);

                        // station metadata
                        UI::InfoRowOpt("Station Name", meta->station_name);
                        UI::InfoRowOpt("Station Genre", meta->station_genre);
                        UI::InfoRowOpt("Station Description", meta->station_description);
                        UI::InfoRowOpt("Station URL", meta->station_url);
                    }

                    if (const auto info = res->radio.get_decoder_info()) {
                        if (!info->codec.empty())
                            UI::InfoRow("Codec", info->codec);
                        if (!info->bitrate.empty())
                            UI::InfoRow("Bitrate", info->bitrate);
                    }

                }

            }

        } // stream_child

    }


    void
    show_history()
    {
        using namespace ImGui::RAII;

        auto now = system_clock::now();

        if (Child history_child{
                "history",
                {0, 0},
                ImGuiChildFlags_AutoResizeY |
                ImGuiChildFlags_FrameStyle |
                ImGuiChildFlags_NavFlattened
            }) {

            ImGui::SetNextItemOpen(state.history_expanded);
            if ((state.history_expanded = ImGui::CollapsingHeader("Track history"))) {

                Font smaller{nullptr, 0.8f * App::get_default_font_size()};
                Indent indenter;

                if (Table table{"table",
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
                        UI::InfoRow(label, title);

                    }

                } // table

            }

        } // history_child
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        if (Child player_child{
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
        if (!res)
            return false;
        if (!station)
            return false;
        if (res->radio.current_state == radio_client::state::stopped)
            return false;
        if (&st == station.get())
            return true;
        return st == *station;
    }


    void
    history_add(const std::string& title)
    {
        if (state.history.back().title == title)
            return;

        state.history.emplace_back(system_clock::now(), title);

        if (state.history.size() > cfg.player_history_limit)
            state.history.erase(state.history.begin(),
                                state.history.begin() + cfg.player_history_limit);
    }

} // namespace PlayerTab
