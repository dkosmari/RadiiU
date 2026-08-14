/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <functional>
#include <vector>

#include "http_client.hpp"

#include "LogManager.hpp"
#include "LogManagerCurl.hpp"
#include "tracer.hpp"


using namespace std::literals;


http_client::http_client(const std::string& user_agent) :
    user_agent{user_agent}
{
    TRACE_FUNC;

    multi.set_max_total_connections(2);
}


http_client::~http_client()
    noexcept
{
    TRACE_FUNC;

    auto e = multi.try_remove(easy);
    if (!e)
        LOG_ERROR("BUG: removing easy handle failed: {}", e.error().what());
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
http_client::set_url(const std::string& url,
                     bool verbose)
{
    // TRACE_FUNC;

    multi.remove(easy);

    easy.reset();
    easy.set_verbose(verbose);
    LogManagerCurl::capture_curl_debug(easy);
    if (!user_agent.empty())
        easy.set_user_agent(user_agent);
    easy.set_accept_encoding("");
    easy.set_auto_referer(true);
    easy.set_buffer_size(1 * 1024 * 1024);
    easy.set_fail_on_error(true);
    easy.set_follow_location(true);
    easy.set_forbid_reuse(false);
    easy.set_http_09_allowed(true);
    easy.set_http_200_aliases({"ICY 200 OK"});
    easy.set_http_version(curl::easy::http_version::none);
    easy.set_ssl_verify_peer(false);
    easy.set_tcp_no_delay(false);
    easy.set_transfer_encoding(true);
    easy.set_url(url);
    easy.set_write_function(std::bind_front(&http_client::curl_write_callback, this));

    multi.add(easy);

    request_prepared = false;
    response_started = false;
    pending_on_response_started = false;
    pending_on_recv = false;

    data_stream.clear();
}


std::string
http_client::get_effective_url()
    const
{
    return easy.get_effective_url();
}


void
http_client::process()
{
    if (!request_prepared) {
        for (auto& hdr : headers)
            easy.append_http_header(hdr);
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
        LOG_DEBUG("End of connection detected.");
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
