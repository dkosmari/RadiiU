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
#include <type_traits>


namespace utils {

    const std::string&
    get_user_agent();


    const std::filesystem::path&
    get_content_path();


    namespace detail {

        template<typename T>
        extern const char* format_helper;

    } // detail


    template<typename T>
    std::string
    format(T&)
    {
        return detail::format_helper<std::remove_cv_t<T>>;
    }

} // namespace utils

#endif
