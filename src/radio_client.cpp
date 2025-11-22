/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>

#include "radio_client.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


radio_client::radio_client(const std::string& url) :
    base_url{url},
    http{base_url},
    data_stream{&http.data_stream}
{
    http.add_header("Icy-MetaData: 1");
    // http.add_accept("audio/*");

    current_state = state::waiting_response;
}


void
radio_client::process()
{
    http.process();
    if (icy_stream)
        icy_stream->process();

    switch (current_state) {

        case state::stopped:
            return;

        case state::waiting_response:
            if (http.responded)
                process_http_response();
            break;

        case state::handling_playlist:
            process_playlist();
            break;

        case state::handling_audio:
            process_audio();
            break;
    }
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
radio_client::process_http_response()
{
    cout << "radio_client::process_http_response()" << endl;

    auto content_type = http.get_header("content-type");
    if (!content_type) {
        cout << "ERROR: server provided no content-type" << endl;
        current_state = state::stopped;
        return;
    }

    if (content_type->ends_with("mpegurl")) {
        // TODO: use a mime matching function to allow more variations
        /*
         * Known mime types for m3u playlists:
         * application/mpegurl
         * application/x-mpegurl
         * audio/mpegurl
         */
        current_state = state::handling_playlist;
    } else if (content_type->starts_with("audio/") || content_type->starts_with("application/")) {
        // TODO: audio streams can have mime audio/* or application/*
        current_state = state::handling_audio;
        try {
            cout << "Trying to create icy_stream" << endl;
            icy_stream = std::make_unique<icy::stream>(http);
            data_stream = &icy_stream->data_stream;
            metadata = icy_stream->get_metadata();
        }
        catch (std::exception& e) {
            cout << "ERROR: " << e.what() << endl;
        }
    }
}


void
radio_client::process_playlist()
{
    // TODO: parse playlist, find new URL, reset the http client, update current_state
    cout << "TODO: radio_client::process_playlist()" << endl;
    if (!http.finished)
        return;

}


void
radio_client::process_audio()
{
    if (icy_stream)
        metadata = icy_stream->get_metadata();

    if (!dec) {
        if (data_stream->size() < 4096)
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
            cout << "Failed to create decoder: " << e.what() << endl;
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
