/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdexcept>

#include "TabIndex.hpp"


TabIndex
to_tab_index(const std::string& str)
{
    if (str == "favorites")
        return TabIndex::favorites;
    if (str == "browser")
        return TabIndex::browser;
    if (str == "recent")
        return TabIndex::recent;
    if (str == "player")
        return TabIndex::player;
    if (str == "settings")
        return TabIndex::settings;
    if (str == "about")
        return TabIndex::about;
    if (str == "last_active")
        return TabIndex::last_active;
    throw std::runtime_error{"invalid TabIndex string: " + str};
}


std::string
to_string(TabIndex idx)
{
    switch (idx) {
        case TabIndex::favorites:
            return "favorites";
        case TabIndex::browser:
            return "browser";
        case TabIndex::recent:
            return "recent";
        case TabIndex::player:
            return "player";
        case TabIndex::settings:
            return "settings";
        case TabIndex::about:
            return "about";
        case TabIndex::last_active:
            return "last_active";
        default:
            throw std::runtime_error{"invalid TabIndex value: " + static_cast<int>(idx)};
    }
}


std::string
to_ui_string(TabIndex idx)
{
    switch (idx) {
        case TabIndex::favorites:
            return "★ Favorites";
        case TabIndex::browser:
            return "🔍 Browser";
        case TabIndex::recent:
            return "🕓 Recent";
        case TabIndex::player:
            return "🎧 Player";
        case TabIndex::settings:
            return "⚙ Settings";
        case TabIndex::about:
            return "❗ About";
        case TabIndex::last_active:
            return "Last active";
        default:
            throw std::runtime_error{"invalid TabIndex value: " + static_cast<int>(idx)};
    }
}
