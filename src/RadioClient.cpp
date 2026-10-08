/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>
#include <cassert>
#include <chrono>
#include <ranges>

#ifdef __WUT__
#include <coreinit/energysaver.h>
#endif

#include <curlxx/url.hpp>
#include <curlxx/easy.hpp>
#include <curlxx/multi.hpp>

#include "RadioClient.hpp"

#include "m3u.hpp"
#include "mime_type.hpp"
#include "pls.hpp"
#include "Settings.hpp"
#include "TraceDuration.hpp"
#include "TraceFunction.hpp"
#include "TraceManager.hpp"
#include "tracer.hpp"
#include "LogManager.hpp"
#include "LogManagerCurl.hpp"
#include "string_utils.hpp"


using namespace std::literals;

using Settings::cfg;

// TODO: use tsdemux/TSDuck/demux-mpegts to demux MPEG-TS streams.

namespace {

    // Mime types for audio streams
    constexpr std::array audio_mimes {
        "audio/*"sv,
        "application/ogg"sv,
        "application/octet-stream"sv,
    };


    // Function declarations.

    std::string
    format_to_string(sdl::audio::format fmt);


    // Function definitions.

    std::string
    format_to_string(sdl::audio::format fmt)
    {
        auto sample_size   = SDL_AUDIO_BITSIZE(fmt);
        bool is_float      = SDL_AUDIO_ISFLOAT(fmt);
        bool is_big_endian = SDL_AUDIO_ISBIGENDIAN(fmt);
        bool is_signed     = SDL_AUDIO_ISSIGNED(fmt);
        return std::format("{}{}{}{}",
                           is_signed ? "" : "u",
                           is_float ? "float" : "int",
                           sample_size,
                           is_big_endian ? "be" : "le");
    }

} // namespace


RadioClient::APDDisabler::APDDisabler()
    noexcept
{
#ifdef __WUT__
    IMDisableAPD();
#endif
}


RadioClient::APDDisabler::~APDDisabler()
    noexcept
{
#ifdef __WUT__
    IMEnableAPD();
#endif
}


RadioClient::RadioClient(const std::string& user_agent_,
                         const std::string& station_url_,
                         std::initializer_list<std::string_view> extra_urls) :
    user_agent{user_agent_},
    station_url{station_url_},
    network_buffer(64 * 1024)
{
    {
        auto url_info_vec = safe_url_info_vec.lock();
        url_info_vec->emplace_back(station_url);
    }
    for (const auto& url : extra_urls)
        add_url(std::string{url});

    network_thread = std::jthread{
        std::bind_front(&RadioClient::network_thread_function, this)
    };

}


RadioClient::~RadioClient()
    noexcept
{
    stop();
}


void
RadioClient::process()
{
    TraceFunction tf{"RadioClient"sv};

    if (is_playing()) {
        if (!apd_disabler && cfg.disable_apd)
            apd_disabler.emplace();
    } else {
        if (apd_disabler)
            apd_disabler.reset();
    }

    // main_tasks.dispatch_all()
}


void
RadioClient::play(unsigned idx)
{
    TraceFunction tf{"RadioClient"sv};

    stop();

    network_tasks.add(
        "task_queue_transfer"sv,
        &RadioClient::task_queue_transfer,
        this,
        idx
    );
}


void
RadioClient::stop()
{
    TraceFunction tf{"RadioClient"sv};

    if  (is_playing()) {
        phase = Phase::stopped;
        network_tasks.add(
            "task_cancel_transfer"sv,
            &RadioClient::task_cancel_transfer,
            this
        );
        decoder_thread.request_stop();
        encoded_audio.close();
    }
}


bool
RadioClient::is_playing()
    const noexcept
{
    switch (phase.load()) {
        using enum Phase;

        case fetching:
        case streaming:
            return true;

        default:
            return false;
    }
}


void
RadioClient::with_metadata(const MetadataFunc& func)
    const
{
    safe_metadata.with_c_lock(func);
}


void
RadioClient::with_decoder_info(const DecoderInfoFunc& func)
    const
{
    safe_decoder_info.with_c_lock(func);
}


void
RadioClient::with_device_info(const DeviceInfoFunc& func)
    const
{
    safe_device_info.with_c_lock(func);
}


void
RadioClient::with_url_info_vec(const URLInfoVecFunction& func)
    const
{
    safe_url_info_vec.with_c_lock(func);
}


unsigned
RadioClient::get_current_url_info_index()
    const
{
    return url_info_idx;
}


