/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DECODER_VORBIS_HPP
#define DECODER_VORBIS_HPP

#include <vector>

#include <vorbis/vorbisfile.h>

#include "byte_stream.hpp"
#include "decoder.hpp"
#include "stream_metadata.hpp"


namespace decoder {

    struct vorbis : base {

        OggVorbis_File ovf;
        byte_stream stream;
        std::vector<char> samples;
        long bitrate = 0;


        vorbis(std::span<const char> data);

        ~vorbis()
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
        std::size_t
        read_callback(void* buf,
                      std::size_t size,
                      std::size_t count,
                      void* ctx);

    }; // struct vorbis

} // namespace decoder

#endif
