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


Decoder::~Decoder()
    noexcept
{}


std::unique_ptr<Decoder>
Decoder::create(const std::string& content_type,
                std::span<const std::byte> data)
{
    if (DecoderAAC::match_mime(content_type)) {
        LOG_INFO("Creating aac decoder from content type: {}", content_type);
        return std::make_unique<DecoderAAC>(data);
    }

    if (DecoderMP3::match_mime(content_type)) {
        LOG_INFO("Creating MP3 decoder from content type: {}", content_type);
        return std::make_unique<DecoderMP3>(data);
    }

    if (DecoderOpus::match_mime(content_type)) {
        LOG_INFO("Creating Opus decoder from content type: {}", content_type);
        return std::make_unique<DecoderOpus>(data);
    }

    if (DecoderVorbis::match_mime(content_type)) {
        LOG_INFO("Creating Vorbis decoder from content type: {}", content_type);
        return std::make_unique<DecoderVorbis>(data);
    }

    LOG_INFO("No match for content type: {}", content_type);

    if (DecoderMP3::match_prefix(data)) {
        LOG_INFO("Creating MP3 decoder from data signature");
        return std::make_unique<DecoderMP3>(data);
    }

    if (DecoderOpus::match_prefix(data)) {
        try {
            LOG_INFO("Creating Opus decoder from data signature.");
            return std::make_unique<DecoderOpus>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Failed: {}", e.what());
        }
        catch (...) {
            LOG_INFO("Failed!");
        }
    }

    if (DecoderVorbis::match_prefix(data)) {
        try {
            LOG_INFO("Creating Vorbis decoder from data signature.");
            return std::make_unique<DecoderVorbis>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Failed: {}", e.what());
        }
        catch (...) {
            LOG_INFO("Failed!");
        }
    }

    if (DecoderAAC::match_prefix(data)) {
        LOG_INFO("Creating AAC decoder from data signature.");
        return std::make_unique<DecoderAAC>(data);
    }

    // Note: FLAC is  66 4C 61 43   "fLaC"

    throw std::runtime_error{"cannot create decoder"};
}
