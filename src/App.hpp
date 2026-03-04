/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APP_HPP
#define APP_HPP

#include <filesystem>
#include <string>


namespace App {

    [[nodiscard]]
    const std::string&
    get_user_agent();


    [[nodiscard]]
    const std::filesystem::path&
    get_content_path();


    void
    initialize();

    void
    finalize();


    void
    run();

} // namespace App

#endif
