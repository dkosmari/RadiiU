/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TAB_INDEX_HPP
#define TAB_INDEX_HPP

#include <string>


enum class TabIndex : unsigned {
    favorites,
    browser,
    recent,
    player,
    settings,
    about,

    last_active,

    num_tabs
};


TabIndex
to_tab_index(const std::string& str);


std::string
to_string(TabIndex idx);


std::string
to_ui_string(TabIndex idx);

#endif
