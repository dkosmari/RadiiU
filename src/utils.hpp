/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef>
#include <ranges>
#include <span>


namespace utils {

    [[nodiscard]]
    bool
    starts_with(std::span<const std::byte> data,
                std::span<const std::byte> prefix);


    template<std::ranges::input_range R>
    [[nodiscard]]
    bool
    starts_with_any(std::span<const std::byte> data,
                    R&& prefix_list)
    {
        for (auto& prefix : prefix_list) {
            auto prefix_bytes = as_bytes(std::span{prefix});
            if (starts_with(data, prefix_bytes))
                return true;
        }
        return false;
    }

} // namespace utils

#endif
