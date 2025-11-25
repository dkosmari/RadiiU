/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <iostream>

#include "decoder_aac.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace decoder {

    aac::error::error(unsigned char code) :
        std::runtime_error{NeAACDecGetErrorMessage(code)}
    {}


    aac::error::error(const std::string& msg) :
        std::runtime_error{msg}
    {}


    aac::error::error(const std::string& msg,
                      unsigned char code) :
        std::runtime_error{msg + ": "s + NeAACDecGetErrorMessage(code)}
    {}


    namespace {

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
            cout << "aac conf:\n"
                 << "  defObjectType: " << obj_type_to_string(cfg->defObjectType) << '\n'
                 << "  defSampleRate: " << cfg->defSampleRate << '\n'
                 << "  outputFormat: " << out_fmt_to_string(cfg->outputFormat) << '\n'
                 << "  downMatrix: " << (unsigned)cfg->downMatrix << '\n'
                 << "  useOldADTSFormat: " << (unsigned)cfg->useOldADTSFormat << '\n'
                 << "  dontUpSampleImplicitSBR: " << (unsigned)cfg->dontUpSampleImplicitSBR
                 << endl;
        }
#endif

    } // namespace


    aac::aac(std::span<const char> data) :
        handle{open()}
    {
        auto cfg = NeAACDecGetCurrentConfiguration(handle);
        // dump(cfg);
        cfg->outputFormat = FAAD_FMT_16BIT;
        // cfg->defSampleRate = 44100;
        cfg->downMatrix = 1; // downmix to stereo
        if (!NeAACDecSetConfiguration(handle, cfg)) {
            close();
            throw error{"NeAACDecSetConfiguration() failed"};
        }

        auto r = NeAACDecInit(handle,
                              reinterpret_cast<unsigned char*>(const_cast<char*>(data.data())),
                              data.size_bytes(),
                              &rate,
                              &channels);
        if (r < 0) {
            close();
            throw error{"NeAACDecInit() failed"};
        }
        assert((unsigned long)r <= data.size());

        cout << "aac::rate = " << rate << '\n'
             << "aac::channels = " << (unsigned)channels << endl;

        cfg = NeAACDecGetCurrentConfiguration(handle);
        // dump(cfg);

        stream.write(data.subspan(r));
    }


    aac::~aac()
        noexcept
    {
        close();
    }


    std::size_t
    aac::feed(std::span<const char> data)
    {
        return stream.write(data);
    }


    std::span<const char>
    aac::decode()
    {
        if (stream.empty())
            return {};
        if (stream.size() < FAAD_MIN_STREAMSIZE * channels)
            return {};

        NeAACDecFrameInfo frame;
        std::vector<unsigned char> buf(stream.size());
        auto sz = stream.peek(std::span{buf});
        auto samples = NeAACDecDecode(handle,
                                      &frame,
                                      buf.data(),
                                      sz);
        if (frame.error) {
            //throw error{"NeAACDecDecode() failed", frame.error};
            cout << "aac::decode(): error: "
                 << NeAACDecGetErrorMessage(frame.error)
                 << endl;
            return {};
        }
        stream.discard(frame.bytesconsumed);
        if (frame.samples == 0) {
            cout << "aac::decode(): frame.samples == 0" << endl;
            return {};
        }

        current_channels = frame.channels;
        current_rate = frame.samplerate;

        return {
            reinterpret_cast<const char*>(samples),
            2 * frame.samples
        };
    }


    std::optional<spec>
    aac::get_spec()
        noexcept
    {
        spec result;
        result.format = AUDIO_S16SYS;
        result.rate = rate;
        result.channels = 2;
        return result;
    }


    info
    aac::get_info()
    {
        info result;
        result.codec = "AAC";
        // TODO: estimate bitrate
        return result;
    }


    std::optional<stream_metadata>
    aac::get_metadata()
        const
    {
        return {};
    }


    NeAACDecHandle
    aac::open()
    {
        auto h = NeAACDecOpen();
        if (!h)
            throw error{"NeAACDecOpen() failed"};
        return h;
    }


    void
    aac::close()
        noexcept
    {
        if (handle) {
            NeAACDecClose(handle);
            handle = nullptr;
        }
    }

} // namespace decoder
