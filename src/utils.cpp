/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cinttypes>

#include <SDL2/SDL_platform.h>

#include "utils.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


namespace utils {

    const std::string&
    get_user_agent()
    {
        static const std::string user_agent = []
        {
            std::string result = PACKAGE_NAME "/" PACKAGE_VERSION " (";
            result += SDL_GetPlatform();
#ifdef __WUT__
            result += "; WUT";
#endif
            result += ")";
            return result;
        }();

        return user_agent;
    }


    const std::filesystem::path&
    get_content_path()
    {
        static const std::filesystem::path content_path =
#ifdef __WIIU__
            "/vol/content"
#else
            "assets/content"
#endif
            ;
        return content_path;
    }


    namespace detail {

        template<> const char* format_helper<char> = "c";

        template<> const char* format_helper<std::int8_t>  = PRIi8;
        template<> const char* format_helper<std::uint8_t> = PRIu8;

        template<> const char* format_helper<std::int16_t>  = PRIi16;
        template<> const char* format_helper<std::uint16_t> = PRIu16;

        template<> const char* format_helper<std::int32_t>  = PRIi32;
        template<> const char* format_helper<std::uint32_t> = PRIu32;

        template<> const char* format_helper<std::int64_t>  = PRIi64;
        template<> const char* format_helper<std::uint64_t> = PRIu64;

        template<> const char* format_helper<char *>       = "s";
        template<> const char* format_helper<const char *> = "s";

    } // namespace detail

} // namespace utils
