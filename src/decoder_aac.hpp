/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DECODER_AAC_HPP
#define DECODER_AAC_HPP

#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>

#include <neaacdec.h>

#include "byte_stream.hpp"
#include "decoder.hpp"


namespace decoder {

    struct aac : base {

        struct error : std::runtime_error {

            error(unsigned char code);

            error(const std::string& msg);

            error(const std::string& msg,
                  unsigned char code);

        }; // struct error


        NeAACDecHandle handle = nullptr;
        unsigned long rate = 0;
        unsigned char channels = 0;
        byte_stream stream;
        // std::vector<std::int16_t> samples;
        unsigned char current_channels = 0;
        unsigned long current_rate = 0;

        aac(std::span<const char> data);

        ~aac()
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


    private:


        static
        NeAACDecHandle
        open();

        void
        close()
            noexcept;

    }; // struct aac

} // namespace decoder

#endif
