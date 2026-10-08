/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <stdexcept>
#include <vector>

#include "Decoder.hpp"

#include "DecoderAAC.hpp"
#include "DecoderMP3.hpp"
#include "DecoderOpus.hpp"
#include "DecoderVorbis.hpp"
#include "LogManager.hpp"


using namespace std::literals;


namespace Decoder {

    Base::~Base()
        noexcept
    {}


    std::unique_ptr<Base>
    create(const std::string& content_type,
           std::span<const std::byte> data)
    {
        if (AAC::match_mime(content_type)) {
            LOG_INFO("Creating aac decoder from content type: {}", content_type);
            return std::make_unique<AAC>(data);
        }

        if (MP3::match_mime(content_type)) {
            LOG_INFO("Creating MP3 decoder from content type: {}", content_type);
            return std::make_unique<MP3>(data);
        }

        if (Opus::match_mime(content_type)) {
            LOG_INFO("Creating Opus decoder from content type: {}", content_type);
            return std::make_unique<Opus>(data);
        }

        if (Vorbis::match_mime(content_type)) {
            LOG_INFO("Creating Vorbis decoder from content type: {}", content_type);
            return std::make_unique<Vorbis>(data);
        }

        LOG_INFO("No match for content type: {}", content_type);

        if (MP3::match_prefix(data)) {
            LOG_INFO("Creating MP3 decoder from data signature");
            return std::make_unique<MP3>(data);
        }

        if (Opus::match_prefix(data)) {
            try {
                LOG_INFO("Creating Opus decoder from data signature.");
                return std::make_unique<Opus>(data);
            }
            catch (std::exception& e) {
                LOG_INFO("Failed: {}", e.what());
            }
            catch (...) {
                LOG_INFO("Failed!");
            }
        }

        if (Vorbis::match_prefix(data)) {
            try {
                LOG_INFO("Creating Vorbis decoder from data signature.");
                return std::make_unique<Vorbis>(data);
            }
            catch (std::exception& e) {
                LOG_INFO("Failed: {}", e.what());
            }
            catch (...) {
                LOG_INFO("Failed!");
            }
        }

        if (AAC::match_prefix(data)) {
            LOG_INFO("Creating AAC decoder from data signature.");
            return std::make_unique<AAC>(data);
        }

        // Note: FLAC is  66 4C 61 43   "fLaC"

        throw std::runtime_error{"cannot create decoder"};
    }

} // namespace Decoder
