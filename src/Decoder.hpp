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


struct Decoder {

    struct Spec {
        SDL_AudioFormat format;
        unsigned rate;
        unsigned channels;
    };


    struct Info {
        std::string codec;
        std::string bitrate;
    };


    constexpr
    Decoder()
        noexcept = default;

    // disallow moving
    Decoder(Decoder&& other) = delete;

    Decoder&
    operator=(Decoder&& other) = delete;


    virtual
    ~Decoder()
        noexcept;

    virtual
    std::span<const std::byte>
    feed(std::span<const std::byte> data) = 0;

    virtual
    std::optional<std::span<const std::byte>>
    decode() = 0;

    virtual
    std::optional<Spec>
    get_spec()
        noexcept = 0;

    virtual
    Info
    get_info() = 0;

    virtual
    bool
    get_metadata(stream_metadata& meta)
        const = 0;


    static
    std::unique_ptr<Decoder>
    create(const std::string& content_type,
           std::span<const std::byte> data);

}; // struct Decoder

#endif
