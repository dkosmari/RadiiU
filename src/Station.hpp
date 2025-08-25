/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATION_HPP
#define STATION_HPP

#include <cstdint>
#include <filesystem>
#include <map>
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
    std::string tags;
    std::string country_code;
    std::string language;
    std::string uuid;

    std::uint64_t votes = 0;
    std::uint64_t bitrate = 0;


    static
    Station
    from_json(const json::object& obj);


    json::object
    to_json()
        const;

};

#endif
