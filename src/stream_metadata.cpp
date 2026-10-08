/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ostream>

#include "stream_metadata.hpp"


void
stream_metadata::merge(const stream_metadata& other)
{
    if (other.title && title != other.title)
        title = other.title;

    if (other.artist && artist != other.artist)
        artist = other.artist;

    if (other.album && album != other.album)
        album = other.album;

    if (other.genre && genre != other.genre)
        genre = other.genre;

    if (other.date && date != other.date)
        date = other.date;

    if (other.cover_art && cover_art != other.cover_art)
        cover_art = other.cover_art;

    if (other.station.name && station.name != other.station.name)
        station.name = other.station.name;

    if (other.station.description && station.description != other.station.description)
        station.description = other.station.description;

    if (other.station.url && station.url != other.station.url)
        station.url = other.station.url;

    for (const auto& [key, val] : other.extra)
        if (!val.empty()) {
            auto it = extra.find(key);
            if (it != extra.end()) {
                // already have key
                if (it->second == val)
                    continue;
                it->second = val;
            } else {
                // key not found, just insert it
                extra[key] = val;
            }
        }
}


std::ostream&
operator <<(std::ostream& out,
            const stream_metadata& m)
{
    if (m.title)
        out << "Title: " << *m.title << '\n';

    if (m.artist)
        out << "Artist: " << *m.artist << '\n';

    if (m.album)
        out << "Album: " << *m.album << '\n';

    if (m.genre)
        out << "Genre: " << *m.genre << '\n';

    if (m.date)
        out << "Date: " << *m.date << '\n';

    if (m.cover_art)
        out << "Cover Art: " << *m.cover_art << '\n';

    if (m.station.name)
        out << "Station Name: " << *m.station.name << '\n';

    if (m.station.genre)
        out << "Station Genre: " << *m.station.genre << '\n';

    if (m.station.description)
        out << "Station Description: " << *m.station.description << '\n';

    if (m.station.url)
        out << "Station URL: " << *m.station.url << '\n';

    if (!m.extra.empty()) {
        out << "Extra:\n";
        for (auto& [k, v] : m.extra) {
            out << "    " << k << ": " << v << '\n';
        }
    }

    return out;
}
