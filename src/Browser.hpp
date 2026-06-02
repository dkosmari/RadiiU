/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BROWSER_HPP
#define BROWSER_HPP

#include <memory>
#include <string>
#include <vector>


struct Station;

namespace Browser {

    void
    initialize();

    void
    finalize();


    void
    process_ui();

    void
    send_click(std::shared_ptr<Station>& station_ptr);

    void
    send_vote(std::shared_ptr<Station>& station_ptr);

    void
    update_station(std::shared_ptr<Station> station_ptr);

    void
    search_stations();

    std::string
    get_country_name(const std::string& code);

} // namespace Browser

#endif
