/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <optional>
#include <ranges>
#include <vector>

#ifdef __WUT__
#include <coreinit/energysaver.h>
#endif

#include <curlxx/curl.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <mpg123xx/mpg123.hpp>

#include <sdl2xx/audio.hpp>

#include "Player.hpp"

#include "Browser.hpp"
#include "byte_stream.hpp"
#include "cfg.hpp"
#include "Favorites.hpp"
#include "humanize.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
#include "icy.hpp"
#include "imgui_extras.hpp"
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

    std::optional<Station> station;


    SDL_AudioFormat
    mpg123_encoding_to_sdl_format(unsigned encoding)
    {
        switch (encoding) {

            case MPG123_ENC_SIGNED_16:
                return AUDIO_S16SYS;

            case MPG123_ENC_UNSIGNED_16:
                return AUDIO_U16SYS;

            case MPG123_ENC_SIGNED_32:
                return AUDIO_S32SYS;

            case MPG123_ENC_FLOAT_32:
                return AUDIO_F32SYS;

            default:
                return 0;

        }
    }


    std::string
    mpg123_encoding_to_string(unsigned encoding)
    {
        switch (encoding) {

            case MPG123_ENC_SIGNED_16:
                return "signed-16-bit";

            case MPG123_ENC_UNSIGNED_16:
                return "unsigned-16-bit";

            case MPG123_ENC_SIGNED_32:
                return "signed-32-bit";

            case MPG123_ENC_FLOAT_32:
                return "float-32-bit";

            default:
                return "forced-signed-16-bit";
        }
    }


    std::string
    get_codec_desc(const mpg123::frame_info& info)
    {
        std::string result;

        // MPEG version
        switch (info.version) {
            case MPG123_1_0:
                result += "MPEG 1.0";
                break;
            case MPG123_2_0:
                result += "MPEG 2.0";
                break;
            case MPG123_2_5:
                result += "MPEG 2.5";
                break;
            default:
                result += "MPEG ?";
        }
        result += " layer ";
        result += std::to_string(info.layer);

        result += "; ";

        // Sampling rate
        switch (info.rate) {
            case 48000:
                result += "48 k";
                break;
            case 44100:
                result += "44.1 k";
                break;
            case 22050:
                result += "22.05 k";
                break;
            default:
                if ((info.rate % 1000) == 0)
                    result += utils::cpp_sprintf("%u k", info.rate / 1000u);
                else // weird rate, just print out without the k
                    result += utils::cpp_sprintf("%u ", info.rate);
        }
        result += "㎐";

        result += "; ";

        // Channel mode
        switch (info.mode) {
            case MPG123_M_STEREO:
                result += "stereo";
                break;
            case MPG123_M_JOINT:
                result += "joint stereo";
                break;
            case MPG123_M_DUAL:
                result += "dual channel";
                break;
            case MPG123_M_MONO:
                result += "mono";
                break;
            default:
                result += "unknown";
        }

        return result;
    }


    std::string
    get_bitrate_desc(const mpg123::frame_info& info)
    {
        std::string result;
        switch (info.vbr_mode) {
            case MPG123_CBR:
                result = "constant";
                break;
            case MPG123_VBR:
                result = "variable";
                break;
            case MPG123_ABR:
                result = "average";
                break;
            default:
                result = "unknown";
        }
        result += " " + std::to_string(info.bitrate) + " kbps";
        return result;
    }


    void
    history_add(const std::string& title);


    /*
     * RAII-managed resources are stored here.
     * Note that they're only allocated while playback is active.
     */
    struct Resources {

        curl::multi multi;
        curl::easy easy;
        mpg123::handle mpg;
        sdl::audio::device audio_dev;

        byte_stream raw_stream;
        byte_stream data_stream;
        byte_stream meta_stream;

        // State variables to help separating data from metadata.
        std::size_t data_left = 0;
        std::size_t meta_left = 0;
        bool parsed_header = false;

        // Parsed iecast headers
        std::size_t icy_metaint = 0;
        std::string icy_name;
        std::string icy_url;
        std::string icy_genre;
        std::string icy_description;

        icy::dict_t meta;

        std::optional<mpg123::format> stream_format;


        Resources(const std::string& url)
        {
            mpg.set_verbose(true);
            mpg.open_feed();

            easy.set_verbose(false);
            easy.set_user_agent(utils::get_user_agent());
            easy.set_url(url);
            easy.set_forbid_reuse(true);
            easy.set_follow(true);
            easy.set_ssl_verify_peer(false);
            easy.set_http_headers({
                    "Icy-MetaData: 1",
                    "Accept: audio/mpeg"
                });
            easy.set_write_function(std::bind(&Resources::easy_write_callback, this, _1));

            multi.set_max_total_connections(2);
            multi.add(easy);

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

            multi.remove(easy);
#ifdef __WUT__
            IMEnableAPD();
#endif
        }


        // Disallow moving.
        Resources(Resources&&) = delete;


        void
        parse_stream()
        {
            if (raw_stream.empty())
                return;

            if (icy_metaint) {
                // we alternate between reading data and metadata
                if (data_left) {
                    data_left -= data_stream.consume(raw_stream, data_left);
                } else {
                    // no more data to read, we start reading metadata
                    if (meta_left == 0) {
                        // if both data_left and meta_left are zero, we're waiting for the meta
                        // size prefix
                        auto c = raw_stream.try_load_u8();
                        if (!c)
                            return;
                        meta_left = *c * 16u;
                        if (meta_left == 0) {
                            // empty metadata, just go back to reading data
                            data_left = icy_metaint;
                            return;
                        }
                    }
                    meta_left -= meta_stream.consume(raw_stream, meta_left);
                    if (meta_left == 0)
                        // finished reading metadata, go back to reading data
                        data_left = icy_metaint;
                }
            } else {
                // not metadata, so everything is data
                data_stream.consume(raw_stream);
            }
        }


        std::size_t
        easy_write_callback(std::span<const char> buf)
        {
            if (buf.empty()) {
                cout << "End of connection detected." << endl;
                state = State::stopping;
                return 0;
            }

            if (is_buffer_too_full())
                return CURL_WRITEFUNC_PAUSE;

            if (!parsed_header) {
                if (auto hdr = easy.try_get_header("icy-metaint")) {
                    parsed_header = true;
                    cout << "Got icy-metaint: " << hdr->value << endl;
                    icy_metaint = std::stoull(hdr->value);
                    if (icy_metaint)
                        data_left = icy_metaint;
                }

                if (auto hdr = easy.try_get_header("icy-name"))
                    icy_name = utils::trimmed(hdr->value);
                else
                    icy_name = "";

                if (auto hdr = easy.try_get_header("icy-url"))
                    icy_url = utils::trimmed(hdr->value);
                else
                    icy_url = "";

                if (auto hdr = easy.try_get_header("icy-genre"))
                    icy_genre = utils::trimmed(hdr->value);
                else
                    icy_genre = "";

                if (auto hdr = easy.try_get_header("icy-description"))
                    icy_description = utils::trimmed(hdr->value);
                else
                    icy_description = "";
            }

            // since CURL requires us to handle all bytes, we stash them in raw_stream
            return raw_stream.write(buf);
        }


        // FIXME: we don't take into account how much data is queued up in mpg123 and SDL.
        std::size_t
        calc_buffer_size()
        {
            return raw_stream.size()
                 + data_stream.size()
                 + meta_stream.size();
        }


        bool
        is_buffer_too_empty()
        {
            if (cfg::player_buffer_size == 0)
                return false;
            return calc_buffer_size() < cfg::player_buffer_size * 1024;
        }


        bool
        is_buffer_too_full()
        {
            return calc_buffer_size() > 2 * cfg::player_buffer_size * 1024;
        }


        void
        process_meta()
        {
            // Note: icy metadata is often padded with null bytes.
            std::string meta_str = utils::trimmed(meta_stream.read_str(), '\0');

            meta = icy::parse(meta_str);

            cout << "Icy Metadata:\n";
            for (auto [key, val] : meta) {
                cout << "    " << key << "=\"" << val << "\"\n";
            }
            cout << endl;

            if (meta.contains("StreamTitle"))
                history_add(meta.at("StreamTitle"));
        }


        void
        process()
        {
            try {
                multi.perform();
                auto done = multi.get_done();
                for (const auto& msg : done) {
                    if (msg.handle == &easy) {
                        // it's our handle
                        state = State::stopping;
                        if (msg.result != CURLE_OK) {
                            cout << "Streaming session ended in error: " << msg.result << endl;
                        }
                    }
                }

                parse_stream();

                if (meta_left == 0 && !meta_stream.empty())
                    process_meta();

                if (is_buffer_too_empty())
                    return;

                auto chunk = data_stream.read_as<char>();
                mpg.feed(std::span<const char>(chunk));

                if (!audio_dev) {
                    // see if we have enough bytes to initialize audio_dev properly.
                    if (auto fmt = mpg.try_get_format()) {
                        cout << "got mpg123 format: " << *fmt << endl;
                        sdl::audio::spec spec;
                        spec.freq = fmt->rate;
                        spec.channels = fmt->channels & MPG123_STEREO ? 2 : 1;
                        spec.format = mpg123_encoding_to_sdl_format(fmt->encoding);
                        spec.samples = 4096;
                        if (!spec.format) {
                            // no exact match, ask mpg123 to convert it to S16
                            spec.format = AUDIO_S16SYS;
                            mpg.set_format(fmt->rate, fmt->channels, MPG123_ENC_SIGNED_16);
                        }
                        stream_format = std::move(*fmt);

                        audio_dev.create(nullptr, false, spec);
                        audio_dev.unpause();
                    }
                }

                if (!audio_dev)
                    return;

                while (auto res = mpg.try_decode_frame()) {
                    audio_dev.play(res->raw_data);

                    // TODO: these don't show up on streams, but do if playing a regular
                    // mp3 file.
                    auto meta_flags = mpg.meta_check();
                    if (meta_flags & MPG123_NEW_ID3) {
                        cout << "Got ID3 metadata." << endl;
                        auto id3 = mpg.get_id3();
                        if (id3.v2) {
                            auto& tag = *id3.v2;
                            if (!tag.title.empty())
                                cout << "Title: " << tag.title << endl;
                            if (!tag.artist.empty())
                                cout << "Artist: " << tag.artist << endl;
                            if (!tag.album.empty())
                                cout << "Album: " << tag.album << endl;
                        } else if (id3.v1) {
                            auto& tag = *id3.v1;
                            if (!tag.title.empty())
                                cout << "Title: " << tag.title << endl;
                            if (!tag.artist.empty())
                                cout << "Artist: " << tag.artist << endl;
                            if (!tag.album.empty())
                                cout << "Album: " << tag.album << endl;
                        }
                    }
                }

                if (!is_buffer_too_full())
                    easy.unpause();

            }
            catch (std::exception& e) {
                cout << "ERROR: " << e.what() << endl;
            }
        }

    }; // struct Resources

    std::optional<Resources> res;


    struct TrackInfo {
        system_clock::time_point when;
        std::string title;
    };

    std::deque<TrackInfo> history;


    void
    initialize()
    {}


    void
    finalize()
    {
        res.reset();
    }


    void
    play()
    {
        if (station)
            play(*station);
    }


    void
    play(const Station& st)
    {
        if (state == State::playing)
            stop();

        cout << "Starting playback of station \"" << st.name << "\"" << endl;

        station = st;
        Recent::add(st);

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

        Browser::send_click(station->uuid);

        state = State::playing;

        res.emplace(url); // allocate and initialize resources here
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

                ui::show_play_button(*station);

                if (Favorites::contains(*station)) {
                    if (ImGui::Button(ICON_FA_HEART /*♥*/))
                        Favorites::remove(*station);
                } else {
                    if (ImGui::Button(ICON_FA_HEART_O /*♡*/))
                        Favorites::add(*station);
                }

                ImGui::SameLine();

                ImGui::BeginDisabled(station->uuid.empty());
                if (ImGui::Button(ICON_FA_INFO_CIRCLE /*🛈*/))
                    ui::open_station_info_popup(station->uuid);
                ImGui::EndDisabled();
                ui::process_station_info_popup();

            } // actions
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("details",
                                  {0, 0},
                                  ImGuiChildFlags_AutoResizeY |
                                  ImGuiChildFlags_NavFlattened)) {

                ui::show_favicon(station->favicon);

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

            if (ImGui::CollapsingHeader("Stream details")) {

                ImGui::Indent();

                if (res && ImGui::BeginTable("icy_meta", 2)) {

                    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    auto it = res->meta.find("StreamTitle");
                    if (it != res->meta.end())
                        ui::show_info_row("Title", it->second);


                    // Show all icy metadata too, even if we don't know what they are
                    for (auto& [key, val] : res->meta) {
                        if (key == "StreamTitle")
                            continue;
                        auto tval = utils::trimmed(val);
                        if (tval.empty())
                            continue;
                        ui::show_info_row(key, tval);
                    }

                    if (!utils::trimmed(res->icy_name).empty())
                        ui::show_info_row("Name", res->icy_name);

                    if (!res->icy_url.empty())
                        ui::show_link_row("URL", res->icy_url);

                    if (!res->icy_genre.empty())
                        ui::show_info_row("Genre", res->icy_genre);

                    if (!res->icy_description.empty())
                        ui::show_info_row("Description", res->icy_description);

                    if (auto info = res->mpg.try_get_frame_info()) {
                        ui::show_info_row("Codec", get_codec_desc(*info));
                        ui::show_info_row("Bitrate", get_bitrate_desc(*info));
                    }

                    ImGui::EndTable();

                    ImGui::Unindent();

                }
            } // endif (ImGui::CollapsingHeader("Stream details"))
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

            if (ImGui::CollapsingHeader("Track history")) {

                ImGui::Indent();

                if (ImGui::BeginTable("table",
                                      2,
                                      ImGuiTableFlags_BordersInnerH)) {

                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    for (const auto& [when, title] : history | std::views::reverse) {

                        auto t = duration_cast<std::chrono::seconds>(now - when);
                        std::string label = humanize::duration(t) + " ago";

                        ui::show_info_row(label, title);

                    }

                    ImGui::EndTable();
                }

                ImGui::Unindent();
            }

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
