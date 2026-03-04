/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLS_HPP
#define PLS_HPP

#include <chrono>
#include <cstddef>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>


namespace pls {

    struct error : std::runtime_error {

        error(const std::string& msg);

    }; // struct error


    struct track {
        std::string url;
        std::optional<std::string> title;
        std::optional<std::chrono::seconds> length;
    }; // struct track


    void
    write(std::ostream& out,
          const track& trk,
          std::size_t num);


    using playlist = std::vector<track>;


    std::ostream&
    operator <<(std::ostream& out,
                const playlist& pl);


    playlist
    parse(const std::string& input);

} // namespace pls

#endif
