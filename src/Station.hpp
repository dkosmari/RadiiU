/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATION_HPP
#define STATION_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glaze/core/meta.hpp>

#include "csv_strings.hpp"
#include "RadioBrowserAPI.hpp"


struct Station {

    std::string stationuuid;
    std::string name;
    std::string url;
    std::string url_resolved;
    std::string homepage;
    std::string favicon;
    std::string countrycode;

    csv_strings language;
    csv_strings tags;

    // Volatile fields, never stored.
    std::uint64_t votes = 0;
    std::uint64_t click_count = 0;
    int click_trend = 0;
    unsigned bitrate = 0;
    std::string codec;


    static
    Station
    from_radio_browser(const RadioBrowserAPI::Station& st);

}; // struct Station


// Comparison ignores volatile fields.
bool
operator ==(const Station& a,
            const Station& b)
    noexcept;


template<>
struct glz::meta<Station> {
    static
    constexpr
    bool
    skip(const std::string_view key,
         const meta_context&) {
        using namespace std::literals;
        if (key == "votes"sv ||
            key == "click_count"sv ||
            key == "click_trend"sv ||
            key == "bitrate"sv ||
            key == "codec"sv)
            return true;
        return false;
    }
};

#endif
