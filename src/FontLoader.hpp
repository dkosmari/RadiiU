/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FONT_LOADER_HPP
#define FONT_LOADER_HPP

#include <filesystem>

namespace FontLoader {

    void
    initialize();

    void
    finalize();


    void
    load(const std::filesystem::path& filename,
         bool merge = true);

    void
    load_dir(const std::filesystem::path& dir);

} // namespace FontLoader

#endif
