/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <SDL2/SDL_platform.h>

#include "utils.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


namespace utils {

    std::string
    get_user_agent()
    {
        std::string user_agent = PACKAGE_NAME "/" PACKAGE_VERSION " (";
        user_agent += SDL_GetPlatform();
#ifdef __WUT__
        user_agent += "; WUT";
#endif
        user_agent += ")";
        return user_agent;
    }


} // namespace utils
