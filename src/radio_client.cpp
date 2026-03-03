/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>

#include "radio_client.hpp"

#include "cfg.hpp"
#include "m3u.hpp"
#include "mime_type.hpp"
#include "tracer.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


radio_client::radio_client(const std::string& url) :
    initial_url{url},
    http{initial_url},
    data_stream{&http.data_stream}
{
    TRACE_FUNC;

    http.add_header("Icy-MetaData: 1");

    current_state = state::started;

    // Note: these callbacks are invoked during http::process()
    http.on_response_started  = [this] { process_http_response_started();  };
    http.on_response_finished = [this] { process_http_response_finished(); };
    http.on_recv = [this] { process_http_recv(); };
}


void
radio_client::process()
{
    http.process();
}


std::optional<decoder::spec>
radio_client::get_spec()
{
    if (!dec)
        return {};
    return dec->get_spec();
}


std::span<const char>
radio_client::get_samples()
{
    if (!dec)
        return {};

    return dec->decode();
}


const std::optional<stream_metadata>&
radio_client::get_metadata()
    const noexcept
{
    return metadata;
}


std::optional<decoder::info>
radio_client::get_decoder_info()
    const
{
    if (!dec)
        return {};
    return dec->get_info();
}


void
radio_client::process_http_response_started()
{
    // TRACE_FUNC;

    auto content_type = http.get_header("content-type");
    if (!content_type) {
        cout << "ERROR: server provided no content-type" << endl;
        current_state = state::stopped;
        return;
    }

    // Mime types for m3u playlists
    const std::vector<std::string> m3u_types{
        "audio/mpegurl",
        "audio/x-mpegurl",
        "application/vnd.apple.mpegurl",
        "application/vnd.apple.mpegurl.audio",
        "application/mpegurl",
        "application/x-mpegurl",
    };

    // Mime types for audio streams
    const std::vector<std::string> audio_types{
        "audio/*",
        "application/ogg",
    };
    if (mime_type::match(*content_type, m3u_types)) {
        current_state = state::receiving_playlist;
    } else if (mime_type::match(*content_type, audio_types)) {
        current_state = state::streaming_audio;
        try {
            cout << "Trying to create ICY stream" << endl;
            icy_stream = std::make_unique<icy::stream>(http);
            data_stream = &icy_stream->data_stream;
            metadata = icy_stream->get_metadata();
        }
        catch (std::exception& e) {
            cout << "Could not create ICY stream: " << e.what() << endl;
        }
    }
}


void
radio_client::process_http_response_finished()
{
    // TRACE_FUNC;

    if (current_state == state::receiving_playlist)
        process_playlist();
}


void
radio_client::process_http_recv()
{
    if (icy_stream)
        icy_stream->process();

    if (current_state == state::streaming_audio)
        process_audio();
}


void
radio_client::process_playlist()
{
    // TRACE_FUNC;

    std::string data = data_stream->read_str();
    auto pl = m3u::parse(data);
    if (pl.empty()) {
        cout << "ERROR: playlis is empty!" << endl;
        cout << "<m3u>\n" << data << "</m3u>" << endl;
        current_state = state::stopped;
        return;
    }

    resolved_url = pl[0].url;
    cout << "radio_client::resolved_url = " << resolved_url << endl;

    http.set_url(resolved_url);
    current_state = state::started;
}


void
radio_client::process_audio()
{
    if (current_state != state::streaming_audio)
        cout << "WARNING: logic error! process_audio should only happen during streaming_audio state" << endl;

    if (icy_stream)
        metadata = icy_stream->get_metadata();

    if (!dec) {
        if (data_stream->size() < cfg::player_buffer_size * 1024)
            return; // don't bother creating a decoder when too little data
        try {
            // try to create a decoder
            auto hdr_content_type = http.get_header("content-type");
            auto content_type = hdr_content_type ? *hdr_content_type : ""s;
            std::vector<char> initial_buf(data_stream->size());
            std::size_t initial_size = data_stream->peek(std::span{initial_buf});
            dec = decoder::create(content_type, std::span{initial_buf.data(),
                                                          initial_size});
            data_stream->discard(initial_size);
        }
        catch (std::exception& e) {
            cout << "Failed to create decoder with "
                 << data_stream->size()
                 << " bytes: " << e.what() << endl;
        }
    }
    if (!dec)
        return;

    dec->feed(data_stream->read_as<char>());

    if (auto dec_meta = dec->get_metadata()) {
        if (metadata)
            metadata->merge(*dec_meta);
        else
            metadata = std::move(dec_meta);
    }

}
