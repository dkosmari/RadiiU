/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef>
#include <filesystem>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <stop_token>


namespace utils {

    std::optional<std::filesystem::path>
    find_custom_ca_info();


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


    struct stop_requested : std::runtime_error{

        stop_requested();

    }; // struct stop_requested;


    void
    throw_if_stopped(std::stop_token& stoken);

} // namespace utils

#endif
