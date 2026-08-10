/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TAB_ID_HPP
#define TAB_ID_HPP

#include <string>


enum class TabID : unsigned {
    browser,
    favorites,
    recent,
    player,
    settings,
    logs,
    about,
    last_active,
    count
};


[[nodiscard]]
std::string
to_string(TabID tab);


[[nodiscard]]
std::string
to_label(TabID tab);

#endif
