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
#include "humanize.hpp"


using std::chrono::system_clock;

using namespace std::literals;
using namespace std::placeholders;

using Settings::cfg;


namespace PlayerTab {

    namespace {

        constexpr std::size_t queued_history_size = 5 * 60;


        /*-------*/
        /* Types */
        /*-------*/

        struct TrackInfo {
            system_clock::time_point when{};
            std::string title{};
        };


        struct State {
            bool details_expanded{};
            bool history_expanded{};
            std::deque<TrackInfo> history{};
        };


        struct PlaybackResources {

            sdl::audio::device audio_dev;
            sdl::audio::spec audio_spec;
            std::vector<std::byte> samples_buffer;
            radio_client radio;
            std::array<float, queued_history_size> queued_history;
            std::size_t last_queued_history = 0;


            PlaybackResources(const std::string& url,
                              const std::string& url_resolved);

            ~PlaybackResources();

            // Disallow moving.
            PlaybackResources(PlaybackResources&&) = delete;


            bool
            is_buffer_too_empty();


            void
            process();


            void
            update_queued_history();

        }; // struct PlaybackResources


        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string details_popup_id = "AudioDetailsPopup";


        /*-----------*/
        /* Variables */
        /*-----------*/

        State state;
        ConstStationPtr station;
        std::optional<PlaybackResources> play_res;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        std::string
        format_to_string(sdl::audio::format fmt);

        void
        history_add(const std::string& title);

        void
        history_trim();

        void
        load();

        void
        process_logic();

        void
        save();

        void
        show_details_popup();

        void
        show_history();

        void
        show_station();

        void
        show_stream();

        void
        show_toolbar();


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        std::string
        format_to_string(sdl::audio::format fmt)
        {
            auto sample_size   = SDL_AUDIO_BITSIZE(fmt);
            bool is_float      = SDL_AUDIO_ISFLOAT(fmt);
            bool is_big_endian = SDL_AUDIO_ISBIGENDIAN(fmt);
            bool is_signed     = SDL_AUDIO_ISSIGNED(fmt);
            return std::format("{}{}{}{}",
                               is_signed ? "" : "u",
                               is_float ? "float" : "int",
                               sample_size,
                               is_big_endian ? "be" : "le");
        }


        void
        history_add(const std::string& title)
        {
            if (!state.history.empty() && state.history.front().title == title)
                return;

            state.history.emplace_front(system_clock::now(), title);
            history_trim();
        }


