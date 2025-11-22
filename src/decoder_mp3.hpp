/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DEOCODER_MP3_HPP
#define DEOCODER_MP3_HPP

#include <mpg123xx/mpg123.hpp>

#include "decoder.hpp"


namespace decoder {

    struct mp3 : base {

        mpg123::handle mpg;


        mp3(std::span<const char> data);

        ~mp3()
            noexcept override;

        std::size_t
        feed(std::span<const char> data)
            override;

        std::span<const char>
        decode()
            override;

        std::optional<spec>
        get_spec()
            noexcept override;

        info
        get_info()
            override;

        std::optional<stream_metadata>
        get_metadata()
            const override;

    }; // struct mp3

} // namespace decoder

#endif