void
RadioClient::add_url(const std::string& input_url)
{
    TraceFunction tf{"RadioClient"sv};

    if (input_url.empty())
        return;

    auto url_info_vec = safe_url_info_vec.lock();

    for (auto& [url, content_type, type] : *url_info_vec)
        if (url == input_url)
            return;

    url_info_vec->emplace_back(input_url);
}


void
RadioClient::network_thread_function(std::stop_token stopper)
    noexcept
{
    TraceManager::thread_name("RadioClient network thread"sv);
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Started network thread.");
    try {

        while (!stopper.stop_requested()) {

            if (!network_tasks.empty()) {
                TraceDuration duration_tasks_dispatch{
                    "network_tasks.dispatch_all()"sv,
                    "RadioClient"sv
                };
                network_tasks.dispatch_all();
            }

            multi.poll(100ms, stopper);

            if (stopper.stop_requested())
                break;

            {
                TraceDuration multi_perform_duration{
                    "multi.perform()"sv,
                    "RadioClient"sv
                };
                multi.perform();
            }


            if (stopper.stop_requested())
                break;

            for (auto [ez, code] : multi.get_done()) {
                assert(ez == &easy);
                multi.remove(easy);
                if (code) {
                    on_transfer_error(curl::error{code});
                } else {
                    on_transfer_finished();
                }
            }

        }

    }
    catch (std::exception& e) {
        LOG_ERROR("Exception in network thread: {}", e.what());
    }
    catch (...) {
        LOG_ERROR("Unknown exception in network thread.");
    }
    LOG_DEBUG("Ending network thread.");
}


void
RadioClient::decoder_thread_function(std::stop_token stopper)
    noexcept
{
    TraceManager::thread_name("RadioClient decoder thread"sv);
    TraceFunction tf{"RadioClient"sv};

    decoder_state = DecoderState::running;
    LOG_DEBUG("Started decoder thread.");
    try {
        constexpr unsigned max_failures = 10;
        unsigned create_failures = 0;

        std::vector<std::byte> buffer(64 * 1024);

        while (!stopper.stop_requested()) {

            if (encoded_audio.is_eof()) {
                LOG_DEBUG("encoded_audio is EOF");
                break;
            }

            if (!decoder) {

                TraceDuration create_decoder_duration{
                    "create decoder"sv,
                    "RadioClient"sv
                };

                // No decoder was created yet.
                auto data = encoded_audio.peek(std::span{buffer});
                if (data.empty()) {
                    std::this_thread::sleep_for(100ms);
                    continue;
                }
                try {
                    LOG_INFO("Attempting to create audio decoder for content-type: {}",
                             stream_content_type);
                    decoder = Decoder::create(stream_content_type, data);
                    LOG_INFO("Decoder created.");
                    encoded_audio.discard(data.size());
                    resume_transfer_if_paused();
                }
                catch (std::exception& e) {
                    ++create_failures;
                    // TODO: count failures and give up after a while?
                    LOG_DEBUG("Cannot create decoder: {}", e.what());
                    if (create_failures >= max_failures)
                        throw std::runtime_error{"Too many failures"};
                    std::this_thread::sleep_for(100ms);
                }

            } else {

                // We have a decoder already
                auto data = encoded_audio.read(std::span{buffer}, stopper);
                if (stopper.stop_requested())
                    break;

                resume_transfer_if_paused();

                decoder->feed(data);

                if (!audio_dev) {
                    TraceDuration create_device_duration{
                        "create audio device"sv,
                        "RadioClient"sv
                    };

                    if (auto decoder_spec = decoder->get_spec()) {
                        sdl::audio::spec desired_spec;
                        desired_spec.freq     = decoder_spec->rate;
                        desired_spec.channels = decoder_spec->channels;
                        desired_spec.format   = decoder_spec->format;
                        desired_spec.samples  = 4096;
                        LOG_INFO("Creating audio device: {} Hz, {}",
                                 desired_spec.freq,
                                 format_to_string(desired_spec.format));
                        sdl::audio::spec obtained_spec;
                        audio_dev.create(nullptr, false, desired_spec, obtained_spec);
                        {
                            auto info = safe_device_info.lock();
                            info->active = true;
                            info->spec = std::move(obtained_spec);
                            info->queued = 0;
                        }
                        audio_dev.unpause();
                    }
                }

                if (stopper.stop_requested())
                    break;

                if (audio_dev) {

                    while (!stopper.stop_requested()) {

                        auto samples = decoder->decode();
                        if (samples.empty())
                            break;

                        if (auto dec_meta = decoder->get_metadata()) {
                            auto metadata = safe_metadata.lock();
                            metadata->merge(*dec_meta);
                        }

                        if (stopper.stop_requested())
                            break;

                        audio_dev.play(samples);

                        {
                            auto info = safe_device_info.lock();
                            auto sample_size = SDL_AUDIO_BITSIZE(info->spec.format);
                            float seconds =
                                audio_dev.get_queued_size()
                                /
                                float(sample_size * info->spec.freq);
                            info->queued = seconds;
                        }

                    } // while (!stopper.stop_requested())

                } // if (audio_dev)

            }

        } // while (!stopper.stop_requested())
    }
    catch (std::exception& e) {
        LOG_ERROR("Exception in decoder thread: {}", e.what());
    }
    catch (...) {
        LOG_ERROR("Unknown exception in decoder thread.");
    }

    LOG_DEBUG("Ending decoder thread.");

    if (audio_dev) {
        audio_dev.clear();
        audio_dev.destroy();
    }

    safe_device_info.store(DeviceInfo{});
    safe_metadata.store(stream_metadata{});

    decoder.reset();

    decoder_state = DecoderState::stopped;
}


