/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HUMANIZE_HPP
#define HUMANIZE_HPP

#include <chrono>
#include <string>
#include <type_traits>


namespace humanize {

    [[nodiscard]]
    std::string
    duration(std::chrono::seconds t,
             bool separate = true);


    template<typename Rep,
             typename Period>
    requires (
        !std::is_same_v<std::chrono::duration<Rep, Period>,
                        std::chrono::seconds>
    )
    [[nodiscard]]
    std::string
    duration(std::chrono::duration<Rep, Period> t,
             bool separate = true)
    {
        return duration(
            duration_cast<std::chrono::seconds>(t),
            separate
        );
    }


    [[nodiscard]]
    std::string
    duration_brief(std::chrono::seconds t);


    [[nodiscard]]
    std::string
    value(std::uint64_t x,
          bool separate = true);

} // namespace humanize

#endif
