/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdexcept>

#include "TabID.hpp"


TabID::TabID(unsigned idx)
    noexcept :
    value{static_cast<Name>(idx)}
{}


std::size_t
TabID::count()
    noexcept
{
    return static_cast<std::size_t>(Name::num_tabs);
}


TabID
TabID::from_string(const std::string& str)
{
    if (str == "favorites")
        return TabID::favorites;
    if (str == "browser")
        return TabID::browser;
    if (str == "recent")
        return TabID::recent;
    if (str == "player")
        return TabID::player;
    if (str == "settings")
        return TabID::settings;
    if (str == "about")
        return TabID::about;
    if (str == "last_active")
        return TabID::last_active;
    throw std::runtime_error{"invalid TabID string: " + str};
}


std::string
to_string(TabID tab)
{
    switch (tab.value) {
        case TabID::Name::favorites:
            return "favorites";
        case TabID::Name::browser:
            return "browser";
        case TabID::Name::recent:
            return "recent";
        case TabID::Name::player:
            return "player";
        case TabID::Name::settings:
            return "settings";
        case TabID::Name::about:
            return "about";
        case TabID::Name::last_active:
            return "last_active";
        default:
            throw std::runtime_error{"invalid TabID value: " + static_cast<unsigned>(tab.value)};
    }
}


std::string
to_ui_string(TabID tab)
{
    switch (tab.value) {
        case TabID::Name::favorites:
            return "★ Favorites";
        case TabID::Name::browser:
            return "🔍 Browser";
        case TabID::Name::recent:
            return "🕓 Recent";
        case TabID::Name::player:
            return "🎧 Player";
        case TabID::Name::settings:
            return "⚙ Settings";
        case TabID::Name::about:
            return "❗ About";
        case TabID::Name::last_active:
            return "Last active";
        default:
            throw std::runtime_error{"invalid TabID value: " + static_cast<unsigned>(tab.value)};
    }
}
