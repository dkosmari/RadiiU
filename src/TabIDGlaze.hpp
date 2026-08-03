/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TAB_ID_GLAZE_HPP
#define TAB_ID_GLAZE_HPP

#include <glaze/core/meta.hpp>

#include "TabID.hpp"


template<>
struct glz::meta<TabID> {
    using enum TabID;
    static constexpr
    auto value = enumerate(favorites,
                           browser,
                           recent,
                           player,
                           settings,
                           logs,
                           about,
                           last_active,
                           count);
}; // struct glz::meta<TabID>

#endif
