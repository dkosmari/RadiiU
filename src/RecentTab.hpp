/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RECENT_TAB_HPP
#define RECENT_TAB_HPP

#include <memory>


struct Station;


namespace RecentTab {

    void
    initialize();

    void
    finalize();


    void
    process_ui();


    void
    process_logic();


    void
    queue_add(std::shared_ptr<Station>& station);

} // namespace RecentTab

#endif
