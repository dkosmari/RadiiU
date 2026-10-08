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

    namespace {

        std::string_view
        to_string_view(std::span<const std::byte> data)
        {
            return {
                reinterpret_cast<const char*>(data.data()),
                data.size()
            };
        }

    } // namespace


    void
    capture_curl_debug(curl::easy& easy,
                       const std::source_location& location)
    {
        easy.set_debug_function(
            [location](curl::easy*,
                       CURL*,
                       curl_infotype type,
                       std::span<const std::byte> data)
            {
                switch (type) {
                    case CURLINFO_TEXT:
                        LogManager::log(LogLevel::debug,
                                        location,
                                        "CURL " ICON_FA_INFO_CIRCLE,
                                        "{}",
                                        to_string_view(data));
                        break;

                    case CURLINFO_HEADER_IN:
                        LogManager::log(LogLevel::debug,
                                        location,
                                        "CURL " ICON_FA_ARROW_CIRCLE_O_DOWN,
                                        "{}",
                                        to_string_view(data));
                        break;

                    case CURLINFO_HEADER_OUT:
                        LogManager::log(LogLevel::debug,
                                        location,
                                        "CURL " ICON_FA_ARROW_CIRCLE_O_UP,
                                        "{}",
                                        to_string_view(data));
                        break;

                    default:
                        ;
                }
            }
        );
    }

} // namespace LogManagerCurl
