/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRACE_MANAGER_HPP
#define TRACE_MANAGER_HPP

#include <optional>
#include <string_view>

#include <glaze/json/generic.hpp>


namespace TraceManager {

    void
    initialize(std::string_view proc_name);

    void
    finalize();


    bool
    duration_begin(std::string_view name,
                   std::optional<std::string_view> cat = {},
                   std::optional<glz::generic_u64> args = {});


    bool
    duration_end(std::string_view name = {},
                 std::optional<std::string_view> cat = {},
                 std::optional<glz::generic_u64> args = {});


    void
    instant(std::string_view name,
            std::optional<std::string_view> cat = {},
            std::optional<char> scope = {},
            std::optional<glz::generic_u64> args = {});


    void
    process_name(std::string_view proc_name);


    void
    thread_name(std::string_view thread_name);


    void
    vsync();

} // namespace TraceManager

#endif
