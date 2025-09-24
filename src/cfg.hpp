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

#include "TabIndex.hpp"


namespace cfg {

    extern std::filesystem::path base_dir;

    extern std::string server;
    extern unsigned    player_buffer_size;
    extern bool        disable_auto_power_down;
    extern unsigned    browser_page_size;
    extern TabIndex    start_tab;
    extern bool        remember_last_tab;
    extern unsigned    max_recent;


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
