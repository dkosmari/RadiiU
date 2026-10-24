/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <stdexcept>

#include "utils.hpp"


namespace utils {

    void
    remove_prefix(std::span<const std::byte>& input,
                  std::size_t n)
    {
        input = input.subspan(n);
    }


    bool
    starts_with(std::span<const std::byte> data,
                std::span<const std::byte> prefix)
    {
        return std::ranges::starts_with(data, prefix);
    }


    void
    throw_if_stopped(std::stop_token& stoken)
    {
        if (stoken.stop_requested())
            throw std::runtime_error{"stop requested"};
    }

} // namespace utils
