/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ostream>
#include <type_traits>

#include "LogLevel.hpp"


using namespace std::literals;


std::string
to_string(LogLevel level)
    noexcept
{
    switch (level) {
        using enum LogLevel;

        case debug:
            return "DEBUG"s;

        case info:
            return "INFO"s;

        case warning:
            return "WARN"s;

        case error:
            return "ERROR"s;

        case count:
        default:
            return "<UNKNOWN>"s;
    }
}


std::ostream&
operator <<(std::ostream& out,
            LogLevel level)
{
    return out << to_string(level);
}
