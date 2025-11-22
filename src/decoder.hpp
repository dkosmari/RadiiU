/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DECODER_HPP
#define DECODER_HPP

#include <cstddef>
#include <unordered_map>
#include <memory>
#include <optional>
#include <iosfwd>
#include <span>
#include <string>

#include <SDL_audio.h>

#include "stream_metadata.hpp"


namespace decoder {

    struct spec {
        SDL_AudioFormat format;
        int rate;
        int channels;
    };


    struct info {
        std::string codec;
        std::string bitrate;
    };


    struct base {

        constexpr
        base()
            noexcept = default;

        // disallow moving
        base(base&& other) = delete;

        base&
        operator=(base&& other) = delete;


        virtual
        ~base()
            noexcept;

        virtual
        std::size_t
        feed(std::span<const char> data) = 0;

        virtual
        std::span<const char>
        decode() = 0;

        virtual
        std::optional<spec>
        get_spec()
            noexcept = 0;

        virtual
        info
        get_info() = 0;

        virtual
        std::optional<stream_metadata>
        get_metadata()
            const = 0;

    }; // struct base


    std::unique_ptr<base>
    create(const std::string& content_type,
           std::span<const char> data);

} // namespace decoder

#endif
