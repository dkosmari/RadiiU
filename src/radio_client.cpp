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
#include "pls.hpp"
#include "tracer.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace {

    // Mime types for M3U playlists
    const std::vector<std::string> m3u_mimes{
        "audio/mpegurl",
        "audio/x-mpegurl",
        "application/vnd.apple.mpegurl",
        "application/vnd.apple.mpegurl.audio",
        "application/mpegurl",
        "application/x-mpegurl",
    };

    // Mime types for PLS playlists
    const std::vector<std::string> pls_mimes{
        "audio/x-scpls",
    };

    // Mime types for audio streams
    const std::vector<std::string> audio_mimes{
        "audio/*",
        "application/ogg",
    };

} // namespace


radio_client::radio_client(const std::string& url,
                           const std::string& url_resolved,
                           const std::string& user_agent) :
    url{url},
    url_resolved{url_resolved},
    user_agent{user_agent},
    http{user_agent},
    data_stream{&http.data_stream}
{
    TRACE_FUNC;

    http.add_header("Icy-MetaData: 1");

    // Note: these callbacks are invoked during http::process()
    http.on_response_started  = [this] { process_http_response_started();  };
    http.on_response_finished = [this] { process_http_response_finished(); };
    http.on_recv = [this] { process_http_recv(); };

    if (!url_resolved.empty())
        set_next_url(url_resolved);
    else
        set_next_url(url);
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
radio_client::set_next_url(const std::string& next_url)
{
    icy_stream.reset();

    http.set_url(next_url);
    if (next_url.empty())
        current_state = state::stopped;
    else
        current_state = state::started;
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

    if (mime_type::match(*content_type, m3u_mimes)) {
        cout << "Detected M3U mime: " << *content_type << endl;
        current_state = state::receiving_playlist;
        current_playlist = playlist_type::m3u;
    } else if (mime_type::match(*content_type, pls_mimes)) {
        cout << "Detected PLS mime: " << *content_type << endl;
        current_state = state::receiving_playlist;
        current_playlist = playlist_type::pls;
    } else if (mime_type::match(*content_type, audio_mimes)) {
        cout << "Detected audio mime: " << *content_type << endl;
        current_state = state::streaming_audio;
        try {
            cout << "Trying to create ICY stream" << endl;
            icy_stream = std::make_unique<icy::stream>(http);
            cout << "ICY stream created. " << endl;
            data_stream = &icy_stream->data_stream;
            metadata = icy_stream->get_metadata();
        }
        catch (std::exception& e) {
            cout << "Could not create ICY stream: " << e.what() << endl;
        }
    } else
        cout << "ERROR: don't know how to handle mime-type: " << *content_type << endl;
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

    url_resolved.clear();

    std::string data = data_stream->read_str();

    switch (current_playlist) {
        case playlist_type::m3u: {
            try {
                auto pl = m3u::parse(data);
                url_resolved = pl.at(0).url;
            }
            catch (std::exception& e) {
                cout << "ERROR: " << e.what() << endl;
                cout << "<m3u>\n" << data << "\n</m3u>" << endl;
            }
            break;
        }

        case playlist_type::pls: {
            try {
                auto pl = pls::parse(data);
                url_resolved = pl.at(0).url;
            }
            catch (std::exception& e) {
                cout << "ERROR: " << e.what() << endl;
                cout << "<pls>\n" << data << "\n</pls>" << endl;
            }
            break;
        }

        default:
            cout << "ERROR: should not invoke process_playlist() when no playlist" << endl;
    } // switch (current_playlist)

    cout << "radio_client::url_resolved = \"" << url_resolved << "\"" << endl;
    set_next_url(url_resolved);
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
