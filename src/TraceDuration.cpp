/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <utility>

#include "TraceDuration.hpp"

#include "TraceManager.hpp"


using namespace std::literals;

#ifdef ENABLE_TRACING

TraceDuration::TraceDuration(std::string_view name_,
                             std::optional<std::string_view> cat_,
                             std::optional<glz::generic_u64> args,
                             const std::source_location& location)
    noexcept :
    name{std::move(name_)},
    cat{std::move(cat_)}
{
    if (!args)
        args.emplace();
    (*args)["function"sv] = location.function_name();
    (*args)["file"sv] = location.file_name();
    (*args)["line"sv] = location.line();
    success = TraceManager::duration_begin(name, cat, std::move(args));
}


TraceDuration::~TraceDuration()
    noexcept
{
    if (success)
        TraceManager::duration_end(name, cat);
}

#endif // ENABLE_TRACING
