/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef M3U_HPP
#define M3U_HPP

#include <chrono>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <vector>


namespace m3u {

    struct track {
        std::string url;
        std::optional<std::chrono::seconds> duration;
        std::optional<std::string> title;
    };


    void
    write(std::ostream& out,
          const track& trk,
          bool is_ext);


    using playlist = std::vector<track>;


    std::ostream&
    operator <<(std::ostream& out,
                const playlist& pl);


    playlist
    parse(const std::string& input);


    bool
    match_mime(const std::string& content_type);


    bool
    match_path(const std::string& path);


    bool
    match_prefix(std::span<const std::byte> data);

} // namespace m3u

#endif
