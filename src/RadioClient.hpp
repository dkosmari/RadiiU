/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef RADIO_CLIENT_HPP
#define RADIO_CLIENT_HPP

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <curlxx/easy.hpp>
#include <curlxx/error.hpp>
#include <curlxx/multi.hpp>

#include <sdl2xx/audio.hpp>

#include "async_byte_stream.hpp"
#include "async_task_queue.hpp"
#include "byte_stream.hpp"
#include "Decoder.hpp"
#include "icy_parser.hpp"
#include "stream_metadata.hpp"
#include "thread_safe.hpp"


// This class is the high-level handler for internet radio streams.

class RadioClient {

    struct APDDisabler {
        APDDisabler()
            noexcept;

        ~APDDisabler()
            noexcept;
    };


public:

    enum class State {
        stopped,
        fetching,
        streaming,
        error,
    };


    enum class TransferState {
        idle,
        queued,
        in_progress,
        paused,
        unpausing,
        error,
    };


    enum class ThreadState {
        stopped,
        queued,
        running,
        stopping,
    };


    enum class URLType {
        unknown,
        playlist,
        audio,
        other,
    };


    enum class PlaylistType {
        none,
        asx,                    // TODO
        cue,                    // TODO
        jspf,                   // TODO text/json
        m3u,
        pls,
        wpl,                    // TODO
        xspf,                   // TODO application/xspf+xml
    };


    struct URLInfo {
        std::string url{};
        std::string content_type{};
        URLType type = URLType::unknown;
    };

    using URLInfoVec = std::vector<URLInfo>;


    using MetadataCallbackSignature = void (const stream_metadata& meta);
    using MetadataFunc = std::function<MetadataCallbackSignature>;


    using DecoderInfoCallbackSignature = void (const Decoder::Info& info);
    using DecoderInfoFunc = std::function<DecoderInfoCallbackSignature>;


    using URLInfoVecCallbackSignature = void (const URLInfoVec& info_vec);
    using URLInfoVecFunction = std::function<URLInfoVecCallbackSignature>;


    using ErrorMessageCallbackSignature = void (const std::string& msg);
    using ErrorMessageFunction = std::function<ErrorMessageCallbackSignature>;


    RadioClient(const std::string& user_agent_,
                const std::string& station_url_,
                std::initializer_list<std::string_view> extra_urls = {});

    ~RadioClient()
        noexcept;

        // This is called by the "Player" tab, once per iteration of the main app loop.
    void
    process();

    // Enter the "fetching" state, starting from the given URL index.
    void
    play(unsigned idx = 0);

    void
    stop();


    bool
    is_playing()
        const noexcept;


    bool
    is_error()
        const noexcept;


    void
    with_metadata(const MetadataFunc& func)
        const;


    void
    with_decoder_info(const DecoderInfoFunc& func)
        const;


    void
    with_url_info_vec(const URLInfoVecFunction& func)
        const;


    void
    with_error_message(const ErrorMessageFunction& func)
        const;


    unsigned
    get_current_url_info_index()
        const;


    State
    get_state()
        const;


    TransferState
    get_transfer_state()
        const;


    ThreadState
    get_audio_thread_state()
        const;


    ThreadState
    get_network_thread_state()
        const;


private:

    const std::string user_agent;
    const std::string station_url;

    std::atomic<State> state = State::stopped;

    thread_safe<URLInfoVec> safe_url_info_vec;
    std::atomic<unsigned> url_info_idx{0};

    async_byte_stream encoded_audio{256 * 1024};

    thread_safe<stream_metadata> safe_metadata; // updated by either thread, read by the UI code

    thread_safe<std::string> safe_error_message;

    std::optional<APDDisabler> apd_disabler;

    // async_task_queue main_tasks; // dispatched from .process()

    struct network_t {
        std::atomic<ThreadState> thread_state = ThreadState::stopped;
        std::atomic<TransferState> transfer_state = TransferState::idle;
        curl::multi multi{nullptr};
        curl::easy easy{nullptr};
        PlaylistType playlist_type = PlaylistType::none;
        byte_stream playlist_stream{2 * 1024 * 1024};
        icy::parser icy_parser;
        async_task_queue tasks;
        std::vector<std::byte> buffer{64 * 1024};
        std::jthread thread;
    } network;

    struct audio_t {
        std::atomic<ThreadState> thread_state = ThreadState::stopped;
        std::unique_ptr<Decoder> decoder;
        sdl::audio::stream converter{nullptr};
        thread_safe<Decoder::Info> safe_info;
        thread_safe<std::string> safe_content_type;
        std::vector<std::byte> buffer{64 * 1024};
        std::jthread thread;
    } audio;



    void
    add_url(const std::string& url);


    void
    network_thread_function(std::stop_token stopper)
        noexcept;

    void
    audio_thread_function(std::stop_token stopper)
        noexcept;


    void
    task_queue_transfer(unsigned idx);


    void
    task_cancel_transfer();


    void
    task_resume_transfer();


    void
    on_transfer_error(const curl::error& e);

    void
    on_transfer_finished();

    std::size_t
    on_transfer_recv(std::span<const std::byte> data);

    void
    on_transfer_started(std::span<const std::byte> data);


    void
    start_network_thread();

    void
    start_audio_thread();


    void
    resume_transfer_if_paused();


    void
    process_playlist();

    void
    try_next_url();

}; // struct RadioClient


std::string
to_string(RadioClient::State state);


std::string
to_string(RadioClient::PlaylistType type);


std::string
to_string(RadioClient::TransferState state);

std::string
to_string(RadioClient::ThreadState state);

std::string
to_string(RadioClient::URLType type);


std::string
to_label(RadioClient::URLType type);

#endif
