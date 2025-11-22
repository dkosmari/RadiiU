/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ICY_STREAM_HPP
#define ICY_STREAM_HPP

#include <cstddef>
#include <span>

#include "byte_stream.hpp"
#include "http_client.hpp"
#include "stream_metadata.hpp"


namespace icy {

    struct stream {

        http_client& http;
        byte_stream meta_stream;
        byte_stream data_stream;

        std::size_t interval = 0;
        std::size_t data_left = 0;
        std::size_t meta_left = 0;

        stream_metadata initial_meta;
        stream_metadata current_meta;

        stream(http_client& hc);


        const stream_metadata&
        get_metadata()
            const noexcept;


        void
        process();

    private:

        void
        process_metadata();


    }; // struct stream

} // namespace icy

#endif
