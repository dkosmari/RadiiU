/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STREAM_METADATA_HPP
#define STREAM_METADATA_HPP

#include <flat_map>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>


struct stream_metadata {

    std::optional<std::string> title;
    std::optional<std::string> artist;
    std::optional<std::string> album;
    std::optional<std::string> genre;
    std::optional<std::string> date;
    std::optional<std::string> cover_art;

    struct station_metadata {
        std::optional<std::string> name;
        std::optional<std::string> genre;
        std::optional<std::string> description;
        std::optional<std::string> url;
        std::optional<std::string> bitrate;
        std::optional<std::string> audio_info;
        std::vector<std::string> notice;
    };

    station_metadata station;

    std::flat_map<std::string, std::string> extra;


    void
    merge(stream_metadata&& other);


    void
    clean_empty()
        noexcept;

}; // struct stream_metadata


std::ostream&
operator <<(std::ostream& out,
            const stream_metadata& m);

#endif
