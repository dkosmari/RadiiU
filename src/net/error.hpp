/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NET_ERROR_HPP
#define NET_ERROR_HPP

#include <string>
#include <system_error>


namespace net {

    struct error : std::system_error {

        error(int code, const std::string& msg);

        error(int code);

    };

} // namespace net

#endif
