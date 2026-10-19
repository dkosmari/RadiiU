/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <cassert>
#include <format>
#include <string_view>

#include "DecoderAAC.hpp"

#include "LogManager.hpp"
#include "mime_type.hpp"
#include "utils.hpp"


using namespace std::literals;


namespace {

    // Constants

    constexpr std::array aac_mimes{
        "audio/aac"sv,
        "audio/aacp"sv,
        "audio/x-aac"sv,
        "audio/x-hx-aac-adts"sv,
    };


    constexpr std::array aac_prefixes{
        "\xff\xf1"sv,
        "\xff\xf9"sv,
    };


#if 0
    std::string
    obj_type_to_string(unsigned char t)
    {
        switch (t) {
            case MAIN:
                return "MAIN";
            case LC:
                return "LC";
            case SSR:
                return "SSR";
            case LTP:
                return "LTP";
            case HE_AAC:
                return "HE_AAC";
            case ER_LC:
                return "ER_LC";
            case ER_LTP:
                return "ER_LTP";
            case LD:
                return "LD";
            case DRM_ER_LC:
                return "DRM_ER_LC";
            default:
                return "unknown " + std::to_string(t);
        }
    }


    std::string
    out_fmt_to_string(unsigned char f)
    {
        switch (f) {
            case FAAD_FMT_16BIT:
                return "16bit";
            case FAAD_FMT_24BIT:
                return "24bit";
            case FAAD_FMT_32BIT:
                return "32bit";
            case FAAD_FMT_FLOAT:
                return "float";
            case FAAD_FMT_DOUBLE:
                return "double";
            default:
                return "unknown " + std::to_string(f);
        }
    }


    void
    dump(const NeAACDecConfiguration* cfg)
    {
        LOG_DEBUG("aac conf:\n"
                  "  defObjectType: {}\n"
                  "  defSampleRate: {}\n"
                  "  outputFormat: {}\n"
                  "  downMatrix: {}\n"
                  "  useOldADTSFormat: {}\n"
                  "  dontUpSampleImplicitSBR: {}\n",
                  obj_type_to_string(cfg->defObjectType),
                  cfg->defSampleRate,
                  out_fmt_to_string(cfg->outputFormat),
                  cfg->downMatrix,
                  cfg->useOldADTSFormat,
                  cfg->dontUpSampleImplicitSBR);
    }
#endif


    std::string
    header_to_string(unsigned char header)
    {
        switch (header) {
            case RAW:
                return "RAW"s;
            case ADIF:
                return "ADIF"s;
            case ADTS:
                return "ADTS"s;
            case LATM:
                return "LATM"s;
            default:
                return "<UNKOWN>"s;
        }
    }


    std::string
    sbr_to_string(unsigned char sbr)
    {
        switch (sbr) {
            case NO_SBR:
                return "no SBR"s;
            case SBR_UPSAMPLED:
                return "upsampled SBR"s;
            case SBR_DOWNSAMPLED:
                return "downsampled SBR"s;
            case NO_SBR_UPSAMPLED:
                return "upsampled"s;
            default:
                return "<UNKNOWN>"s;
        }
    }

} // namespace


DecoderAAC::Error::Error(unsigned char code) :
    std::runtime_error{NeAACDecGetErrorMessage(code)}
{}


DecoderAAC::Error::Error(const std::string& msg) :
    std::runtime_error{msg}
{}


DecoderAAC::Error::Error(const std::string& msg,
                         unsigned char code) :
    std::runtime_error{msg + ": "s + NeAACDecGetErrorMessage(code)}
{}


