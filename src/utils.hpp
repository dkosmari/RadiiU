/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <concepts>
#include <filesystem>
#include <string>


namespace utils {

    [[nodiscard]]
    const std::string&
    get_user_agent();


    [[nodiscard]]
    const std::filesystem::path&
    get_content_path();



    template<typename T,
             std::floating_point F>
    [[nodiscard]]
    T
    lerp(const T& a,
         const T& b,
         F t)
    {
        return a + t * (b - a);
    }

} // namespace utils

#endif
