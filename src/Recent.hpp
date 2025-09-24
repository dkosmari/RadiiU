/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RECENT_HPP
#define RECENT_HPP


struct Station;


namespace Recent {

    void
    initialize();

    void
    finalize();


    void
    process_ui();


    void
    add(const Station& station);

} // namespace Recent

#endif
