/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ICON_MANAGER_HPP
#define ICON_MANAGER_HPP

#include <string>

#include <sdl2xx/renderer.hpp>
#include <sdl2xx/texture.hpp>

namespace IconManager {

    void
    initialize(sdl::renderer& rend);

    void
    finalize();


    const sdl::texture*
    get(const std::string& location);

} // namespace IconManager

#endif