DecoderAAC::DecoderAAC(std::span<const std::byte> input) :
    handle{open()}
{
    auto cfg = NeAACDecGetCurrentConfiguration(handle);
    // dump(cfg);
    cfg->outputFormat = FAAD_FMT_16BIT;
    // cfg->defSampleRate = 44100;
    cfg->downMatrix = 1; // downmix to stereo
    if (!NeAACDecSetConfiguration(handle, cfg)) {
        close();
        throw Error{"NeAACDecSetConfiguration() failed"};
    }

    auto r = NeAACDecInit(
        handle,
        reinterpret_cast<unsigned char*>(const_cast<std::byte*>(input.data())),
        input.size_bytes(),
        &rate,
        &channels
    );
    if (r < 0) {
        close();
        throw Error{"NeAACDecInit() failed"};
    }
    assert((unsigned long)r <= input.size());

    // LOG_DEBUG("DecoderAAC::rate = {}\n"
    //           "DecoderAAC::channels = {}",
    //           rate,
    //           channels);

    cfg = NeAACDecGetCurrentConfiguration(handle);
    // dump(cfg);

    input_stream.write(input.subspan(r));

    LOG_INFO("Created AAC decoder.");
}


DecoderAAC::~DecoderAAC()
    noexcept
{
    close();
}


std::span<const std::byte>
DecoderAAC::feed(std::span<const std::byte> input)
{
    return input_stream.write(input);
}


std::optional<std::span<const std::byte>>
DecoderAAC::decode()
{
    if (input_stream.empty())
        return {};
    if (input_stream.size() < FAAD_MIN_STREAMSIZE)
        return {};

    NeAACDecFrameInfo frame{}; // zero-initialize it
    input_buffer.resize(input_stream.size());
    auto available = input_stream.peek(input_buffer);
    auto samples_ptr = NeAACDecDecode(handle,
                                      &frame,
                                      reinterpret_cast<unsigned char*>(available.data()),
                                      available.size());
    if (frame.error) {
        if (frame.bytesconsumed == 0 || frame.bytesconsumed >= available.size())
            return {}; // not enough data to decode a frame
        throw Error{"NeAACDecDecode() failed", frame.error}; // stop decoding
    }

    input_stream.discard(frame.bytesconsumed);

    total_bytes += frame.bytesconsumed;
    if (frame.samples > 0 && frame.channels > 0 && frame.samplerate > 0)
        total_seconds += double(frame.samples)
            /
            (double(frame.channels) * double(frame.samplerate));

    current_header = frame.header_type;
    current_sbr = frame.sbr;

    std::span<std::int16_t> samples{
        reinterpret_cast<std::int16_t*>(samples_ptr),
        frame.samples
    };

    return as_bytes(samples);
}


std::optional<Decoder::Spec>
DecoderAAC::get_spec()
    noexcept
{
    Spec result;
    result.format = AUDIO_S16SYS;
    result.rate = rate;
    result.channels = 2;
    return result;
}


Decoder::Info
DecoderAAC::get_info()
{
    Info result;
    result.codec = "AAC; "s
        + header_to_string(current_header)
        + "; "s
        + sbr_to_string(current_sbr);

    if (total_seconds > 0) {
        double total_bits = 8.0 * total_bytes;
        double kbps = (total_bits / total_seconds) / 1000.0;
        current_bitrate = std::format("{:.1f} Kbps", kbps);
    }
    result.bitrate = current_bitrate;

    total_seconds = 0;
    total_bytes = 0;

    return result;
}


std::optional<stream_metadata>
DecoderAAC::get_metadata()
    const
{
    return {};
}


bool
DecoderAAC::match_mime(const std::string& content_type)
{
    return mime_type::match_any(content_type, aac_mimes);
}


bool
DecoderAAC::match_prefix(std::span<const std::byte> input)
{
    return utils::starts_with_any(input, aac_prefixes);
}


NeAACDecHandle
DecoderAAC::open()
{
    auto h = NeAACDecOpen();
    if (!h)
        throw Error{"NeAACDecOpen() failed"};
    return h;
}


void
DecoderAAC::close()
    noexcept
{
    if (handle) {
        NeAACDecClose(handle);
        handle = nullptr;
    }
}
