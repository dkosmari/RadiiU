/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <memory>
#include <optional>
#include <string>

#include <curlxx/curl.hpp>

#include "byte_stream.hpp"


struct http_client {

    curl::multi multi;
    curl::easy easy;
    bool requested = false;
    bool responded = false;
    bool finished = false;
    byte_stream data_stream;


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
    process();


    std::optional<std::string>
    get_header(const std::string& name);


private:

    std::vector<std::string> headers;
    std::vector<std::string> accepts;

    std::size_t
    curl_write_callback(std::span<const char> buf);

}; // struct http_client

#endif
