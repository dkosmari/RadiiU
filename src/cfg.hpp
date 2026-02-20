/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CFG_HPP
#define CFG_HPP

#include <cstddef>
#include <string>
#include <filesystem>

#include "TabID.hpp"


namespace cfg {

    extern std::filesystem::path base_dir;

    extern unsigned    browser_page_limit;
    extern bool        disable_apd;
    extern bool        disable_swkbd;
    extern bool        inactive_screen_off;
    extern TabID       initial_tab;
    extern unsigned    player_buffer_size;
    extern unsigned    player_history_limit;
    extern bool        remember_tab;
    extern unsigned    recent_limit;
    extern unsigned    screen_saver_timeout;
    extern bool        send_clicks;
    extern std::string server;


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
