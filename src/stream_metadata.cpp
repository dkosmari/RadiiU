/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "stream_metadata.hpp"

#include <ostream>


void
stream_metadata::merge(const stream_metadata& other)
{
    if (other.title)
        title = other.title;
    if (other.artist)
        artist = other.artist;
    if (other.album)
        album = other.album;
    if (other.genre)
        genre = other.genre;
    if (other.station_name)
        station_name = other.station_name;
    if (other.station_description)
        station_description = other.station_description;
    if (other.station_url)
        station_url = other.station_url;
    for (const auto& [k, v] : other.extra)
        if (!v.empty())
            extra[k] = v;
}


std::ostream&
operator <<(std::ostream& out,
            const stream_metadata& m)
{
    if (m.title)
        out << "Title: " << *m.title << '\n';
    if (!m.artist)
        out << "Artist: " << *m.artist << '\n';
    if (!m.album)
        out << "Album: " << *m.album << '\n';
    if (!m.genre)
        out << "Genre: " << *m.genre << '\n';
    if (!m.station_name)
        out << "Station Name: " << *m.station_name << '\n';
    if (m.station_genre)
        out << "Station Genre: " << *m.station_genre << '\n';
    if (m.station_description)
        out << "Station Description: " << *m.station_description << '\n';
    if (m.station_url)
        out << "Station URL: " << *m.station_url << '\n';

    if (!m.extra.empty()) {
        out << "Extra:\n";
        for (auto& [k, v] : m.extra) {
            out << "    " << k << ": " << v << '\n';
        }
    }

    return out;
}
