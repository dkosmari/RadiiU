/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATION_HPP
#define STATION_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "json.hpp"


struct Station {

    // int order = 0;
    std::string name;
    std::string url;
    std::string url_resolved;
    std::string homepage;
    std::string favicon;
    std::string country_code;
    std::string uuid;

    // Volatile values, never stored.
    std::uint64_t votes = 0;
    std::uint64_t click_count = 0;
    std::int64_t click_trend = 0;
    unsigned bitrate = 0;

    std::vector<std::string> languages;
    std::vector<std::string> tags;


    static
    Station
    from_json(const json::object& obj);


    json::object
    to_json()
        const;

}; // struct Station


bool
operator ==(const Station& a,
            const Station& b)
    noexcept;

#endif
