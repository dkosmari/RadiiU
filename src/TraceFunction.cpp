/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <utility>

#include "TraceFunction.hpp"


TraceFunction::TraceFunction(std::optional<std::string_view> cat_,
                             std::optional<glz::generic_u64> args,
                             const std::source_location& location)
    noexcept :
    TraceDuration{
        location.function_name(),
        std::move(cat_),
        std::move(args),
        location
    }
{}
