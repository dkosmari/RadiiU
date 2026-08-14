/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BROWSER_TAB_HPP
#define BROWSER_TAB_HPP

#include <memory>
#include <string>
#include <vector>

#include "Station.hpp"

namespace BrowserTab {

    void
    initialize();

    void
    finalize();


    void
    process_ui();

    void
    send_click(StationPtr& station);

    void
    update_station(StationPtr station);

    void
    search_stations();

    std::string
    get_country_name(const std::string& code);

} // namespace BrowserTab

#endif
