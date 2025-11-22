/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <utility>

#include "decoder_mp3.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace decoder {

    mp3::mp3(std::span<const char> data)
    {
        mpg.set_verbose(true);
        mpg.open_feed();
        feed(data);

        cout << "Created mp3 decoder." << endl;
    }


    mp3::~mp3()
        noexcept
    {}


    std::size_t
    mp3::feed(std::span<const char> data)
    {
        mpg.feed(data);
        return data.size_bytes();
    }


    std::span<const char>
    mp3::decode()
    {
        auto samples = mpg.try_decode_frame();
        if (!samples)
            return {};
        return samples->as<char>();
    }


    namespace {

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

    } // namespace


    std::optional<spec>
    mp3::get_spec()
        noexcept
    {
        auto fmt = mpg.try_get_format();
        if (!fmt)
            return {};

        spec result;
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

    namespace {

        std::string
        to_string(mpg123_version v)
        {
            switch (v) {
                case MPG123_1_0:
                    return "1.0";
                case MPG123_2_0:
                    return "2.0";
                case MPG123_2_5:
                    return "2.5";
                default:
                    return "?";
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


        std::string
        to_string(mpg123_mode mode)
        {
            switch (mode) {
                case MPG123_M_STEREO:
                    return "stereo";
                case MPG123_M_JOINT:
                    return "joint stereo";
                case MPG123_M_DUAL:
                    return "dual channel";
                case MPG123_M_MONO:
                    return "mono";
                default:
                    return "unknown channel mode";
            }
        }


        std::string
        to_string(mpg123_vbr mode)
        {
            switch (mode) {
                case MPG123_CBR:
                    return "constant";
                case MPG123_VBR:
                    return "variable";
                case MPG123_ABR:
                    return "average";
                default:
                    return "unknown vbr mode";
            }
        }

    } // namespace


    info
    mp3::get_info()
    {
        info result;
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


    std::optional<stream_metadata>
    mp3::get_metadata()
        const
    {
        // These don't show up on streams, but do if playing a regular mp3 file.
        auto meta_flags = mpg.meta_check();
        if ((meta_flags & MPG123_NEW_ID3) == 0)
            return {};

        cout << "Got ID3 metadata." << endl;
        auto id3 = mpg.get_id3();
        if (id3.v2) {
            stream_metadata result;
            auto& tag = *id3.v2;
            if (!tag.title.empty()) {
                cout << "Title: " << tag.title << endl;
                result.title = std::move(tag.title);
            }
            if (!tag.artist.empty()) {
                cout << "Artist: " << tag.artist << endl;
                result.artist = std::move(tag.artist);
            }
            if (!tag.album.empty()) {
                cout << "Album: " << tag.album << endl;
                result.album = std::move(tag.album);
            }
            return result;
        } else if (id3.v1) {
            stream_metadata result;
            auto& tag = *id3.v1;
            if (!tag.title.empty()) {
                cout << "Title: " << tag.title << endl;
                result.title = std::move(tag.title);
            }
            if (!tag.artist.empty()) {
                cout << "Artist: " << tag.artist << endl;
                result.artist = std::move(tag.artist);
            }
            if (!tag.album.empty()) {
                cout << "Album: " << tag.album << endl;
                result.album = std::move(tag.album);
            }
            return result;
        }
        return {};
    }

} // namespace decoder
