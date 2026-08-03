/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LOG_MANAGER_CURL_HPP
#define LOG_MANAGER_CURL_HPP

#include <source_location>

#include <curlxx/easy.hpp>


namespace LogManagerCurl {

    void
    capture_curl_debug(curl::easy& easy,
                       const std::source_location& location = std::source_location::current());

} // namespace LogManagerCurl

#endif
