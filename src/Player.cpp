/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <map>
#include <optional>
#include <vector>

#ifdef __WUT__
#include <coreinit/energysaver.h>
#endif

#include <curlxx/curl.hpp>
#include <imgui.h>
#include <mpg123xx/mpg123.hpp>
#include <sdl2xx/audio.hpp>

#include "Player.hpp"

#include "Browser.hpp"
#include "byte_stream.hpp"
#include "cfg.hpp"
#include "IconManager.hpp"
#include "icy_meta.hpp"
#include "imgui_extras.hpp"
#include "Recent.hpp"
#include "Station.hpp"
#include "utils.hpp"


using std::cout;
using std::endl;
using namespace std::literals;
using sdl::vec2;


namespace Player {

    enum class State {
        stopped,
        playing,
        stopping,
    };

    State state = State::stopped;

    std::optional<Station> station;
    curl::multi multi;
    curl::easy easy;
    mpg123::handle mpg;
    sdl::audio::device audio_dev;

    // iecast headers
    std::size_t icy_metaint = 0;
    std::string icy_name;
    std::string icy_url;
    std::string icy_genre;
    std::string icy_description;
    bool parsed_header = false;

    byte_stream raw_stream;
    byte_stream data_stream;
    byte_stream meta_stream;
    std::size_t data_left = 0;
    std::size_t meta_left = 0;

    std::map<std::string, std::string> meta;


    namespace {

        SDL_AudioFormat
        mpg_to_sdl_format(unsigned fmt)
        {
            switch (fmt) {

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


        void
        trim(std::string& s)
        {
            auto i = s.find_last_not_of('\0');
            if (i != std::string::npos)
                s.erase(i);
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
            return calc_buffer_size() < cfg::player_buffer_size;
        }


        bool
        is_buffer_too_full()
        {
            return calc_buffer_size() > 2 * cfg::player_buffer_size;
        }


        std::size_t
        easy_write_callback(std::span<const char> buf)
        {
            if (buf.empty()) {
                state = State::stopping;
                return 0;
            }

            if (is_buffer_too_full())
                return CURL_WRITEFUNC_PAUSE;

            if (!parsed_header) {
                if (auto hdr = easy.try_get_header("icy-metaint", 0, CURLH_HEADER, -1)) {
                    parsed_header = true;
                    cout << "Got icy-metaint: " << hdr->value << endl;
                    icy_metaint = std::stoull(hdr->value);
                    if (icy_metaint)
                        data_left = icy_metaint;
                }

                if (auto hdr = easy.try_get_header("icy-name", 0, CURLH_HEADER, -1))
                    icy_name = hdr->value;
                else
                    icy_name = "";

                if (auto hdr = easy.try_get_header("icy-url", 0, CURLH_HEADER, -1))
                    icy_url = hdr->value;
                else
                    icy_url = "";

                if (auto hdr = easy.try_get_header("icy-genre", 0, CURLH_HEADER, -1))
                    icy_genre = hdr->value;
                else
                    icy_genre = "";

                if (auto hdr = easy.try_get_header("icy-description", 0, CURLH_HEADER, -1))
                    icy_description = hdr->value;
                else
                    icy_description = "";
            }

            // since CURL requires us to handle all bytes, we stash them in raw_stream
            return raw_stream.write(buf);
        }

    } // namespace


    void
    initialize()
    {}


    void
    finalize()
    {}


    void
    play()
    {
        if (state == State::playing)
            stop();

        if (!station)
            return;

        std::string url;
        if (!station->url_resolved.empty())
            url = station->url_resolved;

        if (url.empty())
            if (!station->url.empty())
                url = station->url; // TODO: might need resolving

        if (url.empty()) {
            cout << "No usable URL found" << endl;
            return;
        }

        if (!station->uuid.empty())
            Browser::send_click(station->uuid);

        cout << "starting to play " << url << endl;

        state = State::playing;

        parsed_header = false;

        icy_metaint = 0;

        mpg = mpg123::handle{};
        mpg.set_verbose();
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
        easy.set_write_function(easy_write_callback);

        multi.set_max_total_connections(2);
        multi.add(easy);

        // TODO: write similar code for desktop
        if (cfg::disable_auto_power_down) {
#ifdef __WUT__
            IMDisableAPD();
#endif
        }

    }


    void
    play(const Station& st)
    {
        cout << "Starting playback of station \"" << st.name << "\"" << endl;
        station = st;
        Recent::add(st);
        play();
    }



    void
    stop()
    {
        if (state == State::stopped)
            return;
        state = State::stopped;
        multi.remove(easy);
        easy.reset();
        audio_dev.destroy();
        mpg.destroy();
        raw_stream.clear();
        data_stream.clear();
        meta_stream.clear();

        icy_metaint = 0;
        icy_name = "";
        icy_url = "";
        icy_genre = "";
        icy_description = "";
        meta.clear();

#ifdef __WUT__
        IMEnableAPD();
#endif
    }


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


    void
    process_meta()
    {
        std::string meta_str = meta_stream.read_str();
        trim(meta_str);

        meta = icy_meta::parse(std::move(meta_str));
        cout << "Metadata:\n";
        for (auto [key, val] : meta) {
            cout << "    " << key << "=\"" << val << "\"\n";
        }
        cout << endl;

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

        try {
            multi.perform();

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
                    spec.format = mpg_to_sdl_format(fmt->encoding);
                    spec.samples = 4096;
                    if (!spec.format) {
                        // no exact match, ask mpg123 to convert it to S16
                        spec.format = AUDIO_S16SYS;
                        mpg.set_format(fmt->rate, fmt->channels, MPG123_ENC_SIGNED_16);
                    }

                    cout << "Creating SDL audio device..." << endl;
                    audio_dev.create(nullptr, false, spec);
                    audio_dev.unpause();
                }
            }

            if (!audio_dev)
                return;

            while (auto res = mpg.try_decode_frame()) {
                audio_dev.play(res->samples);

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


    void
    process_ui()
    {
        if (ImGui::BeginChild("player",
                              {0, 0},
                              ImGuiChildFlags_NavFlattened)) {

            ImGui::BeginDisabled(!station);

            const vec2 button_size = {64, 64};
            if (state == State::playing) {
                if (ImGui::ImageButton("stop",
                                       *IconManager::get("ui/stop-button.png"),
                                       button_size))
                    stop();
            } else {
                if (ImGui::ImageButton("play",
                                       *IconManager::get("ui/play-button.png"),
                                       button_size))
                    play();
            }

            ImGui::ValueWrapped("Name", icy_name);
            if (!icy_url.empty())
                ImGui::TextLink(icy_url.data());
            if (!icy_genre.empty())
                ImGui::ValueWrapped("Genre", icy_genre);
            if (!icy_description.empty())
                ImGui::ValueWrapped("Description", icy_description);


            std::string status_str;
            if (state == State::playing) {
                status_str = "Playing ";
                auto it = meta.find("StreamTitle");
                if (it != meta.end())
                    status_str += it->second;
                else
                    status_str += "?";
            } else {
                status_str = "Stopped";
            }
            ImGui::ValueWrapped("Status", status_str);

            ImGui::EndDisabled();

        }

        ImGui::HandleDragScroll();

        ImGui::EndChild();

    }


} // namespace Player
