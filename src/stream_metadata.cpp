/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ostream>

#include "stream_metadata.hpp"


using namespace std::literals;


namespace {


    void
    erase_if_empty_str(std::optional<std::string>& field)
    {
        if (field && field->empty())
            field.reset();
    }


    template<typename T,
             typename Ptr>
    void
    move_if_present(T& dst,
                    T& src,
                    Ptr member_ptr)
    {
        if (!(src.*member_ptr))
            return;

        if (dst.*member_ptr == src.*member_ptr)
            return;

        dst.*member_ptr = std::move(src.*member_ptr);

        (src.*member_ptr).reset(); // ensure src is cleared
    }


    void
    print_opt(std::ostream& out,
              const std::string& label,
              const std::optional<std::string> value)
    {
        if (value)
            out << label << ": " << *value << '\n';
    }

} // namespace


void
stream_metadata::merge(stream_metadata&& other)
{
    // Track fields
    move_if_present(*this, other, &stream_metadata::title);
    move_if_present(*this, other, &stream_metadata::artist);
    move_if_present(*this, other, &stream_metadata::album);
    move_if_present(*this, other, &stream_metadata::genre);
    move_if_present(*this, other, &stream_metadata::date);
    move_if_present(*this, other, &stream_metadata::cover_art);

    // Station fields
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::name);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::genre);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::description);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::url);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::bitrate);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::audio_info);

    // Extra fields
    for (auto oit = other.extra.begin(); oit != other.extra.end(); ++oit) {
        const auto& key = oit->first;
        auto& val = oit->second;
        auto it = extra.find(key);
        if (it != extra.end())
            it->second = std::move(val);
        else
            extra.emplace(key, std::move(val));
    }
    other.extra.clear();

    clean_empty();
}


void
stream_metadata::clean_empty()
    noexcept
{
    erase_if_empty_str(title);
    erase_if_empty_str(artist);
    erase_if_empty_str(album);
    erase_if_empty_str(genre);
    erase_if_empty_str(date);
    erase_if_empty_str(cover_art);

    erase_if_empty_str(station.name);
    erase_if_empty_str(station.genre);
    erase_if_empty_str(station.description);
    erase_if_empty_str(station.url);
    erase_if_empty_str(station.bitrate);
    erase_if_empty_str(station.audio_info);

    for (auto it = extra.begin(); it != extra.end();) {
        if (it->second.empty())
            it = extra.erase(it);
        else
            ++it;
    }
}


std::ostream&
operator <<(std::ostream& out,
            const stream_metadata& m)
{
    print_opt(out, "Title"s, m.title);
    print_opt(out, "Artist"s, m.artist);
    print_opt(out, "Album"s, m.album);
    print_opt(out, "Genre"s, m.genre);
    print_opt(out, "Date"s, m.date);
    print_opt(out, "Cover Art"s, m.cover_art);

    print_opt(out, "Station Name"s, m.station.name);
    print_opt(out, "Station Genre"s, m.station.genre);
    print_opt(out, "Station Description"s, m.station.description);
    print_opt(out, "Station URL"s, m.station.url);
    print_opt(out, "Station Bitrate"s, m.station.bitrate);
    print_opt(out, "Station Audio Info"s, m.station.audio_info);

    if (!m.extra.empty()) {
        out << "Extra:\n";
        for (const auto& [key, val] : m.extra) {
            out << "    " << key << ": " << val << '\n';
        }
    }

    return out;
}
