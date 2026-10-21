/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ostream>
#include <ranges>
#include <string_view>

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
              std::string_view label,
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
    move_if_present(*this, other, &stream_metadata::album);
    move_if_present(*this, other, &stream_metadata::album_art);
    move_if_present(*this, other, &stream_metadata::artist);
    move_if_present(*this, other, &stream_metadata::composer);
    move_if_present(*this, other, &stream_metadata::cover_art);
    move_if_present(*this, other, &stream_metadata::date);
    move_if_present(*this, other, &stream_metadata::genre);
    move_if_present(*this, other, &stream_metadata::label);
    move_if_present(*this, other, &stream_metadata::next);
    move_if_present(*this, other, &stream_metadata::publisher);
    move_if_present(*this, other, &stream_metadata::title);
    move_if_present(*this, other, &stream_metadata::track);

    // Station fields
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::audio_info);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::description);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::genre);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::name);
    move_if_present(this->station, other.station, &stream_metadata::station_metadata::url);

    if (!other.station.notice.empty()) {
        station.notice = std::move(other.station.notice);
        other.station.notice.clear();
    }

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
    erase_if_empty_str(album);
    erase_if_empty_str(album_art);
    erase_if_empty_str(artist);
    erase_if_empty_str(composer);
    erase_if_empty_str(cover_art);
    erase_if_empty_str(date);
    erase_if_empty_str(genre);
    erase_if_empty_str(label);
    erase_if_empty_str(next);
    erase_if_empty_str(publisher);
    erase_if_empty_str(title);
    erase_if_empty_str(track);

    erase_if_empty_str(station.audio_info);
    erase_if_empty_str(station.description);
    erase_if_empty_str(station.genre);
    erase_if_empty_str(station.name);
    erase_if_empty_str(station.url);

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
    print_opt(out, "Album"sv,     m.album);
    print_opt(out, "Album Art"sv, m.album_art);
    print_opt(out, "Artist"sv,    m.artist);
    print_opt(out, "Composer"sv,  m.composer);
    print_opt(out, "Cover Art"sv, m.cover_art);
    print_opt(out, "Date"sv,      m.date);
    print_opt(out, "Genre"sv,     m.genre);
    print_opt(out, "Label"sv,     m.label);
    print_opt(out, "Next"sv,      m.next);
    print_opt(out, "Publisher"sv, m.publisher);
    print_opt(out, "Title"sv,     m.title);
    print_opt(out, "Track"sv,     m.track);

    print_opt(out, "Audio Info"sv,  m.station.audio_info);
    print_opt(out, "Description"sv, m.station.description);
    print_opt(out, "Genre"sv,       m.station.genre);
    print_opt(out, "Name"sv,        m.station.name);
    print_opt(out, "URL"sv,         m.station.url);

    for (auto [index, line] : m.station.notice | std::views::enumerate)
        out << "Notice " << (index + 1) << ": " << line << '\n';

    if (!m.extra.empty()) {
        out << "Extra:\n";
        for (const auto& [key, val] : m.extra) {
            out << "    " << key << ": " << val << '\n';
        }
    }

    return out;
}
