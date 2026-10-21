/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <chrono>
#include <deque>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "PlayerTab.hpp"

#include "App.hpp"
#include "humanize.hpp"
#include "humanize.hpp"
#include "IconsFontAwesome4.h"
#include "ImageLoader.hpp"
#include "LogManager.hpp"
#include "RadioClient.hpp"
#include "RecentTab.hpp"
#include "Serializer.hpp"
#include "Settings.hpp"
#include "StationClicking.hpp"
#include "StationDetailsPopup.hpp"
#include "StationVoting.hpp"
#include "TraceDuration.hpp"
#include "TraceFunction.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using std::chrono::system_clock;

using namespace std::literals;

using Settings::cfg;


namespace PlayerTab {

    namespace {

        constexpr std::size_t audio_queue_history_size = 5 * 60;


        /*-------*/
        /* Types */
        /*-------*/

        struct HistoryEntry {
            system_clock::time_point when{};
            std::string title{};
        };


        struct State {
            bool details_expanded{};
            bool track_history_expanded{};
            std::deque<HistoryEntry> history{};
        };


        struct PlaybackResources {

            RadioClient radio;
            std::array<float, audio_queue_history_size> audio_queue_history;
            std::size_t audio_queue_index = 0;


            PlaybackResources(const std::string& url,
                              const std::string& url_resolved);

            ~PlaybackResources();

            // Disallow moving.
            PlaybackResources(PlaybackResources&&) = delete;


            // bool
            // is_buffer_too_empty();


            void
            process();


            void
            update_audio_queue_history();

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
        bool changed;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        std::string
        format_to_string(sdl::audio::format fmt);

        void
        load();

        void
        process_logic();

        void
        save();

        // void
        // show_details_popup();

        void
        show_history();

        void
        show_station();

        void
        show_stream();

        void
        show_toolbar();

        void
        track_history_add(const stream_metadata& meta);

