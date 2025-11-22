/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <bit>
#include <cerrno>
#include <iostream>
#include <string_view>

#include <opus/opus_defines.h>

#include "decoder_opus.hpp"

#include "utils.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace decoder {

    namespace {

        std::string
        opus_error_to_string(int code)
        {
            return opus_strerror(code);
        }

    } // namespace


    opus::error::error(int code) :
        std::runtime_error{opus_error_to_string(code)}
    {}


    opus::error::error(const std::string& msg,
                       int code) :
        std::runtime_error{msg + ": " + opus_error_to_string(code)}
    {}


    opus::opus(std::span<const char> data) :
        samples(8192 * 2)
    {
        OpusFileCallbacks callbacks {
            .read = &read_callback,
            .seek = nullptr,
            .tell = nullptr,
            .close = nullptr,
        };

        int e;
        oof = op_open_callbacks(this,
                                &callbacks,
                                reinterpret_cast<const unsigned char*>(data.data()),
                                data.size_bytes(),
                                &e);
        if (!oof)
            throw error{"op_open_callbacks() failed", e};

        cout << "Created opus decoder." << endl;
    }


    opus::~opus()
        noexcept
    {
        op_free(oof);
    }


    std::size_t
    opus::feed(std::span<const char> data)
    {
        return stream.write(data);
    }


    std::span<const char>
    opus::decode()
    {
        int r = op_read_stereo(oof,
                               samples.data(),
                               samples.size());

        if (r <= 0) {
            switch (r) {
                case 0:
                    return {};
                case OP_HOLE:
                case OP_EREAD:
                    cout << "Harmless (?) Opus error: " << opus_error_to_string(r) << endl;
                    return {};
                default:
                    throw error{"op_read_stereo() failed", r};
            }
        }
        auto decoded = std::span(samples.data(), r * 2);
        return std::span{reinterpret_cast<const char*>(decoded.data()),
                         decoded.size_bytes()};
    }


    std::optional<spec>
    opus::get_spec()
        noexcept
    {
        spec result;
        result.format = AUDIO_S16SYS;
        result.rate = 48000;
        //result.channels = op_channel_count(oof, -1);
        result.channels = 2;
        return result;
    }


    info
    opus::get_info()
    {
        info result;

        result.codec = "Ogg Opus";

        if (auto tags = op_tags(oof, -1)) {
            if (tags->vendor)
                result.codec += "; "s + tags->vendor;
        }

        opus_int32 br = op_bitrate_instant(oof);
        if (br > 0)
            bitrate = br;

        if (bitrate)
            result.bitrate = utils::cpp_sprintf("%.1f Kbps", bitrate / 1000.0f);

        return result;
    }


    namespace {

        std::vector<std::string_view>
        to_vector(const OpusTags* tag)
        {
            std::vector<std::string_view> result;
            if (tag && tag->comments > 0) {
                result.resize(tag->comments);
                for (int i = 0; i <tag->comments; ++i) {
                    result[i] = std::string_view(tag->user_comments[i],
                                                 tag->comment_lengths[i]);
                }
            }
            return result;
        }

    } // namespace


    std::optional<stream_metadata>
    opus::get_metadata()
        const
    {
        auto tags = op_tags(oof, -1);
        if (!tags)
            return {};

        stream_metadata result;

        auto tags_vec = to_vector(tags);
        for (auto entry : tags_vec) {
            auto tokens = utils::split(entry, "="sv, false, 2);
            if (tokens.size() != 2)
                continue;
            auto key = tokens[0];
            auto val = tokens[1];

            using utils::equal_case;
            if (equal_case(key, "TITLE"sv))
                result.title = val;
            else if (equal_case(key, "ARTIST"sv))
                result.artist = val;
            else if (equal_case(key, "ALBUM"sv))
                result.album = val;
            else if (equal_case(key, "GENRE"sv))
                result.genre = val;
            else
                result.extra[std::string{key}] = std::string{val};
        }

        return result;
    }


    int
    opus::read_callback(void* ctx,
                        unsigned char* buf,
                        int size)
    {
        auto self = reinterpret_cast<opus*>(ctx);
        if (!self) {
            errno = EINVAL;
            return -1;
        }
        if (self->stream.empty()) {
            errno = EAGAIN;
            return 0;
        }

        return self->stream.read(buf, size);
    }

} // namespace decoder
