/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DECODER_HPP
#define DECODER_HPP

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

#include <SDL_audio.h>

#include "stream_metadata.hpp"


namespace Decoder {

    struct Spec {
        SDL_AudioFormat format;
        int rate;
        int channels;
    };


    struct Info {
        std::string codec;
        std::string bitrate;
    };


    struct Base {

        constexpr
        Base()
            noexcept = default;

        // disallow moving
        Base(Base&& other) = delete;

        Base&
        operator=(Base&& other) = delete;


        virtual
        ~Base()
            noexcept;

        virtual
        std::span<const std::byte>
        feed(std::span<const std::byte> data) = 0;

        virtual
        std::span<const std::byte>
        decode() = 0;

        virtual
        std::optional<Spec>
        get_spec()
            noexcept = 0;

        virtual
        Info
        get_info() = 0;

        virtual
        std::optional<stream_metadata>
        get_metadata()
            const = 0;

    }; // struct Base


    std::unique_ptr<Base>
    create(const std::string& content_type,
           std::span<const std::byte> data);

} // namespace Decoder

#endif
