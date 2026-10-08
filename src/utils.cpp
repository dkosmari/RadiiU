/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>

#include "utils.hpp"


namespace utils {

    bool
    starts_with(std::span<const std::byte> data,
                std::span<const std::byte> prefix)
    {
        return std::ranges::starts_with(data, prefix);
    }

} // namespace utils
