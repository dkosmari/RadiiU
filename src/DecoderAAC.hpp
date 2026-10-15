/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DECODER_AAC_HPP
#define DECODER_AAC_HPP

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <neaacdec.h>

#include "byte_stream.hpp"
#include "Decoder.hpp"


struct DecoderAAC : Decoder {

    struct Error : std::runtime_error {

        Error(unsigned char code);

        Error(const std::string& msg);

        Error(const std::string& msg,
              unsigned char code);

    }; // struct Error


    DecoderAAC(std::span<const std::byte> input);

    ~DecoderAAC()
        noexcept override;

    std::span<const std::byte>
    feed(std::span<const std::byte> input)
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
    match_prefix(std::span<const std::byte> input);


private:

    NeAACDecHandle handle = nullptr;
    unsigned long rate = 0;
    unsigned char channels = 0;
    byte_stream input_stream;
    std::vector<std::byte> input_buffer;
    std::size_t total_bytes = 0;
    double total_seconds = 0;
    std::string current_bitrate;
    unsigned char current_header = 0;
    unsigned char current_sbr = 0;


    static
    NeAACDecHandle
    open();

    void
    close()
        noexcept;

}; // struct DecoderAAC

#endif
