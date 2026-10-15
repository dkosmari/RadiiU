/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DEOCODER_MP3_HPP
#define DEOCODER_MP3_HPP

#include <string>

#include <mpg123xx/mpg123.hpp>

#include "Decoder.hpp"


struct DecoderMP3 : Decoder {

    DecoderMP3(std::span<const std::byte> data);

    ~DecoderMP3()
        noexcept override;

    std::span<const std::byte>
    feed(std::span<const std::byte> data)
        override;

    std::optional<std::span<const std::byte>>
    decode()
        override;

    std::optional<Spec>
    get_spec()
        noexcept override;

    Info
    get_info()
        override;

    std::optional<stream_metadata>
    get_metadata()
        const override;


    static
    bool
    match_mime(const std::string& content_type);


    static
    bool
    match_prefix(std::span<const std::byte> data);


private:

    mpg123::handle mpg;


}; // struct DecoderMP3


#endif
