/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MIME_TYPE_HPP
#define MIME_TYPE_HPP

#include <concepts>
#include <ranges>
#include <string>
#include <string_view>


namespace mime_type {

    /*
     * input: must have no wildcards, parameters are currently ignored
     * candidate: can have wildcards
     */

    bool
    match(const std::string& input,
          std::string_view candidate);

    /*
     * input: must have no wildcards, parameters are currently ignored
     * candidates: can have wildcards
     */
    template<std::ranges::input_range R>
    requires (std::convertible_to<std::ranges::range_value_t<R>, std::string_view>)
    bool
    match_any(const std::string& input,
              R&& candidates)
    {
        for (const auto& candidate : candidates)
            if (match(input, std::string_view{candidate}))
                return true;
        return false;
    }

} // namespace mime_type

#endif