void
RadioClient::task_queue_transfer(unsigned idx)
{
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Network task: queue transfer.");

    if (phase == Phase::error) {
        LOG_ERROR("Canceled task because it's in error phase.");
        return;
    }

    phase = Phase::fetching;

    if (transfer_state != TransferState::idle) {
        LOG_ERROR("invalid transfer state");
        return;
    }

    std::ignore = multi.try_remove(easy);

    easy.reset();

    LogManagerCurl::capture_curl_debug(easy);

    std::string next_url;

    url_info_idx = idx;

    {
        auto url_info_vec = safe_url_info_vec.c_lock();
        if (idx >= url_info_vec->size()) {
            LOG_ERROR("Exausted all URLs, no stream found.");
            phase = Phase::error;
            return;
        }
        next_url = url_info_vec->at(idx).url;
        if (next_url.empty()) {
            LOG_ERROR("BUG: URL is empty!");
            phase = Phase::error;
            return;
        }
    }

    easy.set_verbose(cfg.verbose_stream_logs);

    if (!user_agent.empty())
        easy.set_user_agent(user_agent);
    easy.set_accept_encoding("");
    easy.set_auto_referer(true);
    easy.set_buffer_size(64 * 1024);
    easy.set_fail_on_error(true);
    easy.set_follow_location(true);
    easy.set_forbid_reuse(false);
    easy.set_http_09_allowed(true);
    easy.set_http_200_aliases({"ICY 200 OK"});
    easy.set_http_version(curl::easy::http_version::none);
    easy.set_url(next_url);
    easy.set_ssl_verify_peer(false);
    easy.set_tcp_no_delay(false);
    easy.set_transfer_encoding(true);
    easy.set_tcp_keep_alive(true);
    easy.set_tcp_keep_idle(120s);
    easy.set_tcp_keep_intvl(60s);
    easy.set_write_function(
        std::bind_front(&RadioClient::on_transfer_recv, this)
    );

    easy.append_http_header("Icy-Metadata: 1");

    multi.add(easy);

    playlist_type = PlaylistType::none;
    playlist_data.clear();

    transfer_state = TransferState::started;
}


void
RadioClient::task_cancel_transfer()
{
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Network task: transfer.");

    std::ignore = multi.try_remove(easy);
    easy.reset();

    transfer_state = TransferState::idle;
}


void
RadioClient::task_resume_transfer()
{
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Network task: resume transfer.");

    easy.unpause();
    transfer_state = TransferState::in_progress;
}


void
RadioClient::on_transfer_error(const curl::error& e)
{
    TraceFunction tf{"RadioClient"sv};

    LOG_ERROR("Transfer error: {}", e.what());
    network_error_message.store(e.what());
    transfer_state = TransferState::error;
}


void
RadioClient::on_transfer_finished()
{
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Transfer finished.");
    transfer_state = TransferState::idle;

    URLType type = safe_url_info_vec.lock()->at(url_info_idx).type;
    switch (type) {
        using enum URLType;

        case unknown:
            break;

        case playlist:
            process_playlist();
            try_next_url();
            break;

        case audio:
            // Connection ended.
            encoded_audio.close();
            break;

        case other:
            break;
    }
}


