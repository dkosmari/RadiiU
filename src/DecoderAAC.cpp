/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <cassert>
#include <string_view>

#include "DecoderAAC.hpp"

#include "LogManager.hpp"
#include "mime_type.hpp"
#include "utils.hpp"


using namespace std::literals;


namespace Decoder {

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

    } // namespace


    AAC::Error::Error(unsigned char code) :
        std::runtime_error{NeAACDecGetErrorMessage(code)}
    {}


    AAC::Error::Error(const std::string& msg) :
        std::runtime_error{msg}
    {}


    AAC::Error::Error(const std::string& msg,
                      unsigned char code) :
        std::runtime_error{msg + ": "s + NeAACDecGetErrorMessage(code)}
    {}


    AAC::AAC(std::span<const std::byte> data) :
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
            reinterpret_cast<unsigned char*>(const_cast<std::byte*>(data.data())),
            data.size_bytes(),
            &rate,
            &channels
        );
        if (r < 0) {
            close();
            throw Error{"NeAACDecInit() failed"};
        }
        assert((unsigned long)r <= data.size());

        LOG_DEBUG("AAC::rate = {}\n"
                  "AAC::channels = {}",
                  rate,
                  channels);

        cfg = NeAACDecGetCurrentConfiguration(handle);
        // dump(cfg);

        stream.write(data.subspan(r));
    }


    AAC::~AAC()
        noexcept
    {
        close();
    }


    std::span<const std::byte>
    AAC::feed(std::span<const std::byte> data)
    {
        return stream.write(data);
    }


    std::span<const std::byte>
    AAC::decode()
    {
        if (stream.empty())
            return {};
        if (stream.size() < FAAD_MIN_STREAMSIZE * channels)
            return {};

        NeAACDecFrameInfo frame;
        std::vector<unsigned char> buf(stream.size());
        auto available = stream.peek(std::span{buf});
        auto samples = NeAACDecDecode(handle,
                                      &frame,
                                      buf.data(),
                                      available.size());
        if (frame.error) {
            // TODO: throwing from here should stop playback?
            throw Error{"NeAACDecDecode() failed", frame.error};
        }
        stream.discard(frame.bytesconsumed);
        if (frame.samples == 0) {
            LOG_ERROR("frame.samples == 0");
            return {};
        }

        current_channels = frame.channels;
        current_rate = frame.samplerate;

        return {
            reinterpret_cast<const std::byte*>(samples),
            2 * frame.samples
        };
    }


    std::optional<Spec>
    AAC::get_spec()
        noexcept
    {
        Spec result;
        result.format = AUDIO_S16SYS;
        result.rate = rate;
        result.channels = 2;
        return result;
    }


    Info
    AAC::get_info()
    {
        Info result;
        result.codec = "AAC";
        // TODO: estimate bitrate
        return result;
    }


    std::optional<stream_metadata>
    AAC::get_metadata()
        const
    {
        return {};
    }


    bool
    AAC::match_mime(const std::string& content_type)
    {
        return mime_type::match_any(content_type, aac_mimes);
    }


    bool
    AAC::match_prefix(std::span<const std::byte> data)
    {
        return utils::starts_with_any(data, aac_prefixes);
    }


    NeAACDecHandle
    AAC::open()
    {
        auto h = NeAACDecOpen();
        if (!h)
            throw Error{"NeAACDecOpen() failed"};
        return h;
    }


    void
    AAC::close()
        noexcept
    {
        if (handle) {
            NeAACDecClose(handle);
            handle = nullptr;
        }
    }

} // namespace Decoder
