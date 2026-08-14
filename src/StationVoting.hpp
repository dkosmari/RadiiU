/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STATION_VOTING_HPP
#define STATION_VOTING_HPP

#include <functional>

#include "Station.hpp"


namespace StationVoting {

    void
    process_logic();


    using VoteCallbackSignature = void();
    using VoteFunction = std::move_only_function<VoteCallbackSignature>;


    void
    Button(ConstStationPtr station);

} // namespace StationVoting

#endif
