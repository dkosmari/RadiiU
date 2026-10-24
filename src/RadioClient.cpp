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

#include <sys/socket.h>

#ifdef __WUT__
#include <coreinit/energysaver.h>
#endif

#include <curlxx/url.hpp>
#include <curlxx/easy.hpp>
#include <curlxx/multi.hpp>

#include "RadioClient.hpp"

#include "App.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "LogManagerCurl.hpp"
#include "m3u.hpp"
#include "mime_type.hpp"
#include "pls.hpp"
#include "Settings.hpp"
#include "string_utils.hpp"
#include "TraceDuration.hpp"
#include "TraceFunction.hpp"
#include "TraceManager.hpp"
#include "tracer.hpp"


using namespace std::literals;

using Settings::cfg;

// TODO: use tsdemux/TSDuck/demux-mpegts to demux MPEG-TS streams.

namespace {

    /*-----------*/
    /* Constants */
    /*-----------*/

    // Mime types for audio streams
    constexpr std::array audio_mimes {
        "audio/*"sv,
        "application/ogg"sv,
        "application/octet-stream"sv,
    };


    class ThreadStateManager {

        std::atomic<RadioClient::ThreadState>& state;
        std::string_view name;

    public:

        ThreadStateManager(std::atomic<RadioClient::ThreadState>& state_,
                           std::string_view name_)
            noexcept;


        ~ThreadStateManager()
            noexcept;

        void
        stopping()
            noexcept;

    }; // class ThreadStateManager


    /*-----------------------*/
    /* Function declarations */
    /*-----------------------*/

    std::string
    format_to_string(sdl::audio::format fmt);


    /*----------------------*/
    /* Function definitions */
    /*----------------------*/

    ThreadStateManager::ThreadStateManager(std::atomic<RadioClient::ThreadState>& state_,
                                           std::string_view name_)
        noexcept :
        state(state_),
        name{name_}
    {
        using enum RadioClient::ThreadState;

        auto val = state.load();
        if (val != queued)
            LOG_ERROR("BUG: wrong {}: {}",
                      name,
                      to_string(val));
        state = running;
    }


    ThreadStateManager::~ThreadStateManager()
        noexcept
    {
        state = RadioClient::ThreadState::stopped;
    }


