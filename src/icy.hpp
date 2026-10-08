/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ICY_HPP
#define ICY_HPP

#include <string_view>
#include <unordered_map>


namespace icy {

    using dict_t = std::unordered_map<std::string_view, std::string_view>;

    dict_t
    parse(std::string_view input);

} // namespace icy

#endif
