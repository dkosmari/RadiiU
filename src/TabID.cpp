/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdexcept>

#include "TabID.hpp"

#include "IconsFontAwesome4.h"


std::string
to_string(TabID tab)
{
    switch (tab) {
        using enum TabID;
        case favorites:
            return "favorites";
        case browser:
            return "browser";
        case recent:
            return "recent";
        case player:
            return "player";
        case settings:
            return "settings";
        case about:
            return "about";
        case last_active:
            return "last_active";
        default:
            throw std::runtime_error{"invalid TabID value: "
                                     + static_cast<std::underlying_type_t<TabID>>(tab)};
    }
}


std::string
to_label(TabID tab)
{
    switch (tab) {
        using enum TabID;
        case favorites:
            return ICON_FA_HEART " Favorites";
        case browser:
            return ICON_FA_GLOBE " Browser";
        case recent:
            return ICON_FA_HISTORY " Recent";
        case player:
            return ICON_FA_MUSIC " Player";
        case settings:
            return ICON_FA_SLIDERS " Settings";
        case about:
            return ICON_FA_LIGHTBULB_O " About";
        case last_active:
            return "Last active";
        default:
            throw std::runtime_error{"invalid TabID value: "
                                     + static_cast<std::underlying_type_t<TabID>>(tab)};
    }
}
