/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ICON_MANAGER_HPP
#define ICON_MANAGER_HPP

#include <string>

#include <sdl2xx/renderer.hpp>
#include <sdl2xx/texture.hpp>
#include <sdl2xx/vec2.hpp>

namespace IconManager {

    void
    initialize(sdl::renderer& rend);

    void
    finalize();

    void
    process();

    const sdl::texture*
    get(const std::string& location,
        const sdl::vec2& size_limit = {0, 0});

} // namespace IconManager

#endif
