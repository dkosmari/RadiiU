/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STREAM_METADATA_HPP
#define STREAM_METADATA_HPP

#include <iosfwd>
#include <string>
#include <optional>
#include <unordered_map>


struct stream_metadata {

    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> album;
    std::optional<std::string> genre;
    std::optional<std::string> station_name;
    std::optional<std::string> station_genre;
    std::optional<std::string> station_description;
    std::optional<std::string> station_url;
    std::unordered_map<std::string, std::string> extra;

    void
    merge(const stream_metadata& other);

}; // struct stream_metadata


std::ostream&
operator <<(std::ostream& out,
            const stream_metadata& m);

#endif