        void
        track_history_trim();


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
        load()
        try {
            TRACE_FUNC;

            auto filename = App::get_config_dir() / "player.json";
            Serializer::load(state, filename);
            changed = false;
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
            radio{App::get_user_agent(), url, {url_resolved}}
        {
            audio_queue_history.fill(0);
        }


        PlaybackResources::~PlaybackResources()
        {
        }


#if 0
        bool
        PlaybackResources::is_buffer_too_empty()
        {
            // if (cfg.player_buffer_size == 0)
            //     return false;

            // return total_bytes_fed < cfg.player_buffer_size * 1024u;
            return false;
        }
#endif


        void
        PlaybackResources::process()
        {
            TraceFunction tf{"PlayerTab"sv};

            try {
                radio.process();

                update_audio_queue_history();

                radio.with_metadata(track_history_add);
            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
            }
        }


        void
        PlaybackResources::update_audio_queue_history()
        {
            audio_queue_history[audio_queue_index++] =
                App::get_queued_audio_duration().count() / 1000.0;

            if (audio_queue_index >= audio_queue_history.size())
                audio_queue_index = 0;
        }


        void
        save()
        try {
            TRACE_FUNC;

            auto filename = App::get_config_dir() / "player.json";
            Serializer::save(state, filename);
            changed = false;
        }
        catch (std::exception& e) {
            LOG_ERROR("{}", e.what());
        }


        void
        show_details_popup()
        {
            using namespace ImGui::RAII;

            const auto& style = ImGui::GetStyle();
            float width = audio_queue_history_size * 3
                + 2 * style.FramePadding.x
                + 2 * style.WindowPadding.x;
            ImGui::SetNextWindowSize({width, 600}, ImGuiCond_Always);
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

                auto spec = App::get_audio_spec();
                if (Table table{"table", 2}) {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    try {
                        auto [default_name, default_spec] =
                            sdl::audio::get_default_info(false);
                        UI::InfoRow("Device", default_name);
                    }
                    catch (...) {}

                    UI::FormatInfoRow("Sound mode",
                                      "{}㎐, {}",
                                      humanize::value(spec.rate),
                                      format_to_string(spec.format));

                    auto transfer_state = play_res->radio.get_transfer_state();
                    UI::InfoRow("Transfer state", to_string(transfer_state));

                    auto net_state = play_res->radio.get_network_thread_state();
                    UI::InfoRow("Network thread", to_string(net_state));

                    auto audio_state = play_res->radio.get_audio_thread_state();
                    UI::InfoRow("Audio thread", to_string(audio_state));

                }

                ImGui::SeparatorText("Audio buffer (seconds)");

                auto available = ImGui::GetContentRegionAvail();
                ImGui::PlotHistogram("##audio_queue_history"s,
                                     play_res->audio_queue_history,
                                     play_res->audio_queue_index,
                                     {},
                                     0,
                                     FLT_MAX,
                                     available);
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

                ImGui::SetNextItemOpen(state.track_history_expanded);
                bool old_track_history_expanded = state.track_history_expanded;
                if ((state.track_history_expanded = ImGui::CollapsingHeader("Track history"))) {

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
                if (old_track_history_expanded != state.track_history_expanded)
                    changed = true;

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
                bool old_details_expanded = state.details_expanded;
                if ((state.details_expanded = ImGui::CollapsingHeader("Stream details"))) {

                    if (!play_res)
                        return;

                    Font smaller{nullptr, 0, 0.8f};

                    Indent indenter;
                    if (Table metadata_table{"metadata", 2}) {

                        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                        play_res->radio.with_metadata(
                            [](const stream_metadata& meta)
                            {
                                UI::InfoRowOpt("Title", meta.title);
                                UI::InfoRowOpt("Track", meta.track);
                                UI::InfoRowOpt("Artist", meta.artist);
                                UI::InfoRowOpt("Composer", meta.composer);
                                UI::ImageRowOpt("Cover Art", meta.album_art);
                                UI::InfoRowOpt("Next", meta.next);

                                UI::InfoRowOpt("Album", meta.album);
                                UI::ImageRowOpt("Album Art", meta.album_art);

                                UI::InfoRowOpt("Genre", meta.genre);
                                UI::InfoRowOpt("Date", meta.date);
                                UI::InfoRowOpt("Publisher", meta.publisher);
                                UI::InfoRowOpt("Label", meta.label);

                                // station metadata
                                UI::InfoRowOpt("Station Name", meta.station.name);
                                UI::InfoRowOpt("Station Genre", meta.station.genre);
                                UI::InfoRowOpt("Station Description", meta.station.description);
                                UI::InfoRowOpt("Station URL", meta.station.url);
                                UI::InfoRowOpt("Station Audio Info", meta.station.audio_info);

                                if (!meta.station.notice.empty()) {
                                    if (ImGui::CollapsingHeader("Station Notice")) {
                                        for (auto [index, line] :
                                                 meta.station.notice | std::views::enumerate) {
                                            UI::InfoRow(
                                                "Notice "s + std::to_string(index+1),
                                                line
                                            );
                                        }
                                    }
                                }

                                // extra metadata
                                for (const auto& [key, val] : meta.extra)
                                    UI::InfoRow(key, val);

                            }
                        );

                        play_res->radio.with_decoder_info(
                            [](const Decoder::Info& info)
                            {
                                if (!info.codec.empty())
                                    UI::InfoRow("Codec", info.codec);
                                if (!info.bitrate.empty())
                                    UI::InfoRow("Bitrate", info.bitrate);
                            }
                        );
                    } // Table

                }
                if (old_details_expanded != state.details_expanded)
                    changed = true;

            } // stream_child
        }


        void
        show_toolbar()
        {
            using namespace ImGui::RAII;

            if (Child toolbar{
                    "toolbar",
                    {0, 0},
                    ImGuiChildFlags_AutoResizeY |
                    ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_NoSavedSettings
                }) {

                Font smaller{nullptr, 0, 0.8f};

                ImGui::AlignTextToFramePadding();

                if (play_res) {

                    if (ImGui::Button(ICON_FA_INFO_CIRCLE))
                        ImGui::OpenPopup(details_popup_id);
                    ImGui::SetItemTooltip("Show output audio device properties.");
                    show_details_popup();

                    ImGui::SameLine();

                    // Show URL info vec
                    play_res->radio.with_url_info_vec(
                        [](const RadioClient::URLInfoVec& info_vec)
                        {
                            auto cur_idx = play_res->radio.get_current_url_info_index();
                            auto size = info_vec.size();
                            std::string combo_label = std::format(ICON_FA_SERVER " {}/{}",
                                                                  cur_idx + 1,
                                                                  size);
                            auto combo_label_size = ImGui::CalcTextSize(combo_label);
                            const auto& style = ImGui::GetStyle();
                            float combo_width =
                                2 * style.FramePadding.x // padding
                                + combo_label_size.x // text
                                + style.ItemInnerSpacing.x // separation between text and arrow
                                + ImGui::GetFrameHeight(); // arrow
                            ImGui::SetNextItemWidth(combo_width);
                            if (Combo url_vec_combo{"##url_vec_combo"s, combo_label}) {
                                for (auto [idx, entry] : info_vec | std::views::enumerate) {
                                    auto& [url, content_type, type] = entry;
                                    auto label = std::format(
                                        "{} {} ({})",
                                        to_label(type),
                                        url,
                                        content_type.empty() ? "?"s : content_type
                                    );
                                    if (ImGui::RadioButton(label, std::cmp_equal(idx, cur_idx)))
                                        play_res->radio.play(idx);
                                }
                            }
                            ImGui::SetItemTooltip("Show all URLs for this stream.");
                        }
                    );

                    ImGui::SameLine();

                    auto state = play_res->radio.get_state();
                    ImGui::FormatText("State: {}", to_string(state));

                    play_res->radio.with_error_message(
                        [](const std::string& message)
                        {
                            if (message.empty())
                                return;

                            ImGui::SameLine();

                            ImGui::FormatTextAligned(0, -1, "Error: {}", message);
                        }
                    );

                } else {
                    ImGui::Text("No station set.");
                }

            } // toolbar
        }


        void
        track_history_add(const stream_metadata& meta)
        {
            TraceFunction tf{"PlayerTab"sv};

            std::string title;
            if (meta.title) {
                title = *meta.title;
            } else {
                if (meta.artist && meta.track)
                    title = *meta.artist + " - "s + *meta.track;
                else if (meta.track)
                    title = *meta.track;
            }

            if (!state.history.empty() && state.history.front().title == title)
                return;

            state.history.emplace_front(system_clock::now(), std::move(title));
            track_history_trim();
            changed = true;
        }


        void
        track_history_trim()
        {
            if (state.history.size() > cfg.player_history_limit) {
                std::size_t excess = state.history.size() - cfg.player_history_limit;
                state.history.erase(state.history.end() - excess,
                                    state.history.end());
                changed  =true;
            }
        }

    } // namespace


