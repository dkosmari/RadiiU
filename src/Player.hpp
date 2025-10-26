/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <memory>
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
    process_logic();


    void
    play();

    void
    play(std::shared_ptr<Station>& st);


    void
    stop();


    bool
    is_playing(const Station& st);

    bool
    is_playing(std::shared_ptr<Station>& st);

} // namespace Player

#endif
