/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATION_GLAZE_HPP
#define STATION_GLAZE_HPP

#include <glaze/core/common.hpp>

#include "string_utils.hpp"


template<>
struct glz::meta<Station> {

    using T = Station;

    static constexpr
    auto read_language =
        [](T& self, const std::string& arg)
        {
            self.language = string_utils::from_csv(arg);
        };

    static constexpr
    auto write_language =
        [](const T& self) -> std::string
        {
            return string_utils::to_csv(self.language);
        };


    static constexpr
    auto read_tags =
        [](T& self, const std::string& arg)
        {
            self.tags = string_utils::from_csv(arg);
        };

    static constexpr
    auto write_tags =
        [](const T& self) -> std::string
        {
            return string_utils::to_csv(self.tags);
        };


    // NOTE: since we have 7 automatic serializations and 6 skips, it's shorter to do an
    // explicit serialization instead of calling skip().

    static constexpr
    auto value = object(
        "stationuuid",  &T::stationuuid,
        "name",         &T::name,
        "url",          &T::url,
        "url_resolved", &T::url_resolved,
        "homepage",     &T::homepage,
        "favicon",      &T::favicon,
        "countrycode",  &T::countrycode,

        "language", custom<read_language, write_language>,
        "tags",     custom<read_tags, write_tags>

        // The other fields are intentionally ignored.
    );

}; // struct glz::meta<Station>

#endif
