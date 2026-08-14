/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <cstddef>
#include <string>
#include <filesystem>

#include "TabID.hpp"


namespace Settings {

    struct Cfg {
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
        bool        verbose_image_logs   = false;
        bool        verbose_rest_logs    = false;
        bool        verbose_stream_logs  = false;
    };


    extern Cfg cfg;


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

} // namespace Settings

#endif
