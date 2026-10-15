/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRACE_FUNCTION_HPP
#define TRACE_FUNCTION_HPP

#include <optional>
#include <source_location>
#include <string_view>

#include <glaze/json/generic.hpp>

#include "TraceDuration.hpp"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef ENABLE_TRACING

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
