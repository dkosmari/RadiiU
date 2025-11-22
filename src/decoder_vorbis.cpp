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

#include "decoder_vorbis.hpp"

#include "utils.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace decoder {

    namespace {

        std::string
        vorbis_error_to_string(int code)
        {
            switch (code) {
                case OV_FALSE:
                    return "OV_FALSE";
                case OV_EOF:
                    return "OV_EOF";
                case OV_HOLE:
                    return "OV_HOLE";
                case OV_EREAD:
                    return "OV_EREAD";
                case OV_EFAULT:
                    return "OV_EFAULT";
                case OV_EIMPL:
                    return "OV_EIMPL";
                case OV_EINVAL:
                    return "OV_EINVAL";
                case OV_ENOTVORBIS:
                    return "OV_ENOTVORBIS";
                case OV_EBADHEADER:
                    return "OV_EBADHEADER";
                case OV_EVERSION:
                    return "OV_EVERSION";
                case OV_ENOTAUDIO:
                    return "OV_ENOTAUDIO";
                case OV_EBADPACKET:
                    return "OV_EBADPACKET";
                case OV_EBADLINK:
                    return "OV_EBADLINK";
                case OV_ENOSEEK:
                    return "OV_ENOSEEK";
                default:
                    return "unknown Vorbis error " + std::to_string(code);
            }
        }

    } // namespace


    vorbis::error::error(int code) :
        std::runtime_error{vorbis_error_to_string(code)}
    {}


    vorbis::error::error(const std::string& msg,
                         int code) :
        std::runtime_error{msg + ": " + vorbis_error_to_string(code)}
    {}


    vorbis::vorbis(std::span<const char> data) :
        samples(8192)
    {
        ov_callbacks callbacks {
            .read_func = &read_callback,
            .seek_func = nullptr,
            .close_func = nullptr,
            .tell_func = nullptr,
        };

        int e = ov_open_callbacks(this,
                                  &ovf,
                                  data.data(),
                                  data.size_bytes(),
                                  callbacks);
        if (e)
            throw error{"ov_open_callbacks() failed", e};

        cout << "Created vorbis decoder." << endl;
    }


    vorbis::~vorbis()
        noexcept
    {
        ov_clear(&ovf);
    }


    std::size_t
    vorbis::feed(std::span<const char> data)
    {
        return stream.write(data);
    }


    std::span<const char>
    vorbis::decode()
    {
        // TODO: when more than 2 channels, we should reorder them to match SDL channels.
        int bitstream;
        long r = ov_read(&ovf,
                         samples.data(),
                         samples.size(),
                         std::endian::native == std::endian::big,
                         2,
                         1,
                         &bitstream);
        if (r == 0)
            return {};
        if (r < 0) {
            cout << "vorbis::decode(): " << vorbis_error_to_string(r) << endl;
            return {};
        }
        return std::span(samples.data(), r);
    }


    std::optional<spec>
    vorbis::get_spec()
        noexcept
    {
        auto vinfo = ov_info(&ovf, -1);
        if (!vinfo)
            return {};
        spec result;
        result.format = AUDIO_S16SYS;
        result.rate = vinfo->rate;
        result.channels = vinfo->channels;
        return result;
    }


    info
    vorbis::get_info()
    {
        info result;

        result.codec = "Ogg Vorbis"s;

        if (auto comment = ov_comment(&ovf, -1)) {
            if (comment->vendor)
                result.codec += "; "s + comment->vendor;
        }

        long current_bitrate = ov_bitrate_instant(&ovf);
        if (current_bitrate > 0)
            bitrate = current_bitrate;

        if (bitrate)
            result.bitrate = utils::cpp_sprintf("%.1f Kbps", bitrate / 1000.0f);

        return result;
    }


    namespace {

        std::vector<std::string_view>
        to_vector(const vorbis_comment* vc)
        {
            std::vector<std::string_view> result;
            if (vc && vc->comments > 0) {
                result.resize(vc->comments);
                for (int i = 0; i < vc->comments; ++i) {
                    result[i] = std::string_view(vc->user_comments[i],
                                                 vc->comment_lengths[i]);
                }
            }
            return result;
        }

    } // namespace


    std::optional<stream_metadata>
    vorbis::get_metadata()
        const
    {
        auto comment = ov_comment(const_cast<OggVorbis_File*>(&ovf), -1);
        if (!comment)
            return {};

        stream_metadata result;

        auto comment_vec = to_vector(comment);
        for (auto entry : comment_vec) {
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


    std::size_t
    vorbis::read_callback(void* buf,
                          std::size_t size,
                          std::size_t count,
                          void* ctx)
    {
        auto self = reinterpret_cast<vorbis*>(ctx);
        if (!self) {
            errno = EINVAL;
            return 0;
        }
        if (self->stream.empty()) {
            errno = EAGAIN;
            return 0;
        }

        return self->stream.read(buf, size * count);
    }

} // namespace decoder
