/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ICY_STREAM_HPP
#define ICY_STREAM_HPP

#include <cstddef>
#include <span>

#include <curlxx/url.hpp>

#include "byte_stream.hpp"
#include "stream_metadata.hpp"


namespace icy {

    class stream {

    public:

        stream();


        // NOTE: pass-by-value is intentional
        void
        reset(std::string base_url_,
              std::size_t new_interval = 0);


        void
        feed(std::span<const std::byte> input);


        const stream_metadata&
        get_metadata()
            const noexcept;


        std::span<std::byte>
        get_data(std::span<std::byte> dst);


    private:

        std::string base_url;

        byte_stream meta_stream;
        byte_stream data_stream;

        std::size_t interval = 0;
        std::size_t data_left = 0;
        std::size_t meta_left = 0;

        stream_metadata current_meta;


        void
        parse_metadata();

        std::string
        resolve_url(std::string_view url)
            noexcept;

    }; // class stream

} // namespace icy

#endif
