/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATION_GLAZE_HPP
#define STATION_GLAZE_HPP

#include <glaze/core/meta.hpp>

#include "string_utils.hpp"


template<>
struct glz::meta<Station> {

    static constexpr
    auto read_language =
        [](Station& self, const std::string& arg)
        {
            self.language = string_utils::from_csv(arg);
        };

    static constexpr
    auto write_language =
        [](const Station& self) -> std::string
        {
            return string_utils::to_csv(self.language);
        };


    static constexpr
    auto read_tags =
        [](Station& self, const std::string& arg)
        {
            self.tags = string_utils::from_csv(arg);
        };

    static constexpr
    auto write_tags =
        [](const Station& self) -> std::string
        {
            return string_utils::to_csv(self.tags);
        };


    static constexpr
    auto modify = object(
        "language", glz::custom<read_language, write_language>,
        "tags", glz::custom<read_tags, write_tags>
    );


    static constexpr
    bool
    skip(const std::string_view key,
         const meta_context&) {
        using namespace std::literals;
        if (key == "votes"sv ||
            key == "click_count"sv ||
            key == "click_trend"sv ||
            key == "bitrate"sv ||
            key == "codec"sv)
            return true;
        return false;
    }

}; // struct glz::meta<Station>

#endif
