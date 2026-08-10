/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CONFIRM_DELETE_STATION_POPUP_HPP
#define CONFIRM_DELETE_STATION_POPUP_HPP

#include <functional>

#include "Station.hpp"


namespace ConfirmDeleteStationPopup {

    using ConfirmCallbackSignature = void();
    using ConfirmFunction = std::move_only_function<ConfirmCallbackSignature>;


    void
    open(ConstStationPtr station,
         ConfirmFunction func);

    void
    process_ui();

} // namespace ConfirmDeleteStationPopup

#endif
