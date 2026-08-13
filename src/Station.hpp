/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATION_HPP
#define STATION_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>


namespace RadioBrowserAPI {
    struct Station;
}

struct Station {

    std::string stationuuid;
    std::string name;
    std::string url;
    std::string url_resolved;
    std::string homepage;
    std::string favicon;
    std::string countrycode;

    std::vector<std::string> language; // special serialization needed
    std::vector<std::string> tags; // special serialization needed

    // Volatile fields, never stored.
    mutable std::optional<std::string> codec;
    mutable std::optional<unsigned> bitrate;
    mutable std::optional<std::uint64_t> votes;
    mutable std::optional<std::uint64_t> click_count;
    mutable std::optional<int> click_trend;

    // UI-related fields, never stored.
    mutable bool expanded = false;

    static
    Station
    from_radio_browser(const RadioBrowserAPI::Station& st);

}; // struct Station


using StationPtr = std::shared_ptr<Station>;

using ConstStationPtr = std::shared_ptr<Station>;


// Comparison ignores volatile fields.
bool
operator ==(const Station& a,
            const Station& b)
    noexcept;

#endif
