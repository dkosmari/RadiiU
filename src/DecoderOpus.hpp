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


namespace Decoder {

    struct Opus : Base {

        struct Error : std::runtime_error {
            Error(int code);
            Error(const std::string& msg,
                  int code);
        };


        OggOpusFile* oof = nullptr;
        byte_stream stream;
        std::vector<opus_int16> samples;
        opus_int32 bitrate = 0;


        Opus(std::span<const std::byte> data);

        ~Opus()
            noexcept override;


        std::span<const std::byte>
        feed(std::span<const std::byte> data)
            override;

        std::span<const std::byte>
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

        static
        int
        read_callback(void* ctx,
                      unsigned char* buf,
                      int size);


    }; // struct Opus

} // namespace Decoder

#endif
