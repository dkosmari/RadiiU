/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RESOLUTION_HPP
#define RESOLUTION_HPP

#include <string>

#include <sdl2xx/vec2.hpp>


enum class Resolution : unsigned {
    hd,
    full_hd,
    count
};


[[nodiscard]]
std::string
to_string(Resolution r);


[[nodiscard]]
std::string
to_label(Resolution r);



[[nodiscard]]
sdl::vec2
to_size(Resolution r)
    noexcept;

#endif
