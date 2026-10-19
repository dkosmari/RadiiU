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
#include <stop_token>


namespace utils {

    void
    remove_prefix(std::span<const std::byte>& input,
                  std::size_t n);


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

    void
    throw_if_stopped(std::stop_token& stopper);

} // namespace utils

#endif
