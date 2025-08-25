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
    initialize(const std::string& user_agent);

    void
    finalize();


    void
    process_ui();


    void
    process_playback();


    void
    play();

    void
    play(const std::string& new_url);

    void
    play(const Station& station);


    void
    stop();

} // namespace Player

#endif
