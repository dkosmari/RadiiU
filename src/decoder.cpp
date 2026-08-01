/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "decoder.hpp"

#include "decoder_aac.hpp"
#include "decoder_mp3.hpp"
#include "decoder_opus.hpp"
#include "decoder_vorbis.hpp"
#include "LogManager.hpp"


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
        if (content_type == "audio/aac" ||
            content_type == "audio/aacp" ||
            content_type == "audio/x-aac" ||
            content_type == "audio/audio/x-hx-aac-adts") {
            LOG_INFO("Creating aac decoder from content type: {}", content_type);
            return std::make_unique<aac>(data);
        }

        if (content_type == "audio/mpeg") {
            LOG_INFO("Creating MP3 decoder from content type: {}", content_type);
            return std::make_unique<mp3>(data);
        }

        if (content_type == "audio/vorbis") {
            LOG_INFO("Creating Opus decoder from content type: {}", content_type);
            return std::make_unique<opus>(data);
        }

        if (content_type == "audio/vorbis") {
            LOG_INFO("Creating Vorbis decoder from content type: {}", content_type);
            return std::make_unique<vorbis>(data);
        }

        LOG_INFO("No match for content type: {}", content_type);

        if (match(data, "\xff\xfb") || match(data, "ID3")) {
            LOG_INFO("Creating MP3 decoder from data signature");
            return std::make_unique<mp3>(data);
        }

        if (match(data, "OggS")) {
            try {
                LOG_INFO("Creating Opus decoder from data signature.");
                return std::make_unique<opus>(data);
            }
            catch (std::exception& e) {
                LOG_INFO("Failed: {}", e.what());
            }
            catch (...) {
                LOG_INFO("Failed!");
            }

            try {
                LOG_INFO("Creating Vorbis decoder from data signature.");
                return std::make_unique<vorbis>(data);
            }
            catch (std::exception& e) {
                LOG_INFO("Failed: {}", e.what());
            }
            catch (...) {
                LOG_INFO("Failed!");
            }
        }

        if (match(data, "\xff\xf1") ||
            match(data, "\xff\xf9")) {
            LOG_INFO("Creating AAC decoder from data signature.");
            return std::make_unique<aac>(data);
        }

        // Note: FLAC is  66 4C 61 43   "fLaC"

        throw std::runtime_error{"cannot create decoder"};
    }

} // namespace decoder
