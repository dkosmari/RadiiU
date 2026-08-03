/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LOG_LEVEL_HPP
#define LOG_LEVEL_HPP

#include <format>
#include <string>
#include <string_view>
#include <iosfwd>


enum class LogLevel : unsigned {
    debug,
    info,
    warning,
    error,
    count
}; // enum class LogLevel


std::string
to_string(LogLevel level)
    noexcept;


std::ostream&
operator <<(std::ostream& out,
            LogLevel level);


// Formatter for LogLevel
template<>
struct std::formatter<LogLevel, char> : std::formatter<std::string_view, char> {
    template<typename Ctx>
    Ctx::iterator
    format(LogLevel level,
           Ctx& ctx)
        const
    {
        return formatter<string_view>::format(to_string(level), ctx);
    }
};

#endif
