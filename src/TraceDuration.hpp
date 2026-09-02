/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRACE_DURATION_HPP
#define TRACE_DURATION_HPP

#include <optional>
#include <source_location>
#include <string_view>

#include <glaze/json/generic.hpp>


struct TraceDuration {

    const std::string_view name;
    const std::optional<std::string_view> cat;
    bool success;


    TraceDuration(std::string_view name_,
                  std::optional<std::string_view> cat_ = {},
                  std::optional<glz::generic_u64> args_ = {},
                  const std::source_location& location = std::source_location::current())
        noexcept;


    TraceDuration(TraceDuration&&) = delete;


    ~TraceDuration()
        noexcept;

}; // struct TraceDuration

#endif
