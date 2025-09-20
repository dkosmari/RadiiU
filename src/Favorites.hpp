/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FAVORITES_HPP
#define FAVORITES_HPP

#include <string>


struct Station;

namespace Favorites {

    void
    initialize();


    void
    finalize();


    void
    process_ui();


    bool
    contains(const std::string& uuid);


    void
    add(const Station& st);


    void
    remove(const std::string& uuid);

} // namespace Favorites

#endif
