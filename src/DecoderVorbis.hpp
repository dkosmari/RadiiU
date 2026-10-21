/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DECODER_VORBIS_HPP
#define DECODER_VORBIS_HPP

#include <span>
#include <vector>
#include <stdexcept>
#include <string>

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#include "byte_stream.hpp"
#include "Decoder.hpp"


struct DecoderVorbis : Decoder {

    struct Error : std::runtime_error {
        Error(int code);
        Error(const std::string& msg,
              int code);
    };


    DecoderVorbis(std::span<const std::byte> input);

    ~DecoderVorbis()
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

    bool
    get_metadata(stream_metadata& meta)
        const override;


    static
    bool
    match_mime(const std::string& content_type);


    static
    bool
    match_prefix(std::span<const std::byte> input);


private:

    OggVorbis_File ovf;
    byte_stream input_stream;
    std::vector<char> output_buffer;
    long bitrate = 0;


    static
    std::size_t
    read_callback(void* buf,
                  std::size_t size,
                  std::size_t count,
                  void* ctx);

}; // struct DecoderVorbis

#endif
