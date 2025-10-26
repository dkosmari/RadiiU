/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RECENT_HPP
#define RECENT_HPP

#include <memory>


struct Station;


namespace Recent {

    void
    initialize();

    void
    finalize();


    void
    process_ui();


    void
    process_logic();


    void
    add(std::shared_ptr<Station>& station);

} // namespace Recent

#endif
