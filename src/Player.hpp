/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <string>


struct Station;


namespace Player {

    void
    initialize();

    void
    finalize();


    void
    process_ui();


    void
    process_playback();


    void
    play();

    void
    play(const Station& station);


    void
    stop();

} // namespace Player

#endif
