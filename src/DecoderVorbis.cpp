/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <bit>
#include <cerrno>
#include <format>
#include <string_view>

#include "DecoderVorbis.hpp"

#include "LogManager.hpp"
#include "mime_type.hpp"
#include "string_utils.hpp"
#include "utils.hpp"


using namespace std::literals;


namespace {

    // Constants

    constexpr std::array vorbis_mimes{
        "audio/vorbis"sv,
        "audio/ogg; codecs=vorbis"sv,
        "audio/ogg"sv,
    };


    constexpr std::array vorbis_prefixes{
        "OggS"sv,
    };


    // Function definitions

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


DecoderVorbis::Error::Error(int code) :
    std::runtime_error{vorbis_error_to_string(code)}
{}


DecoderVorbis::Error::Error(const std::string& msg,
                            int code) :
    std::runtime_error{msg + ": " + vorbis_error_to_string(code)}
{}


DecoderVorbis::DecoderVorbis(std::span<const std::byte> input) :
    output_buffer(2 * 8192)
{
    ov_callbacks callbacks {
        .read_func = &read_callback,
        .seek_func = nullptr,
        .close_func = nullptr,
        .tell_func = nullptr,
    };

    int e = ov_open_callbacks(
        this,
        &ovf,
        reinterpret_cast<const char*>(input.data()),
        input.size_bytes(),
        callbacks
    );
    if (e)
        throw Error{"ov_open_callbacks() failed", e};

    LOG_INFO("Created vorbis decoder.");
}


DecoderVorbis::~DecoderVorbis()
    noexcept
{
    ov_clear(&ovf);
}


std::span<const std::byte>
DecoderVorbis::feed(std::span<const std::byte> input)
{
    return input_stream.write_some(input);
}


std::optional<std::span<const std::byte>>
DecoderVorbis::decode()
{
    // TODO: when more than 2 channels, we should reorder them to match SDL channels.
    int bitstream;
    long r = ov_read(
        &ovf,
        output_buffer.data(),
        output_buffer.size(),
        std::endian::native == std::endian::big,
        2,
        1,
        &bitstream
    );

    if (r == 0)
        return {};
    if (r < 0) {
        LOG_ERROR("{}", vorbis_error_to_string(r));
        return {};
    }

    std::span samples{
        output_buffer.data(),
        static_cast<std::size_t>(r)
    };
    return as_bytes(samples);
}


std::optional<Decoder::Spec>
DecoderVorbis::get_spec()
    noexcept
{
    auto vinfo = ov_info(&ovf, -1);
    if (!vinfo)
        return {};
    Spec result;
    result.format = AUDIO_S16SYS;
    result.rate = vinfo->rate;
    result.channels = vinfo->channels;
    return result;
}


Decoder::Info
DecoderVorbis::get_info()
{
    Info result;

    result.codec = "Ogg Vorbis"s;

    if (auto comment = ov_comment(&ovf, -1)) {
        if (comment->vendor)
            result.codec += "; "s + comment->vendor;
    }

    long current_bitrate = ov_bitrate_instant(&ovf);
    if (current_bitrate > 0)
        bitrate = current_bitrate;

    if (bitrate)
        result.bitrate = std::format("{:.1f} Kbps", bitrate / 1000.0f);

    return result;
}


std::optional<stream_metadata>
DecoderVorbis::get_metadata()
    const
{
    auto comment = ov_comment(const_cast<OggVorbis_File*>(&ovf), -1);
    if (!comment)
        return {};

    stream_metadata result;

    auto comment_vec = to_vector(comment);
    for (auto entry : comment_vec) {
        auto tokens = string_utils::split_view(entry, "="sv, false, 2);
        if (tokens.size() != 2)
            continue;
        auto key = tokens[0];
        auto val = tokens[1];

        using string_utils::equal_case;
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


bool
DecoderVorbis::match_mime(const std::string& content_type)
{
    return mime_type::match_any(content_type, vorbis_mimes);
}


bool
DecoderVorbis::match_prefix(std::span<const std::byte> input)
{
    return utils::starts_with_any(input, vorbis_prefixes);
}


std::size_t
DecoderVorbis::read_callback(void* buf,
                             std::size_t size,
                             std::size_t count,
                             void* ctx)
{
    auto self = reinterpret_cast<DecoderVorbis*>(ctx);
    if (!self) {
        errno = EINVAL;
        return 0;
    }
    if (self->input_stream.empty()) {
        errno = EAGAIN;
        return 0;
    }

    std::span src{
        reinterpret_cast<std::byte*>(buf),
        size * count
    };

    auto result = self->input_stream.read(src);
    return result.size();
}
