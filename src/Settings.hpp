/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>

#include "Resolution.hpp"
#include "TabID.hpp"


namespace Settings {

    struct Cfg {

        unsigned    browser_page_limit   = 20;
        bool        disable_apd          = true;
        bool        ignore_ssl_errors    = false;
        bool        inactive_screen_off  = false;
        TabID       initial_tab          = TabID::browser;
        std::chrono::milliseconds player_buffer_duration = std::chrono::seconds{5};
        unsigned    player_history_limit = 20;
        bool        remember_tab         = true;
        unsigned    recent_limit         = 10;
        Resolution  resolution           = Resolution::hd;
        unsigned    screen_saver_timeout = 120;
        bool        send_clicks          = false;
        std::string server               = {};
        std::string style                = {};
        bool        switch_to_player     = false;
        bool        use_swkbd            = true;
        bool        verbose_image_logs   = false;
        bool        verbose_rest_logs    = false;
        bool        verbose_stream_logs  = false;

        constexpr
        bool
        operator ==(const Cfg& other)
            const noexcept = default;

    }; // struct Cfg


    extern Cfg cfg;


    void
    initialize();


    void
    finalize();


    void
    reset()
        noexcept;

} // namespace Settings

#endif
