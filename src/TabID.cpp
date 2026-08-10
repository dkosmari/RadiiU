/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdexcept>

#include "TabID.hpp"

#include "IconsFontAwesome4.h"


using namespace std::literals;


std::string
to_string(TabID tab)
{
    switch (tab) {
        using enum TabID;
        case browser:
            return "browser"s;
        case favorites:
            return "favorites"s;
        case recent:
            return "recent"s;
        case player:
            return "player"s;
        case settings:
            return "settings"s;
        case logs:
            return "logs"s;
        case about:
            return "about"s;
        case last_active:
            return "last_active"s;
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
        case browser:
            return ICON_FA_GLOBE " Browser"s;
        case favorites:
            return ICON_FA_HEART " Favorites"s;
        case recent:
            return ICON_FA_HISTORY " Recent"s;
        case player:
            return ICON_FA_MUSIC " Player"s;
        case settings:
            return ICON_FA_SLIDERS " Settings"s;
        case logs:
            return ICON_FA_LIST_ALT " Logs"s;
        case about:
            return ICON_FA_LIGHTBULB_O " About"s;
        case last_active:
            return "Last active"s;
        default:
            throw std::runtime_error{"invalid TabID value: "
                                     + static_cast<std::underlying_type_t<TabID>>(tab)};
    }
}
