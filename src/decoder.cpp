/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "decoder.hpp"

#include "decoder_mp3.hpp"
#include "decoder_vorbis.hpp"


using std::cout;
using std::endl;


namespace decoder {

    base::~base()
        noexcept
    {}


    namespace {

        bool
        match(std::span<const char> data,
              std::span<const char> prefix)
        {
            if (data.size() < prefix.size())
                return false;

            return std::equal(prefix.begin(),
                              prefix.end(),
                              data.begin());
        }

    } // namespace


    std::unique_ptr<base>
    create(const std::string& content_type,
           std::span<const char> data)
    {
        if (content_type == "audio/mpeg") {
            cout << "Creating mp3 decoder from content type: " << content_type << endl;
            return std::make_unique<mp3>(data);
        }

        if (content_type == "audio/vorbis") {
            cout << "Creating vorbis decoder from content type: " << content_type << endl;
            return std::make_unique<vorbis>(data);
        }

        if (match(data, "\xff\xfb") || match(data, "ID3")) {
            cout << "Creating mp3 decoder from data signature" << endl;
            return std::make_unique<mp3>(data);
        }

        if (match(data, "OggS")) {
            cout << "Creating vorbis decoder from data signature" << endl;
            return std::make_unique<vorbis>(data);
        }

        // Note: FLAC is  66 4C 61 43   "fLaC"
        // Note: AAC is FF F1 or FF F9

        throw std::runtime_error{"cannot create decoder"};
    }

} // namespace decoder
