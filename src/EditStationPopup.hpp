/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef EDIT_STATION_POPUP_HPP
#define EDIT_STATION_POPUP_HPP

#include <functional>

#include "Station.hpp"


namespace EditStationPopup {

    using ConfirmCallbackSignature = void(Station station);
    using ConfirmFunction = std::move_only_function<ConfirmCallbackSignature>;


    void
    open(ConfirmFunction func);

    void
    open(const Station& station,
         ConfirmFunction func);

    void
    process_ui();

} // namespace EditStationPopup

#endif