        void
        history_trim()
        {
            if (state.history.size() > cfg.player_history_limit) {
                std::size_t excess = state.history.size() - cfg.player_history_limit;
                state.history.erase(state.history.end() - excess,
                                    state.history.end());
            }
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
        process_logic()
        {
            if (play_res)
                play_res->process();
        }


        PlaybackResources::PlaybackResources(const std::string& url,
                  const std::string& url_resolved) :
            samples_buffer(65536),
            radio{url, url_resolved, App::get_user_agent()}
        {
            queued_history.fill(0);

            if (cfg.disable_apd) {
#ifdef __WUT__
                IMDisableAPD();
#else
                // TODO: write similar code for desktop, to prevent computer from
                // suspending.
#endif
            }
        }


        PlaybackResources::~PlaybackResources()
        {
#ifdef __WUT__
            IMEnableAPD();
#endif
        }

        bool
        PlaybackResources::is_buffer_too_empty()
        {
            // if (cfg.player_buffer_size == 0)
            //     return false;

            // return total_bytes_fed < cfg.player_buffer_size * 1024u;
            return false;
        }


        void
        PlaybackResources::process()
        {
            try {
                radio.process();

                update_queued_history();

                radio.with_metadata(
                    [this](const radio_client::opt_stream_metadata& meta)
                    {
                        if (!meta)
                            return;

                        if (meta->title) {
                            if (meta->artist)
                                history_add(*meta->artist + " - " + *meta->title);
                            else
                                history_add(*meta->title);
                        } else
                            history_add({});
                    }
                );

                if (is_buffer_too_empty()) {
                    // LOG_DEBUG("buffer too empty");
                    return;
                }

                if (!audio_dev) {
                    // see if we have enough bytes to initialize audio_dev properly.
                    radio.with_decoder_spec(
                        [this](const radio_client::opt_decoder_spec& radio_spec)
                        {
                            if (!radio_spec)
                                return;
                            sdl::audio::spec desired_spec;
                            desired_spec.freq     = radio_spec->rate;
                            desired_spec.channels = radio_spec->channels;
                            desired_spec.format   = radio_spec->format;
                            desired_spec.samples  = 8192;
                            audio_dev.create(nullptr, false, desired_spec, audio_spec);
                            audio_dev.unpause();
                        }
                    );
                }

                if (!audio_dev)
                    return;

                radio.consume_samples(
                    [this](byte_stream& stream)
                    {
                        while (!stream.empty()) {
                            auto samples = stream.read(std::span(samples_buffer));
                            audio_dev.play(samples);
                        }
                    }
                );

            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
            }
        }


        void
        PlaybackResources::update_queued_history()
        {
            std::size_t queued_size = audio_dev ? audio_dev.get_queued_size() : 0z;
            auto sample_size = SDL_AUDIO_BITSIZE(audio_spec.format);
            float current =
                queued_size
                / float(sample_size * audio_spec.freq);

            queued_history[last_queued_history++] = current;
            if (last_queued_history >= queued_history.size())
                last_queued_history = 0;
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
        show_details_popup()
        {
            using namespace ImGui::RAII;

            const auto& style = ImGui::GetStyle();
            float width = queued_history_size * 3
                + 2 * style.FramePadding.x
                + 2 * style.WindowPadding.x;
            ImGui::SetNextWindowSize({width, 500}, ImGuiCond_Always);
            auto viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(),
                                    ImGuiCond_Always,
                                    {0.5f, 0.5f});
            Popup popup{details_popup_id};
            if (!popup)
                return;

            if (!play_res) {
                ImGui::CloseCurrentPopup();
                return;
            }

            if (Child content{"content",
                              {0, 0},
                              ImGuiChildFlags_NavFlattened}) {

                UI::Title("Playback stats");

                Font smaller{nullptr, 0, 0.8f};

                const auto& dev = play_res->audio_dev;
                if (!dev) {
                    ImGui::Text("Output audio device not initialized.");
                } else {
                    const auto& spec = play_res->audio_spec;
                    if (Table table{"table", 2}) {
                        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                        try {
                            auto [default_name, default_spec] = sdl::audio::get_default_info(false);
                            UI::InfoRow("Device", default_name);
                        }
                        catch (...) {}
                        UI::InfoRow("Status", to_string(dev.get_status()));
                        UI::FormatInfoRow("Frequency", "{}Hz", humanize::value(spec.freq));
                        UI::InfoRow("Sample format", format_to_string(spec.format));
                    }

                    auto available = ImGui::GetContentRegionAvail();
                    ImGui::PlotHistogram("##queued_audio",
                                         std::span{play_res->queued_history},
                                         play_res->last_queued_history,
                                         {},
                                         0,
                                         FLT_MAX,
                                         available);
                }
            }
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
                    ImGuiChildFlags_NavFlattened
                }) {

                ImGui::SetNextItemOpen(state.history_expanded);
                if ((state.history_expanded = ImGui::CollapsingHeader("Track history"))) {

                    Font smaller{nullptr, 0, 0.8f};
                    Indent indenter;

                    if (Table table{"table",
                                    2,
                                    ImGuiTableFlags_BordersInnerH}) {

                        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                        for (const auto& [when, title] : state.history) {

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
                    ImGuiChildFlags_NavFlattened
                }) {

                ImGui::SetNextItemOpen(state.details_expanded);
                if ((state.details_expanded = ImGui::CollapsingHeader("Stream details"))) {

                    if (!play_res)
                        return;

                    Font smaller{nullptr, 0, 0.8f};

                    Indent indenter;
                    if (Table metadata_table{"metadata", 2}) {

                        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                        play_res->radio.with_metadata(
                            [](const radio_client::opt_stream_metadata& meta)
                            {
                                if (!meta)
                                    return;
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
                        );

                        play_res->radio.with_decoder_info(
                            [](const radio_client::opt_decoder_info& info)
                            {
                                if (!info)
                                    return;
                                if (!info->codec.empty())
                                    UI::InfoRow("Codec", info->codec);
                                if (!info->bitrate.empty())
                                    UI::InfoRow("Bitrate", info->bitrate);
                            }
                        );
                    }

                }

            } // stream_child
        }


        void
        show_toolbar()
        {
            using namespace ImGui::RAII;

            ImGui::AlignTextToFramePadding();

            if (play_res) {
                if (ImGui::Button(ICON_FA_INFO_CIRCLE))
                    ImGui::OpenPopup(details_popup_id);
                ImGui::SetItemTooltip("Show output audio device properties.");
                show_details_popup();

                ImGui::SameLine();

                if (const auto& dev = play_res->audio_dev) {
                    ImGui::FormatText("Output audio device: {}",
                                      to_string(dev.get_status()));
                } else {
                    ImGui::Text("Waiting for audio.");
                }
            } else {
                ImGui::Text("Not playing.");
            }
        }

    } // namespace


    void
    initialize()
    {
        load();

        App::add_callback(process_logic);
    }


    void
    finalize()
    {
        save();
        play_res.reset();
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

            show_toolbar();
            show_station();
            show_stream();
            show_history();

        } // player_child

        StationDetailsPopup::process_ui();
    }


    void
    play()
    {
        if (!station)
            return;

        if (play_res && play_res->radio.current_state != radio_client::state::stopped)
            stop();

        LOG_INFO("Starting playback of station {:?}", station->name);

        RecentTab::add(station);

        LOG_INFO("Playing url={:?}, url_resolved={:?}",
                 station->url,
                 station->url_resolved);

        play_res.emplace(station->url, station->url_resolved);

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
        play_res.reset();
    }


    bool
    is_playing(const Station& st)
    {
        if (!play_res)
            return false;
        if (!station)
            return false;
        if (play_res->radio.current_state == radio_client::state::stopped)
            return false;
        if (&st == station.get())
            return true;
        return st == *station;
    }

} // namespace PlayerTab