    void
    ThreadStateManager::stopping()
        noexcept
    {
        state = RadioClient::ThreadState::stopping;
    }


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
    station_url{station_url_}
{
    {
        auto url_info_vec = safe_url_info_vec.lock();
        url_info_vec->emplace_back(station_url);
    }
    for (const auto& url : extra_urls)
        add_url(std::string{url});

    start_network_thread();
}


RadioClient::~RadioClient()
    noexcept
{
    TRACE_FUNC;

    network.thread.request_stop();
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

    TRACE_FUNC;

    if (is_playing()) {
        network.tasks.add(
            "task_cancel_transfer"sv,
            &RadioClient::task_cancel_transfer,
            this
        );
        audio.thread.request_stop();
        encoded_audio.close();
    }

    state = State::stopped;

    safe_error_message.store("");

    start_network_thread();

    network.tasks.add(
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

    TRACE_FUNC;

    if (is_playing()) {
        audio.thread.request_stop();
        encoded_audio.close();
    }

    state = State::stopped;
}


bool
RadioClient::is_playing()
    const noexcept
{
    switch (state.load()) {
        using enum State;

        case fetching:
        case streaming:
            return true;

        default:
            return false;
    }
}


bool
RadioClient::is_error()
    const noexcept
{
    return state == State::error;
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
    audio.safe_info.with_c_lock(func);
}


void
RadioClient::with_url_info_vec(const URLInfoVecFunction& func)
    const
{
    safe_url_info_vec.with_c_lock(func);
}


void
RadioClient::with_error_message(const ErrorMessageFunction& func)
    const
{
    safe_error_message.with_c_lock(func);
}


unsigned
RadioClient::get_current_url_info_index()
    const
{
    return url_info_idx;
}


RadioClient::State
RadioClient::get_state()
    const
{
    return state;
}


RadioClient::TransferState
RadioClient::get_transfer_state()
    const
{
    return network.transfer_state;
}


RadioClient::ThreadState
RadioClient::get_audio_thread_state()
    const
{
    return audio.thread_state;
}


RadioClient::ThreadState
RadioClient::get_network_thread_state()
    const
{
    return network.thread_state;
}


void
RadioClient::add_url(const std::string& new_url)
{
    TraceFunction tf{"RadioClient"sv};

    if (is_error()) {
        LOG_DEBUG("add_url() gave up: error state");
        return;
    }

    if (new_url.empty())
        return;

    auto url_info_vec = safe_url_info_vec.lock();

    // Avoid duplicates.
    for (auto& [url, content_type, type] : *url_info_vec)
        if (url == new_url)
            return;

    url_info_vec->emplace_back(new_url);
}


void
RadioClient::network_thread_function(std::stop_token stoken)
    noexcept
{
    TraceManager::thread_name("RadioClient network thread"sv);
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Started network thread.");

    ThreadStateManager tsm{
        network.thread_state,
        "network.thread_state"sv
    };

    auto& multi = network.multi;
    auto& easy = network.easy;

    try {

        assert(!network.buffer.empty());

        multi.create();
        multi.set_max_connections(2);
        multi.set_max_host_connections(2);
        multi.set_max_total_connections(2);

        easy.create();

        while (!stoken.stop_requested()) {

            if (!network.tasks.empty()) {
                TraceDuration duration_tasks_dispatch{
                    "network_tasks.dispatch_all()"sv,
                    "RadioClient"sv
                };
                network.tasks.dispatch_all();
            }


            if (is_error()) {
                auto ts = network.transfer_state.load();
                if (ts != TransferState::idle && ts != TransferState::error) {
                    std::ignore = multi.try_remove(easy);
                    on_transfer_error(curl::error{"invalid network.transfer_state: "s
                                                  + to_string(ts)});
                }
            }

            multi.poll(100ms, stoken);

            if (stoken.stop_requested())
                break;

            {
                TraceDuration multi_perform_duration{
                    "multi.perform()"sv,
                    "RadioClient"sv
                };
                multi.perform();
            }


            if (stoken.stop_requested())
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
        tsm.stopping();

    }
    catch (curl::error& e) {
        LOG_ERROR("Exception in network thread: {}", e.what());
        on_transfer_error(e);
        tsm.stopping();
        state = State::error;
    }
    catch (std::exception& e) {
        LOG_ERROR("Exception in network thread: {}", e.what());
        on_transfer_error(curl::error{e.what()});
        tsm.stopping();
        state = State::error;
    }
    catch (...) {
        LOG_ERROR("Unknown exception in network thread.");
        on_transfer_error(curl::error{"unknown exception"});
        tsm.stopping();
        state = State::error;
    }

    if (multi && easy)
        std::ignore = multi.try_remove(easy);

    multi.destroy();
    easy.destroy();

    encoded_audio.close();

    LOG_DEBUG("Ending network thread.");
}


void
RadioClient::audio_thread_function(std::stop_token stoken)
    noexcept
{
    TraceManager::thread_name("RadioClient audio thread"sv);
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Started audio thread.");

    ThreadStateManager tsm{
        audio.thread_state,
        "audio.thread_state"sv
    };

    auto& converter = audio.converter;
    auto& buffer = audio.buffer;
    auto& decoder = audio.decoder;

    try {

        assert(!buffer.empty());

        constexpr unsigned max_failures = 4;
        unsigned create_failures = 0;

        while (!stoken.stop_requested()) {

            if (is_error()) {
                LOG_DEBUG("audio thread stopping: error state");
                break;
            }

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
                auto data = encoded_audio.peek(buffer);

                // Avoid creating a decoder when less than 8 KiB was received.
                if (data.size() < 8 * 1024) {
                    std::this_thread::sleep_for(100ms);
                    continue;
                }

                try {
                    auto content_type = audio.safe_content_type.load();
                    LOG_INFO("Attempting to create audio decoder for content-type: {:?}",
                             content_type);
                    decoder = Decoder::create(content_type, data);
                    LOG_INFO("Decoder created.");
                    encoded_audio.discard(data.size());
                    resume_transfer_if_paused();
                }
                catch (std::exception& e) {
                    ++create_failures;
                    LOG_DEBUG("Cannot create decoder: {}", e.what());
                    if (create_failures >= max_failures)
                        throw std::runtime_error{"Too many failures"};
                    std::this_thread::sleep_for(100ms);
                }

            } else {

                // We have a decoder already
                auto data = encoded_audio.read(buffer, stoken);

                if (stoken.stop_requested())
                    break;

                resume_transfer_if_paused();

                decoder->feed(data);

                if (!converter) {
                    TraceDuration create_converter_duration{
                        "create audio converter"sv,
                        "RadioClient"sv
                    };

                    if (auto src_spec = decoder->get_spec()) {
                        LOG_INFO("Creating audio converter from {} Hz, {}",
                                 src_spec->rate,
                                 format_to_string(src_spec->format));

                        auto dst_spec = App::get_audio_spec();

                        converter.create(
                            src_spec->format,
                            src_spec->channels,
                            src_spec->rate,
                            dst_spec.format,
                            dst_spec.channels,
                            dst_spec.rate
                        );

                        LOG_INFO("Audio converter created.");
                    }
                }

                if (stoken.stop_requested())
                    break;

                if (converter) {

                    while (!stoken.stop_requested()) {

                        auto samples = decoder->decode();
                        if (!samples)
                            break;

                        if (stoken.stop_requested())
                            break;

                        if (!samples->empty()) {
                            converter.put(*samples);

                            while (true) {
                                auto converted = converter.get(std::span{buffer});
                                if (converted.empty())
                                    break;
                                App::play_audio(converted);
                            }
                        }

                    } // while (!stoken.stop_requested())

                } // if (audio_converter)

                audio.safe_info.store(decoder->get_info());

                {
                    stream_metadata dec_meta;
                    if (decoder->get_metadata(dec_meta))
                        safe_metadata.lock()->merge(std::move(dec_meta));
                }

            }

        } // while (!stoken.stop_requested())
        tsm.stopping();
    }
    catch (std::exception& e) {
        LOG_ERROR("Exception in audio thread: {}", e.what());
        safe_error_message.store(e.what());
        tsm.stopping();
        try_next_url();
    }
    catch (...) {
        LOG_ERROR("Unknown exception in audio thread.");
        safe_error_message.store("unknown exception in audio thread");
        tsm.stopping();
        try_next_url();
    }

    App::clear_audio();

    safe_metadata.store(stream_metadata{});

    converter.destroy();

    audio.safe_info.store(Decoder::Info{});
    decoder.reset();

    LOG_DEBUG("Ending audio thread.");
}


void
RadioClient::task_queue_transfer(unsigned idx)
{
    TraceFunction tf{"RadioClient"sv};

    if (is_error()) {
        LOG_ERROR("Canceled task because it's in error state.");
        return;
    }

    LOG_DEBUG("task_queue_transfer()");

    state = State::fetching;
    safe_error_message.store("");

    auto ts = network.transfer_state.load();
    switch (ts) {
        using enum TransferState;

        case idle:
        case error:
            break;

        case queued:
        case in_progress:
        case paused:
        case unpausing:
        default:
            LOG_ERROR("invalid transfer state: {}", to_string(ts));
    }

    assert(network.multi);
    assert(network.easy);

    auto& easy = network.easy;

    std::ignore = network.multi.try_remove(easy);
    easy.reset();

    std::string url;

    {
        auto url_info_vec = safe_url_info_vec.c_lock();
        if (idx >= url_info_vec->size()) {
            LOG_ERROR("Exausted all URLs, no stream found.");
            state = State::error;
            return;
        }
        url = url_info_vec->at(idx).url;
        if (url.empty()) {
            LOG_ERROR("BUG: URL is empty!");
            state = State::error;
            return;
        }
    }

    LogManagerCurl::capture_curl_debug(easy);
    easy.set_verbose(cfg.verbose_stream_logs);

    if (!user_agent.empty())
        easy.set_user_agent(user_agent);
    easy.set_accept_encoding("");
    easy.set_auto_referer(true);
    easy.set_buffer_size(64 * 1024);
    easy.set_connect_timeout(15s);
    easy.set_fail_on_error(true);
    easy.set_follow_location(true);
    easy.set_forbid_reuse(false);
    easy.set_http_09_allowed(true);
    easy.set_http_200_aliases({"ICY 200 OK"});
    easy.set_http_version(curl::easy::http_version::none);
    easy.set_low_speed_limit(100);
    easy.set_low_speed_time(15s);
    // Optimize TCP performance.
    easy.set_sockopt_function(
        [](curl_socket_t fd,
           curlsocktype) -> int
        {
#ifdef __WIIU__
            std::size_t winscale_arg = 1;
            setsockopt(fd, SOL_SOCKET, SO_WINSCALE, &winscale_arg, sizeof winscale_arg);
#endif
            std::size_t rcvbuf_arg = 64 * 1024;
            setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_arg, sizeof rcvbuf_arg);

            return CURL_SOCKOPT_OK;
        }
    );
    easy.set_ssl_verify_peer(false);
    easy.set_tcp_keep_alive(true);
    easy.set_tcp_keep_idle(60s);
    easy.set_tcp_keep_intvl(30s);
    easy.set_tcp_no_delay(false);
    easy.set_transfer_encoding(true);
    easy.set_url(url);
    easy.set_write_function(
        std::bind_front(&RadioClient::on_transfer_recv, this)
    );

    easy.append_http_header("Icy-Metadata: 1");

    network.multi.add(easy);

    network.playlist_type = PlaylistType::none;
    network.playlist_stream.clear();

    encoded_audio.clear();

    url_info_idx = idx;

    network.transfer_state = TransferState::queued;
}


void
RadioClient::task_cancel_transfer()
{
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("task_cancel_transfer()");

    on_transfer_error(curl::error{"transfer canceled"});

    std::ignore = network.multi.try_remove(network.easy);
    network.easy.reset();

    network.transfer_state = TransferState::idle;
    state = State::stopped; // TODO: check if we should do this here
}


void
RadioClient::task_resume_transfer()
{
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("task_resume_transfer()");

    network.easy.unpause();
    network.transfer_state = TransferState::in_progress;
}


void
RadioClient::on_transfer_error(const curl::error& e)
{
    TraceFunction tf{"RadioClient"sv};

    LOG_ERROR("Transfer error: {}", e.what());
    safe_error_message.store(e.what());
    network.transfer_state = TransferState::error;

    switch (state.load()) {
        using enum State;

        case stopped:
            break;

        case fetching:
            try_next_url();
            break;

        case streaming:
            state = error;
            encoded_audio.close();
            break;

        case error:
            break;

        default:
            state = error;
    }
}


void
RadioClient::on_transfer_finished()
{
    TraceFunction tf{"RadioClient"sv};

    LOG_DEBUG("Transfer finished.");
    network.transfer_state = TransferState::idle;

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

    if (network.transfer_state == TransferState::queued)
        on_transfer_started(input);

    network.icy_parser.feed(input);

    {
        stream_metadata icy_meta;
        if (network.icy_parser.get_metadata(icy_meta))
            safe_metadata.lock()->merge(std::move(icy_meta));
    }

    URLType type = safe_url_info_vec.lock()->at(url_info_idx).type;

    // Loop to consume all data from the icy_parser
    while (true) {
        auto data = network.icy_parser.get_data(network.buffer);
        if (data.empty())
            break;

        switch (type) {
            using enum URLType;

            case unknown: // should not happen
                LOG_ERROR("BUG: receiving unknown data type");
                return CURL_WRITEFUNC_ERROR;

            case playlist: {
                auto written = network.playlist_stream.write(data);
                // If the playlist is huge, just give up, let libcurl know we can't handle it.
                if (written.empty())
                    return CURL_WRITEFUNC_ERROR;
                break;
            }

            case audio: {
                // Make sure the decoder thread is running.
                start_audio_thread();

                // If accumulated too much audio, tell libcurl to pause.
                auto written = encoded_audio.write(data);
                if (written.empty()) {
                    LOG_DEBUG("Pausing transfer: encoded_audio buffer is full.");
                    TransferState expected = TransferState::in_progress;
                    if (!network.transfer_state.compare_exchange_strong(
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

    if (cfg.verbose_stream_logs)
        LOG_DEBUG("Transfer started.");

    network.transfer_state = TransferState::in_progress;

    auto effective_url = network.easy.get_effective_url();

    std::string path;
    try {
        curl::url url_parser{effective_url};
        path = url_parser.get_path();
    }
    catch (std::exception& e) {
        LOG_ERROR("Could not parse URL: {}", e.what());
    }

    auto& easy = network.easy;
    auto& playlist_type = network.playlist_type;

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
            playlist_type = PlaylistType::m3u;
            info.content_type = std::move(content_type);
            info.type = URLType::playlist;

        } else if (pls::match_path(path) ||
                   pls::match_mime(content_type) |
                   pls::match_prefix(data)) {

            LOG_INFO("Detected PLS: {:?}", content_type);
            playlist_type = PlaylistType::pls;
            info.content_type = std::move(content_type);
            info.type = URLType::playlist;

        } else if (mime_type::match_any(content_type, audio_mimes)) {

            LOG_INFO("Detected audio mime: {:?}", content_type);
            state = State::streaming;
            safe_error_message.store("");
            playlist_type = PlaylistType::none;
            audio.safe_content_type.store(content_type);
            info.content_type = std::move(content_type);
            info.type = URLType::audio;

        } else {

            LOG_WARN("HTTP transfer of unknown type: {:?}", content_type);

        }
    }

    network.playlist_stream.clear();
    encoded_audio.clear();

    std::size_t icy_metaint = 0;
    if (auto hdr = easy.try_get_header("icy-metaint")) {
        LOG_INFO("icy-metaint={}", hdr->value);
        try {
            icy_metaint = std::stoull(hdr->value);
        }
        catch (std::exception& e) {
            LOG_ERROR("failed to parse icy-metaint: {}", e.what());
            icy_metaint = 0;
        }
    }
    network.icy_parser.reset(std::move(effective_url), icy_metaint);

    // Initialize metadata from the ICY headers.
    {
        using string_utils::trimmed;
        using string_utils::equal_case;

        auto metadata = safe_metadata.lock();

        *metadata = {};

        if (auto hdr = easy.try_get_header("icy-name"))
            metadata->station.name = trimmed(hdr->value);

        if (auto hdr = easy.try_get_header("icy-genre"))
            metadata->station.genre = trimmed(hdr->value);

        if (auto hdr = easy.try_get_header("icy-description"))
            metadata->station.description = trimmed(hdr->value);

        if (auto hdr = easy.try_get_header("icy-url"))
            metadata->station.url = trimmed(hdr->value);

        if (auto hdr = easy.try_get_header("icy-audio-info"))
            metadata->station.audio_info = trimmed(hdr->value);

        for (auto hdr : easy.get_header_iterator()) {
            if (hdr.name.starts_with("icy-notice"sv))
                metadata->station.notice.push_back(trimmed(hdr.value));
        }

        metadata->clean_empty();
    }
}


void
RadioClient::start_network_thread()
{
    auto& thread_state = network.thread_state;
    auto st = thread_state.load();
    // early out
    if (st == ThreadState::running) [[likely]]
        return;

    TraceFunction tf{"RadioClient"sv};

    if (is_error()) {
        LOG_ERROR("start_network_thread() gave up: error state");
        return;
    }

    switch (st) {
        using enum ThreadState;

        case stopping:
            // wait until it goes to stopped
            while (thread_state != ThreadState::stopped)
                std::this_thread::sleep_for(5ms);

            [[fallthrough]];

        case stopped:
            thread_state = queued;
            network.thread = std::jthread{
                std::bind_front(&RadioClient::network_thread_function, this)
            };
            break;

        case queued:
        case running:
            break;
    }
}


void
RadioClient::start_audio_thread()
{
    auto& thread_state = audio.thread_state;
    auto st = thread_state.load();
    // early out
    if (st == ThreadState::running) [[likely]]
        return;

    TraceFunction tf{"RadioClient"sv};

    if (is_error()) {
        LOG_DEBUG("start_audio_thread() gave up: error state");
        return;
    }

    switch (st) {
        using enum ThreadState;

        case stopping:
            // wait until it goes to stopped
            while (thread_state != ThreadState::stopped)
                std::this_thread::sleep_for(5ms);

            [[fallthrough]];

        case stopped:
            thread_state = queued;
            audio.thread = std::jthread{
                std::bind_front(&RadioClient::audio_thread_function, this)
            };
            break;

        case queued:
        case running:
            break;
    }
}


void
RadioClient::resume_transfer_if_paused()
{
    if (is_error()) {
        LOG_DEBUG("resume_transfer_if_paused() gave up: error state");
        return;
    }

    if (network.transfer_state == TransferState::paused) {
        network.transfer_state = TransferState::unpausing;
        network.tasks.add(
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

    if (is_error()) {
        LOG_DEBUG("process_playlist() gave up: error state");
        return;
    }

    // NOTE: make a copy of the URLInfo entry, so we don't keep the list locked.
    unsigned idx = url_info_idx;
    auto info = safe_url_info_vec.lock()->at(idx);

    LOG_DEBUG("Processing playlist: {}", to_string(network.playlist_type));

    std::string playlist_str = network.playlist_stream.read_all_as<std::string>();
    assert(network.playlist_stream.empty());

    switch (network.playlist_type) {
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
    if (is_error()) {
        LOG_DEBUG("try_next_url() gave up: error state");
        return;
    }

    network.tasks.add(
        "task_queue_transfer"sv,
        &RadioClient::task_queue_transfer,
        this,
        url_info_idx + 1
    );
}


std::string
to_string(RadioClient::State state)
{
    switch (state) {
        using enum RadioClient::State;

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
        case in_progress:
            return "in_progress"s;
        case paused:
            return "paused"s;
        case unpausing:
            return "unpausing"s;
        case error:
            return "error"s;
        default:
            return "<UNKNOWN>"s;
    }
}


std::string
to_string(RadioClient::ThreadState state)
{
    switch (state) {
        using enum RadioClient::ThreadState;

        case stopped:
            return "stopped"s;
        case queued:
            return "queued"s;
        case running:
            return "running"s;
        case stopping:
            return "stopping"s;
        default:
            return "<UNKNOWN>"s;
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


std::string
to_label(RadioClient::URLType type)
{
    switch (type) {
        using enum RadioClient::URLType;

        case unknown:
            return ICON_FA_QUESTION_CIRCLE;
        case playlist:
            return ICON_FA_LIST_OL;
        case audio:
            return ICON_FA_FILE_AUDIO_O;
        case other:
            return ICON_FA_EXCLAMATION_TRIANGLE;
        default:
            return ICON_FA_BAN;
    }
}
