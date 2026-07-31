/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYER_TAB_HPP
#define PLAYER_TAB_HPP

#include <memory>
#include <string>

#include "Station.hpp"


namespace PlayerTab {

    void
    initialize();

    void
    finalize();


    void
    process_ui();


    void
    process_logic();


    void
    play();

    void
    play(StationPtr& st);


    void
    stop();


    bool
    is_playing(const Station& st);

} // namespace PlayerTab

#endif