std::size_t
RadioClient::on_transfer_recv(std::span<const std::byte> input)
{
    TraceFunction tf{"RadioClient"sv};

    if (input.empty()) {
        LOG_DEBUG("Connection ended.");
        return 0;
    }

    if (transfer_state == TransferState::started)
        on_transfer_started(input);

    icy_parser.feed(input);

    {
        auto metadata = safe_metadata.lock();
        metadata->merge(icy_parser.get_metadata());
    }

    URLType type = safe_url_info_vec.lock()->at(url_info_idx).type;

    // Loop to consume all data from the icy_parser
    while (true) {
        auto data = icy_parser.get_data(network_buffer);
        if (data.empty())
            break;

        switch (type) {
            using enum URLType;

            case unknown: // should not happen
                LOG_ERROR("BUG: receiving unknown data type");
                return CURL_WRITEFUNC_ERROR;

            case playlist: {
                auto written = playlist_data.write(data);
                // If the playlist is huge, just give up, let libcurl know we can't handle it.
                if (written.empty())
                    return CURL_WRITEFUNC_ERROR;
                break;
            }

            case audio: {
                // Make sure the decoder thread is running.
                if (decoder_state == DecoderState::stopped)
                    start_decoder_thread();

                // If accumulated too much audio, tell libcurl to pause.
                auto written = encoded_audio.write(data);
                if (written.empty()) {
                    LOG_DEBUG("Pausing transfer: encoded_audio buffer is full.");
                    TransferState expected = TransferState::in_progress;
                    if (!transfer_state.compare_exchange_strong(
                            expected,
                            TransferState::paused)) {
                        LOG_ERROR("Incorrect state: should be in_progress, got {}",
                                  to_string(expected));
                        return CURL_WRITEFUNC_ERROR;
                    }

                    return CURL_WRITEFUNC_PAUSE;
                }
                break;
            }

            case other:
                return CURL_WRITEFUNC_ERROR;

        }
    }

    return input.size();
}


void
RadioClient::on_transfer_started(std::span<const std::byte> data)
{
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Transfer started.");

    transfer_state = TransferState::in_progress;

    auto effective_url = easy.get_effective_url();

    std::string path;
    try {
        curl::url url_parser{effective_url};
        path = url_parser.get_path();
    }
    catch (std::exception& e) {
        LOG_ERROR("Could not parse URL: {}", e.what());
    }

    std::string content_type;
    if (auto hdr = easy.try_get_header("content-type"))
        content_type = hdr->value;

    {
        // NOTE: minimize lock scope for safe_url_info_vec
        auto url_info_vec = safe_url_info_vec.lock();
        auto& info = url_info_vec->at(url_info_idx);

        if (m3u::match_path(path) ||
            m3u::match_mime(content_type) ||
            m3u::match_prefix(data)) {

            LOG_INFO("Detected M3U: {:?}", content_type);
            info.content_type = content_type;
            info.type = URLType::playlist;
            playlist_type = PlaylistType::m3u;

        } else if (pls::match_path(path) ||
                   pls::match_mime(content_type) |
                   pls::match_prefix(data)) {

            LOG_INFO("Detected PLS: {:?}", content_type);
            info.content_type = content_type;
            info.type = URLType::playlist;
            playlist_type = PlaylistType::pls;

        } else if (mime_type::match_any(content_type, audio_mimes)) {

            LOG_INFO("Detected audio mime: {:?}", content_type);
            phase = Phase::streaming;
            stream_content_type = content_type;
            info.content_type = content_type;
            info.type = URLType::audio;
            playlist_type = PlaylistType::none;
            encoded_audio.clear();

        } else {

            LOG_WARN("HTTP transfer of unknown type: {:?}", content_type);

        }
    }

    safe_metadata.store(stream_metadata{});

    std::size_t icy_meta_int = 0;
    if (auto hdr = easy.try_get_header("icy-metaint")) {
        LOG_INFO("icy-metaint={}", hdr->value);
        icy_meta_int = std::stoull(hdr->value);
    }
    icy_parser.reset(std::move(effective_url), icy_meta_int);

    {
        using string_utils::trimmed;
        auto metadata = safe_metadata.lock();

        if (auto hdr = easy.try_get_header("icy-name"))
            metadata->station.name = trimmed(hdr->value);

        if (auto hdr = easy.try_get_header("icy-url"))
            metadata->station.url = trimmed(hdr->value);

        if (auto hdr = easy.try_get_header("icy-genre"))
            metadata->station.genre = trimmed(hdr->value);

        if (auto hdr = easy.try_get_header("icy-description"))
            metadata->station.description = trimmed(hdr->value);

        // TODO: process icy-noticeN headers.


#if 0
        if (auto hdr = easy.try_get_header("icy-br")) {
            // No need to store advertised bitrate.
        }

        if (auto hdr = easy.try_get_header("icy-audio-info")) {
            // No need to store advertised audio info.
        }

        if (auto hdr = easy.try_get_header("icy-pub")) {
            // No need to store this.
        }
#endif

    }
}


