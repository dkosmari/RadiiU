/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef COUNTRY_FLAG_MANAGER_HPP
#define COUNTRY_FLAG_MANAGER_HPP

#include <string>


namespace CountryFlagManager {

    void
    initialize();

    void
    finalize();

    char32_t
    get_codepoint(const std::string& iso_code);

    std::string
    get_utf8(const std::string& iso_code);

} // namespace CountryFlagManager

#endif
