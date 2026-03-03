/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <functional>
#include <iostream>
#include <vector>

#include "http_client.hpp"

#include "tracer.hpp"
#include "utils.hpp"
#include "string_utils.hpp"


using std::cout;
using std::endl;

using namespace std::literals;
using namespace std::placeholders;


http_client::http_client(const std::string& url)
{
    multi.set_max_total_connections(2);
    set_url(url);
}


http_client::~http_client()
    noexcept
{
    multi.try_remove(easy);
}


// std::optional<stream_metadata>
// http_client::get_metadata()
//     const
// {
//     std::optional<stream_metadata> result;
//     if (metadata)
//         result = metadata;
//     if (dec) {
//         if (auto decoder_metadata = dec->get_metadata()) {
//             if (result)
//                 result->merge(*decoder_metadata);
//             else
//                 result = *decoder_metadata;
//         }
//     }
//     return result;
// }


void
http_client::add_header(const std::string& hdr)
{
    headers.push_back(hdr);
}


void
http_client::add_accept(const std::string& mime)
{
    accepts.push_back(mime);
}


void
http_client::set_url(const std::string& url)
{
    // TRACE_FUNC;

    multi.try_remove(easy);

    easy.reset();
    easy.set_verbose(true); // DEBUG
    easy.set_user_agent(utils::get_user_agent());
    easy.set_forbid_reuse(false);
    easy.set_follow_location(true);
    easy.set_ssl_verify_peer(false);
    easy.set_accept_encoding("");
    easy.set_transfer_encoding(true);
    easy.set_write_function(std::bind(&http_client::curl_write_callback, this, _1));
    easy.set_url(url);

    multi.add(easy);

    request_prepared = false;
    response_started = false;
    pending_on_response_started = false;
    pending_on_recv = false;

    data_stream.clear();
}


void
http_client::process()
{
    if (!request_prepared) {
        if (!accepts.empty())
            easy.set_http_headers({
                    "Accept: "s + string_utils::join(accepts, ",")
                });

        headers.push_back("Accept: " + string_utils::join(accepts, ","));
        easy.set_http_headers(headers);
        request_prepared = true;
    }

    multi.perform();

    // Note: we invoke them here, not inside the curl callback.
    if  (pending_on_response_started) {
        if (on_response_started)
            on_response_started();
        pending_on_response_started = false;
    }

    if (pending_on_recv) {
        if (on_recv)
            on_recv();
        pending_on_recv = false;
    }

    // check for completion
    auto done = multi.get_done();
    for (const auto& msg : done)
        if (msg.handle == &easy) {
            if (msg.result != CURLE_OK)
                throw std::runtime_error{"http_client::process(): " + std::to_string(msg.result)};
            if (on_response_finished)
                on_response_finished();
        }
}


std::optional<std::string>
http_client::get_header(const std::string& name)
{
    if (!response_started)
        return {};

    auto result = easy.try_get_header(name);
    if (result)
        return result->value;
    return {};
}


std::size_t
http_client::curl_write_callback(std::span<const char> buf)
{
    if (buf.empty()) {
        cout << "End of connection detected." << endl;
        return 0;
    }

    data_stream.write(buf);

    if (!response_started) {
        response_started = true;
        pending_on_response_started = true;
    }

    pending_on_recv = true;

    return buf.size();
}