void
RadioClient::start_decoder_thread()
{
    TraceFunction tf{"RadioClient"sv};

    decoder_state = DecoderState::queued;
    decoder_thread = std::jthread{
        std::bind_front(&RadioClient::decoder_thread_function, this)
    };
}


void
RadioClient::resume_transfer_if_paused()
{
    if (transfer_state == TransferState::paused) {
        transfer_state = TransferState::unpausing;
        network_tasks.add(
            "task_resume_transfer"sv,
            &RadioClient::task_resume_transfer,
            this
        );
    }
}


void
RadioClient::process_playlist()
{
    TraceFunction tf{"RadioClient"sv};

    // NOTE: make a copy of the URLInfo entry, so we don't keep the list locked.
    unsigned idx = url_info_idx;
    auto info = safe_url_info_vec.lock()->at(idx);

    LOG_DEBUG("Processing playlist: {}", to_string(playlist_type));

    std::string playlist_str = playlist_data.read_all_as<std::string>();

    switch (playlist_type) {
        using enum PlaylistType;

        case none:
            LOG_ERROR("BUG: no playlist type detected.");
            break;

        case m3u:
            try {
                auto pl = m3u::parse(playlist_str);
                for (const auto& track : pl) {
                    curl::url url_parser{info.url};
                    url_parser.set_url(track.url);
                    add_url(url_parser.get_url());
                }
            }
            catch (std::exception& e) {
                LOG_ERROR("{}\n<m3u>\n{}\n</m3u>", e.what(), playlist_str);
            }
            break;

        case pls:
            try {
                auto pl = pls::parse(playlist_str);
                for (const auto& track : pl) {
                    curl::url url_parser{info.url};
                    url_parser.set_url(track.url);
                    add_url(url_parser.get_url());
                }
            }
            catch (std::exception& e) {
                LOG_ERROR("{}\n<pls>\n{}\n</pls>", e.what(), playlist_str);
            }
            break;

        case asx:
        case cue:
        case jspf:
        case wpl:
        case xspf:
            LOG_ERROR("Playlist type not supported yet.");
            break;

        default:
            LOG_ERROR("Invalid playlist type");
    }
}


void
RadioClient::try_next_url()
{
    network_tasks.add(
        "task_queue_transfer"sv,
        &RadioClient::task_queue_transfer,
        this,
        url_info_idx + 1
    );
}


std::string
to_string(RadioClient::Phase phase)
{
    switch (phase) {
        using enum RadioClient::Phase;

        case stopped:
            return "stopped"s;
        case fetching:
            return "fetching"s;
        case streaming:
            return "streaming"s;
        case error:
            return "error"s;
        default:
            return "<UNKNOWN>"s;
    }
}


std::string
to_string(RadioClient::PlaylistType type)
{
    switch (type) {
        using enum RadioClient::PlaylistType;

        case none:
            return "none"s;
        case asx:
            return "asx"s;
        case cue:
            return "cue"s;
        case jspf:
            return "jspf"s;
        case m3u:
            return "m3u"s;
        case pls:
            return "pls"s;
        case wpl:
            return "wpl"s;
        case xspf:
            return "xspf"s;
        default:
            return "<UNKNOWN>"s;
    }
}


std::string
to_string(RadioClient::TransferState state)
{
    switch (state) {
        using enum RadioClient::TransferState;

        case idle:
            return "idle"s;
        case queued:
            return "queued"s;
        case started:
            return "started"s;
        case in_progress:
            return "in_progress"s;
        case paused:
            return "paused"s;
        case unpausing:
            return "unpausing"s;
        case error:
            return "error"s;
        default:
            return "<UNKNOWN>";
    }
}


std::string
to_string(RadioClient::URLType type)
{
    switch (type) {
        using enum RadioClient::URLType;

        case unknown:
            return "unknown"s;
        case playlist:
            return "playlist"s;
        case audio:
            return "audio"s;
        case other:
            return "other"s;
        default:
            return "<UNKNOWN>"s;
    }
}
