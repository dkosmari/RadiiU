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


namespace cfg {

    extern std::filesystem::path base_dir;

    extern std::string radio_info_server;
    extern unsigned    player_buffer_size;
    extern bool        disable_auto_power_down;
    extern unsigned    browser_page_size;


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
