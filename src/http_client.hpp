/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <curlxx/curl.hpp>

#include "byte_stream.hpp"


struct http_client {

    curl::multi multi;
    curl::easy easy;
    bool request_prepared = false;
    bool response_started = false;
    byte_stream data_stream;


    std::function<void()> on_response_started;
    std::function<void()> on_response_finished;
    std::function<void()> on_recv;


    // disallow moving
    http_client(http_client&&) = delete;


    http_client(const std::string& url);

    ~http_client()
        noexcept;


    void
    add_accept(const std::string& mime);

    void
    add_header(const std::string& hdr);


    void
    set_url(const std::string& url);


    void
    process();


    std::optional<std::string>
    get_header(const std::string& name);


private:

    std::vector<std::string> headers;
    std::vector<std::string> accepts;

    std::size_t
    curl_write_callback(std::span<const char> buf);

    bool pending_on_response_started = false;
    bool pending_on_recv = false;

}; // struct http_client

#endif
