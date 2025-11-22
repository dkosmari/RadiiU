/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
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
#include "decoder.hpp"


namespace decoder {

    struct opus : base {

        struct error : std::runtime_error {
            error(int code);
            error(const std::string& msg,
                  int code);
        };


        OggOpusFile* oof = nullptr;
        byte_stream stream;
        std::vector<opus_int16> samples;
        opus_int32 bitrate = 0;






        opus(std::span<const char> data);

        ~opus()
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


        static
        int
        read_callback(void* ctx,
                      unsigned char* buf,
                      int size);


    }; // struct opus

} // namespace decoder

#endif
