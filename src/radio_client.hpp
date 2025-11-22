/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RADIO_CLIENT_HPP
#define RADIO_CLIENT_HPP

#include <cstddef>
#include <span>
#include <string>

#include <sdl2xx/audio.hpp>

#include "decoder.hpp"
#include "http_client.hpp"
#include "icy_stream.hpp"


// This class is the high-level handler for internet radio streams.

struct radio_client {

    enum class state {
        stopped,
        waiting_response,
        handling_playlist,
        handling_audio,
    };

    state current_state = state::stopped;

    std::string base_url;
    std::string resolved_url;

    std::optional<stream_metadata> metadata;

    http_client http;
    std::unique_ptr<icy::stream> icy_stream;

    byte_stream* data_stream = nullptr;

    std::unique_ptr<decoder::base> dec;


    radio_client(const std::string& url);

    void
    process();


    std::optional<decoder::spec>
    get_spec();


    std::span<const char>
    get_samples();


    const std::optional<stream_metadata>&
    get_metadata()
        const noexcept;


    std::optional<decoder::info>
    get_decoder_info()
        const;


private:

    void
    process_http_response();

    void
    process_playlist();

    void
    process_audio();

}; // struct radio_client

#endif
