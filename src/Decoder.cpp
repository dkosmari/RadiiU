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
        try {
            LOG_INFO("Creating AAC decoder from content type: {}", content_type);
            return std::make_unique<DecoderAAC>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Cannot create AAC decoder: {}", e.what());
        }
    }

    if (DecoderMP3::match_mime(content_type)) {
        try {
            LOG_INFO("Creating MP3 decoder from content type: {}", content_type);
            return std::make_unique<DecoderMP3>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Cannot create MP3 decoder: {}", e.what());
        }
    }

    if (DecoderOpus::match_mime(content_type)) {
        try {
            LOG_INFO("Creating Opus decoder from content type: {}", content_type);
            return std::make_unique<DecoderOpus>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Cannot create Opus decoder: {}", e.what());
        }
    }

    if (DecoderVorbis::match_mime(content_type)) {
        try {
            LOG_INFO("Creating Vorbis decoder from content type: {}", content_type);
            return std::make_unique<DecoderVorbis>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Cannot create Vorbis decoder: {}", e.what());
        }
    }

    LOG_INFO("No match for content type {:?}, attempting to match data signature.",
             content_type);

    if (DecoderAAC::match_prefix(data)) {
        try {
            LOG_INFO("Creating AAC decoder from data signature.");
            return std::make_unique<DecoderAAC>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Cannot create AAC decoder: {}", e.what());
        }
    }

    if (DecoderMP3::match_prefix(data)) {
        try {
            LOG_INFO("Creating MP3 decoder from data signature");
            return std::make_unique<DecoderMP3>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Cannot create MP3 decoder: {}", e.what());
        }
    }

    if (DecoderOpus::match_prefix(data)) {
        try {
            LOG_INFO("Creating Opus decoder from data signature.");
            return std::make_unique<DecoderOpus>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Cannot create Opus decoder: {}", e.what());
        }
    }

    if (DecoderVorbis::match_prefix(data)) {
        try {
            LOG_INFO("Creating Vorbis decoder from data signature.");
            return std::make_unique<DecoderVorbis>(data);
        }
        catch (std::exception& e) {
            LOG_INFO("Cannot create Vorbis decoder: {}", e.what());
        }
    }

    // Note: FLAC is  66 4C 61 43   "fLaC"

    throw std::runtime_error{"cannot create decoder"};
}
