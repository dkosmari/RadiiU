/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Timer.hpp"

#include "LogManager.hpp"


Timer::Timer(const std::string& name_) :
    name{name_}
{}


void
Timer::report()
{
    LOG_DEBUG(
        "Timer {}: {}",
        name,
        duration_cast<std::chrono::microseconds>(elapsed())
    );
}
