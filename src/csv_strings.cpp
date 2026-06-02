/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "csv_strings.hpp"

#include "string_utils.hpp"


csv_strings::csv_strings(const std::optional<std::string>& joined)
{
    if (joined && !joined->empty())
        operator =(string_utils::split(*joined, ",", false));
}


csv_strings::operator std::optional<std::string>()
    const
{
    if (empty())
        return {};
    return string_utils::join(*this, ",", false);
}
