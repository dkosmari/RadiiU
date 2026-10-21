/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <string_view>
#include <utility>

#include "DecoderMP3.hpp"

#include "LogManager.hpp"
#include "mime_type.hpp"
#include "utils.hpp"


using namespace std::literals;


namespace {

    // Constants

    constexpr std::array mp3_mimes{
        "audio/mpeg"sv,
        "audio/mp3"sv,
    };


    constexpr std::array mp3_prefixes{
        "\xff\xfb"sv,
        "ID3"sv,
    };


    std::string
    to_string(mpg123_version v)
    {
        switch (v) {
            case MPG123_1_0:
                return "1.0"s;
            case MPG123_2_0:
                return "2.0"s;
            case MPG123_2_5:
                return "2.5"s;
            default:
                return "?"s;
        }
    }


    std::string
    rate_to_string(unsigned rate)
    {
        // Note: we know CafeStd has this suffix.
#define SUFFIX "㎐"
        switch (rate) {
            case 48000:
                return "48 k" SUFFIX;
            case 44100:
                return "44.1 k" SUFFIX;
            case 22050:
                return "22.05 k" SUFFIX;
            default:
                if ((rate % 1000) == 0)
                    return std::to_string(rate / 1000u) + " k" SUFFIX;
                else // unusual rate, just show it without the k
                    return std::to_string(rate) + " " SUFFIX;
        }
#undef SUFFIX
    }


    SDL_AudioFormat
    to_sdl_format(unsigned encoding)
    {
        switch (encoding) {

            case MPG123_ENC_SIGNED_16:
                return AUDIO_S16SYS;

            case MPG123_ENC_UNSIGNED_16:
                return AUDIO_U16SYS;

            case MPG123_ENC_SIGNED_32:
                return AUDIO_S32SYS;

            case MPG123_ENC_FLOAT_32:
                return AUDIO_F32SYS;

                // Note: 24-bit encoding doesn't have a SDL equivalent.
            default:
                return 0;

        }
    }


    std::string
    to_string(mpg123_mode mode)
    {
        switch (mode) {
            case MPG123_M_STEREO:
                return "stereo"s;
            case MPG123_M_JOINT:
                return "joint stereo"s;
            case MPG123_M_DUAL:
                return "dual channel"s;
            case MPG123_M_MONO:
                return "mono"s;
            default:
                return "unknown channel mode"s;
        }
    }


    std::string
    to_string(mpg123_vbr mode)
    {
        switch (mode) {
            case MPG123_CBR:
                return "constant"s;
            case MPG123_VBR:
                return "variable"s;
            case MPG123_ABR:
                return "average"s;
            default:
                return "unknown vbr mode"s;
        }
    }

} // namespace


DecoderMP3::DecoderMP3(std::span<const std::byte> data)
{
    mpg.set_verbose(true);
    mpg.open_feed();
    feed(data);
    mpg.get_format();

    LOG_INFO("Created MP3 decoder.");
}


DecoderMP3::~DecoderMP3()
    noexcept
{}


std::span<const std::byte>
DecoderMP3::feed(std::span<const std::byte> data)
{
    mpg.feed(data);
    return data;
}


std::optional<std::span<const std::byte>>
DecoderMP3::decode()
{
    auto samples = mpg.try_decode_frame();
    if (!samples)
        return {};
    return samples->as<std::byte>();
}


std::optional<Decoder::Spec>
DecoderMP3::get_spec()
    noexcept
{
    auto fmt = mpg.try_get_format();
    if (!fmt)
        return {};

    Spec result;
    result.rate = fmt->rate;
    result.channels = fmt->channels & MPG123_STEREO ? 2 : 1;
    result.format = to_sdl_format(fmt->encoding);
    if (!result.format) {
        // no exact match, ask mpg123 to convert it to S16
        result.format = AUDIO_S16SYS;
        mpg.set_format(fmt->rate, fmt->channels, MPG123_ENC_SIGNED_16);
    }

    return result;
}


Decoder::Info
DecoderMP3::get_info()
{
    Info result;
    if (auto info = mpg.try_get_frame_info()) {
        result.codec = "MPEG "s + to_string(info->version)
            + " layer "s
            + std::to_string(info->layer)
            + "; "s
            + rate_to_string(info->rate)
            + "; "
            + to_string(info->mode);
        result.bitrate = to_string(info->vbr_mode)
            + " "s
            + std::to_string(info->bitrate)
            + " kbps"s;
    }
    return result;
}


bool
DecoderMP3::get_metadata(stream_metadata& meta)
    const
{
    // These don't show up on streams, but do if playing a regular mp3 file.
    auto meta_flags = mpg.meta_check();
    if ((meta_flags & MPG123_NEW_ID3) == 0)
        return false;

    LOG_DEBUG("Got ID3 metadata.");
    auto id3 = mpg.get_id3();

    if (id3.v2) {

        auto& tag = *id3.v2;
        if (!tag.title.empty()) {
            LOG_DEBUG("Title: {:?}", tag.title);
            meta.title = std::move(tag.title);
        }
        if (!tag.artist.empty()) {
            LOG_DEBUG("Artist: {:?}", tag.artist);
            meta.artist = std::move(tag.artist);
        }
        if (!tag.album.empty()) {
            LOG_DEBUG("Album: {:?}", tag.album);
            meta.album = std::move(tag.album);
        }
        return true;

    } else if (id3.v1) {

        stream_metadata result;
        auto& tag = *id3.v1;
        if (!tag.title.empty()) {
            LOG_DEBUG("Title: {:?}", tag.title);
            meta.title = std::move(tag.title);
        }
        if (!tag.artist.empty()) {
            LOG_DEBUG("Artist: {:?}", tag.artist);
            meta.artist = std::move(tag.artist);
        }
        if (!tag.album.empty()) {
            LOG_DEBUG("Album: {:?}", tag.album);
            meta.album = std::move(tag.album);
        }
        return true;

    }

    return false;
}


bool
DecoderMP3::match_mime(const std::string& content_type)
{
    return mime_type::match_any(content_type, mp3_mimes);
}


bool
DecoderMP3::match_prefix(std::span<const std::byte> data)
{
    return utils::starts_with_any(data, mp3_prefixes);
}
