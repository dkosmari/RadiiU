/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdexcept>
#include <utility>

#include "Resolution.hpp"


using namespace std::literals;


[[nodiscard]]
std::string
to_string(Resolution r)
{
    switch (r) {
        using enum Resolution;

        case hd:
            return "hd"s;

        case full_hd:
            return "full_hd"s;

        default:
            return "<UNKNOWN: "s + std::to_string(std::to_underlying(r)) + ">"s;
    }
}



[[nodiscard]]
std::string
to_label(Resolution r)
{
    switch (r) {
        using enum Resolution;

        case hd:
            return "HD (1280x720)"s;

        case full_hd:
            return "FullHD (1920x1080)"s;

        default:
            return "<UNKNOWN: "s + std::to_string(std::to_underlying(r)) + ">"s;
    }
}


sdl::vec2
to_size(Resolution r)
    noexcept
{
    switch (r) {
        using enum Resolution;

        case hd:
            return { 1280, 720 };

        case full_hd:
            return { 1920, 1080 };

        default:
            return { 1280, 720 };
    }
}
