/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RADIO_CLIENT_HPP
#define RADIO_CLIENT_HPP

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
#include <thread>

#include <sdl2xx/audio.hpp>

#include "byte_stream.hpp"
#include "decoder.hpp"
#include "http_client.hpp"
#include "icy_stream.hpp"
#include "thread_safe.hpp"


// This class is the high-level handler for internet radio streams.

struct RadioClient {

    enum class state {
        stopped,
        started,
        receiving_playlist,
        streaming_audio,
    };


    enum class playlist_type {
        none,
        asx,                    // TODO
        cue,                    // TODO
        jspf,                   // TODO text/json
        m3u,
        pls,
        wpl,                    // TODO
        xspf,                   // TODO application/xspf+xml
    };

    using opt_stream_metadata = std::optional<stream_metadata>;
    using opt_decoder_info = std::optional<decoder::info>;
    using opt_decoder_spec = std::optional<decoder::spec>;

    using ConsumeSamplesFunction = std::function<void(byte_stream& stream)>;
    using MetadataFunction = std::function<void(const opt_stream_metadata&)>;
    using DecoderInfoFunction = std::function<void(const opt_decoder_info&)>;
    using DecoderSpecFunction = std::function<void(const opt_decoder_spec&)>;

    state current_state = state::stopped;

    playlist_type current_playlist = playlist_type::none;

    std::string url;
    std::string url_resolved;
    std::string user_agent;

    http_client http;
    std::unique_ptr<icy::stream> icy_stream;

    byte_stream* data_stream = nullptr;


    RadioClient(const std::string& url,
                const std::string& url_resolved,
                const std::string& user_agent);

    // disallow moving
    RadioClient(RadioClient&&) = delete;


    void
    process();


    void
    consume_samples(const ConsumeSamplesFunction& func);


    void
    with_metadata(const MetadataFunction& func)
        const noexcept;


    void
    with_decoder_info(const DecoderInfoFunction& func)
        const noexcept;


    void
    with_decoder_spec(const DecoderSpecFunction& func);

private:

    std::unique_ptr<decoder::base> dec;
    std::vector<char> network_to_decoder_input_buffer;
    std::vector<char> decoder_input_to_decoder_buffer;

    thread_safe<opt_stream_metadata> safe_metadata;
    thread_safe<opt_decoder_spec> safe_decoder_spec;
    thread_safe<opt_decoder_info> safe_decoder_info;

    std::mutex decoder_input_mutex;
    std::condition_variable_any empty_decoder_input;
    byte_stream decoder_input;
    thread_safe<byte_stream> decoder_output;
    std::jthread decoder_thread;


    void
    set_next_url(const std::string& next_url);

    void
    process_http_response_started();

    void
    process_http_response_finished();

    void
    process_http_recv();

    void
    process_playlist();

    void
    process_audio();

    void
    decoder_thread_function(std::stop_token stopper);

}; // struct RadioClient

#endif
