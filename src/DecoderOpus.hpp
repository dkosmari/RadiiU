/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DECODER_OPUS_HPP
#define DECODER_OPUS_HPP

#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <opus/opusfile.h>

#include "byte_stream.hpp"
#include "Decoder.hpp"


struct DecoderOpus : Decoder {

    struct Error : std::runtime_error {
        Error(int code);
        Error(const std::string& msg,
              int code);
    };


    DecoderOpus(std::span<const std::byte> data);

    ~DecoderOpus()
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


    OggOpusFile* oof = nullptr;
    byte_stream input_stream;
    std::vector<opus_int16> output_buffer;
    opus_int32 bitrate = 0;


    static
    int
    read_callback(void* ctx,
                  unsigned char* buf,
                  int size);


}; // struct DecoderOpus

#endif
