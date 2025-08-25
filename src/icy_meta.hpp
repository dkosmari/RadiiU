/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ICY_META_HPP
#define ICY_META_HPP

#include <map>
#include <string>


namespace icy_meta {

    std::map<std::string, std::string>
    parse(std::string input);

} // namespace icy_meta

#endif
