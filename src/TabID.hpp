/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TAB_ID_HPP
#define TAB_ID_HPP

#include <string>

#include <glaze/core/common.hpp>
#include <glaze/forward.hpp>


enum class TabID : unsigned {
    favorites,
    browser,
    recent,
    player,
    settings,
    about,
    last_active,
    num_tabs
};


[[nodiscard]]
std::string
to_string(TabID tab);


[[nodiscard]]
std::string
to_label(TabID tab);


template<>
struct glz::meta<TabID> {
    using enum TabID;
    static
    constexpr
    auto value = enumerate(favorites,
                           browser,
                           recent,
                           player,
                           settings,
                           about,
                           last_active,
                           num_tabs);
}; // struct glz::meta<TabID>

#endif
