/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "error.hpp"


namespace net {

    error::error(int code, const std::string& msg) :
        std::system_error{std::make_error_code(std::errc{code}), msg}
    {}


    error::error(int code) :
        std::system_error{std::make_error_code(std::errc{code})}
    {}

} // namespace std
