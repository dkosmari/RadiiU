/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <bit>
#include <cerrno>
#include <format>
#include <string_view>
#include <array>

#include <opus/opus_defines.h>

#include "DecoderOpus.hpp"

#include "LogManager.hpp"
#include "mime_type.hpp"
#include "string_utils.hpp"
#include "utils.hpp"


using namespace std::literals;


namespace {

    // Constants

    constexpr std::array opus_mimes{
        "audio/opus"sv,
        "audio/ogg; codecs=opus"sv,
        "audio/ogg"sv,
        "application/ogg"sv,
    };


    constexpr std::array opus_prefixes{
        "OggS"sv,
    };


    // Function definitions

    std::string
    opus_error_to_string(int code)
    {
        return opus_strerror(code);
    }


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


DecoderOpus::Error::Error(int code) :
    std::runtime_error{opus_error_to_string(code)}
{}


DecoderOpus::Error::Error(const std::string& msg,
                          int code) :
    std::runtime_error{msg + ": " + opus_error_to_string(code)}
{}


// NOTE: we down/up mix to stereo, to avoid worrying about remapping channels
DecoderOpus::DecoderOpus(std::span<const std::byte> data) :
    output_buffer(8192 * 2)
{
    OpusFileCallbacks callbacks {
        .read = &read_callback,
        .seek = nullptr,
        .tell = nullptr,
        .close = nullptr,
    };

    int e;
    oof = op_open_callbacks(
        this,
        &callbacks,
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size_bytes(),
        &e
    );
    if (!oof)
        throw Error{"op_open_callbacks() failed", e};

    LOG_INFO("Created Opus decoder.");
}


DecoderOpus::~DecoderOpus()
    noexcept
{
    op_free(oof);
}


std::span<const std::byte>
DecoderOpus::feed(std::span<const std::byte> data)
{
    return input_stream.write_some(data);
}


std::optional<std::span<const std::byte>>
DecoderOpus::decode()
{
    int r = op_read_stereo(
        oof,
        output_buffer.data(),
        output_buffer.size()
    );

    if (r <= 0) {
        switch (r) {
            case 0:
                return {};
            case OP_HOLE:
            case OP_EREAD:
                LOG_DEBUG("Harmless (?) Opus error: {:?}", opus_error_to_string(r));
                return {};
            default:
                throw Error{"op_read_stereo() failed", r};
        }
    }

    std::span decoded{
        output_buffer.data(),
        static_cast<std::size_t>(r * 2)
    };
    return as_bytes(decoded);
}


std::optional<Decoder::Spec>
DecoderOpus::get_spec()
    noexcept
{
    Spec result;
    result.format = AUDIO_S16SYS;
    result.rate = 48000;
    //result.channels = op_channel_count(oof, -1);
    result.channels = 2; // NOTE: we hardcode the output to be stereo
    return result;
}


Decoder::Info
DecoderOpus::get_info()
{
    Info result;

    result.codec = "Ogg Opus";

    if (auto tags = op_tags(oof, -1)) {
        if (tags->vendor)
            result.codec += "; "s + tags->vendor;
    }

    opus_int32 br = op_bitrate_instant(oof);
    if (br > 0)
        bitrate = br;

    if (bitrate)
        result.bitrate = std::format("{:.1f} Kbps", bitrate / 1000.0f);

    return result;
}


bool
DecoderOpus::get_metadata(stream_metadata& meta)
    const
{
    auto tags = op_tags(oof, -1);
    if (!tags)
        return false;

    auto tags_vec = to_vector(tags);
    for (auto entry : tags_vec) {

        auto tokens = string_utils::split_view(entry, "="sv, false, 2);
        if (tokens.size() != 2)
            continue;

        auto key = tokens[0];
        auto val = tokens[1];

        using string_utils::equal_case;
        if (equal_case(key, "TITLE"sv))
            meta.title = val;
        else if (equal_case(key, "ARTIST"sv))
            meta.artist = val;
        else if (equal_case(key, "ALBUM"sv))
            meta.album = val;
        else if (equal_case(key, "GENRE"sv))
            meta.genre = val;
        else
            meta.extra[std::string{key}] = std::string{val};

    }

    return true;
}


bool
DecoderOpus::match_mime(const std::string& content_type)
{
    return mime_type::match_any(content_type, opus_mimes);
}


bool
DecoderOpus::match_prefix(std::span<const std::byte> data)
{
    return utils::starts_with_any(data, opus_prefixes);
}


int
DecoderOpus::read_callback(void* ctx,
                           unsigned char* buf,
                           int size)
{
    auto self = reinterpret_cast<DecoderOpus*>(ctx);
    if (!self) {
        errno = EINVAL;
        return -1;
    }
    if (self->input_stream.empty()) {
        errno = EAGAIN;
        return 0;
    }

    std::span src{
        reinterpret_cast<std::byte*>(buf),
        static_cast<std::size_t>(size)
    };

    auto result = self->input_stream.read(src);
    return result.size();
}
