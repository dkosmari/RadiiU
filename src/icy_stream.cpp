/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <stdexcept>

#include "icy_stream.hpp"

#include "utils.hpp"
#include "icy.hpp"


using std::cout;
using std::endl;


namespace icy {

    stream::stream(http_client& hc) :
        http(hc)
    {
        unsigned icy_num = 0;

        if (auto hdr = http.get_header("icy-metaint")) {
            cout << "Got icy-metaint: " << *hdr << endl;
            data_left = interval = std::stoull(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-name")) {
            initial_meta.station_name = utils::trimmed(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-url")) {
            initial_meta.station_url = utils::trimmed(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-genre")) {
            initial_meta.station_genre = utils::trimmed(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-description")) {
            initial_meta.station_description = utils::trimmed(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-br"))
            ++icy_num;

        if (auto hdr = http.get_header("ice-audio-info"))
            ++icy_num;

        if (auto hdr = http.get_header("icy-pub"))
            ++icy_num;

        if (!icy_num)
            throw std::runtime_error{"not an icecast stream"};

        current_meta = initial_meta;
    }


    const stream_metadata&
    stream::get_metadata()
        const noexcept
    {
        return current_meta;
    }


    void
    stream::process()
    {
        if (!interval) {
            data_stream.consume(http.data_stream);
            return;
        }

        // try to convert the whole raw_stream into either data_stream or meta_stream
        while (!http.data_stream.empty()) {

            if (data_left)
                data_left -= data_stream.consume(http.data_stream, data_left);
            else {
                // no more data, start reading metadata

                if (meta_left == 0) {
                    // when both data_left and meta_left are zero, we're waiting for the
                    // meta size prefix
                    auto c = http.data_stream.try_load_u8();
                    if (!c) // if not enough raw data to parse size of meta data, give up
                        return;
                    meta_left = *c * 16u;
                    if (meta_left == 0) { // no metadata for now
                        data_left = interval;
                        return;
                    }
                }

                meta_left -= meta_stream.consume(http.data_stream, meta_left);
                if (meta_left == 0) {
                    // finished reading this chunk of metadata
                    data_left = interval;
                    process_metadata();
                }
            }

        }
    }


    void
    stream::process_metadata()
    {
        current_meta = initial_meta;

        // Note: icy metadata is padded with null bytes.
        std::string meta_str = utils::trimmed(meta_stream.read_str(), '\0');

        auto meta = icy::parse(meta_str);

        cout << "Icy Metadata:\n";
        for (auto [key, val] : meta)
            cout << "    " << key << "=\"" << val << "\"\n";
        cout << endl;

        for (const auto& [k, v] : meta) {
            // TODO: check if there are more special keys
            if (k == "StreamTitle")
                current_meta.title = utils::trimmed(v);
            else
                current_meta.extra[k] = utils::trimmed(v);
        }

    }

} // namespace icy
