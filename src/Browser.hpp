/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BROWSER_HPP
#define BROWSER_HPP

#include <string>
#include <vector>


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
    send_click(const std::string& uuid);

    void
    send_vote(const std::string& uuid);


    const std::string*
    get_country_name(const std::string& code);

} // namespace Browser

#endif
