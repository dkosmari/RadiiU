/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CFG_HPP
#define CFG_HPP

#include <cstddef>
#include <string>
#include <filesystem>

#include "TabID.hpp"


namespace cfg {

    struct State {
        unsigned    browser_page_limit   = 20;
        bool        disable_apd          = true;
        bool        disable_swkbd        = false;
        bool        inactive_screen_off  = false;
        TabID       initial_tab          = TabID::browser;
        unsigned    player_buffer_size   = 8;
        unsigned    player_history_limit = 20;
        bool        remember_tab         = true;
        unsigned    recent_limit         = 10;
        unsigned    screen_saver_timeout = 120;
        bool        send_clicks          = false;
        std::string server               = {};
        std::string style                = {};
        bool        switch_to_player     = false;
    };

    extern State state;


    void
    load_defaults();


    void
    initialize();


    void
    finalize();


    void
    load();


    void
    save();

} // namespace cfg

#endif
