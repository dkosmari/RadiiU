/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATION_DETAILS_POPUP_HPP
#define STATION_DETAILS_POPUP_HPP

#include <string>


namespace StationDetailsPopup {

    [[nodiscard]]
    bool
    show_button(const std::string& uuid);

    void
    open(const std::string& uuiid);

    void
    process_ui();

} // namespace StationDetailsPopup

#endif
