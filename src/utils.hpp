/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <filesystem>
#include <string>


namespace utils {

    const std::string&
    get_user_agent();


    const std::filesystem::path&
    get_content_path();

} // namespace utils

#endif
