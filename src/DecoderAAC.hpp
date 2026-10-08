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


namespace Decoder {

    struct AAC : Base {

        struct Error : std::runtime_error {

            Error(unsigned char code);

            Error(const std::string& msg);

            Error(const std::string& msg,
                  unsigned char code);

        }; // struct Error


        NeAACDecHandle handle = nullptr;
        unsigned long rate = 0;
        unsigned char channels = 0;
        byte_stream stream;
        // std::vector<std::int16_t> samples;
        unsigned char current_channels = 0;
        unsigned long current_rate = 0;


        AAC(std::span<const std::byte> data);

        ~AAC()
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
        NeAACDecHandle
        open();

        void
        close()
            noexcept;

    }; // struct AAC

} // namespace Decoder

#endif
