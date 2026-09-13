/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRACE_FUNCTION_HPP
#define TRACE_FUNCTION_HPP

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef ENABLE_TRACING

#include "TraceDuration.hpp"


struct TraceFunction : TraceDuration {

    TraceFunction(std::optional<std::string_view> cat_ = {},
                  std::optional<glz::generic_u64> args_ = {},
                  const std::source_location& location = std::source_location::current())
        noexcept;

}; // struct TraceFunction

#else

struct TraceFunction {

    template<typename... Args>
    constexpr
    TraceFunction(Args&&...)
        noexcept
    {}

}; // struct TraceFunction

#endif

#endif
