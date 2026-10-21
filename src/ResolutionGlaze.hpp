/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RESOLUTION_GLAZE_HPP
#define RESOLUTION_GLAZE_HPP

#include <glaze/core/meta.hpp>

#include "Resolution.hpp"


template<>
struct glz::meta<Settings::Resolution> {
    using enum TabID;
    static constexpr
    auto value = enumerate(
        hd,
        full_hd,
        count
    );
};


#endif
