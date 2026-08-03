/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <span>
#include <string_view>

#include "LogManagerCurl.hpp"

#include "IconsFontAwesome4.h"
#include "LogManager.hpp"


namespace LogManagerCurl {

    void
    capture_curl_debug(curl::easy& easy,
                       const std::source_location& location)
    {
        easy.set_debug_function(
            [location](CURL*,
                       curl_infotype type,
                       std::span<const char> data)
            {
                switch (type) {
                    case CURLINFO_TEXT:
                        LogManager::log(LogLevel::debug,
                                        location,
                                        "CURL " ICON_FA_INFO_CIRCLE,
                                        "{}",
                                        std::string_view(data.data(), data.size()));
                        break;

                    case CURLINFO_HEADER_IN:
                        LogManager::log(LogLevel::debug,
                                        location,
                                        "CURL " ICON_FA_ARROW_CIRCLE_O_DOWN,
                                        "{}",
                                        std::string_view(data.data(), data.size()));
                        break;

                    case CURLINFO_HEADER_OUT:
                        LogManager::log(LogLevel::debug,
                                        location,
                                        "CURL " ICON_FA_ARROW_CIRCLE_O_UP,
                                        "{}",
                                        std::string_view(data.data(), data.size()));
                        break;

                    default:
                        ;
                }
            }
        );
    }

} // namespace LogManagerCurl
