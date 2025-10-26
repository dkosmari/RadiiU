/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BROWSER_HPP
#define BROWSER_HPP

#include <functional>
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
    refresh_mirrors();

    std::vector<std::string>
    get_mirrors();


    std::string
    get_server();


    void
    connect();


    void
    queue_refresh_stations();


    void
    process_logic();

    void
    process_ui();


    void
    send_click(const std::string& uuid,
               std::function<void()> on_success = {});

    void
    send_click(std::shared_ptr<Station>& station_ptr);

    void
    send_vote(const std::string& uuid,
              std::function<void()> on_success = {});

    void
    send_vote(std::shared_ptr<Station>& station_ptr);

    void
    refresh_station_async(std::shared_ptr<Station> station_ptr);

    const std::string*
    get_country_name(const std::string& code);

} // namespace Browser

#endif