    void
    initialize()
    {
        TRACE_FUNC;

        load();

        App::add_callback("PlayerTab::process_logic()"sv,
                          process_logic);
    }


    void
    finalize()
    {
        TRACE_FUNC;

        if (changed)
            save();
        play_res.reset();
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        show_toolbar();

        // NOTE: flat navigation doesn't work well on child windows that scroll.
        if (Child player_child{"player"}) {

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

        if (play_res && play_res->radio.is_playing())
            stop();

        LOG_INFO("Starting playback of station {:?}", station->name);

        RecentTab::add(station);

        LOG_INFO("Playing url={:?}, url_resolved={:?}",
                 station->url,
                 station->url_resolved);

        if (!play_res)
            play_res.emplace(station->url, station->url_resolved);

        play_res->radio.play();

        StationClicking::click(station);
    }


    void
    play(StationPtr& st)
    {
        if (st != station)
            play_res.reset();

        station = st;
        play();
    }


    void
    stop()
    {
        if (play_res)
            play_res->radio.stop();
    }


    bool
    is_playing(const Station& st)
    {
        if (!play_res)
            return false;
        if (!station)
            return false;
        if (!play_res->radio.is_playing())
            return false;
        if (&st == station.get())
            return true;
        return st == *station;
    }

} // namespace PlayerTab
