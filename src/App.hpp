/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APP_HPP
#define APP_HPP

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <span>

#include <sdl2xx/audio.hpp>

#include "TabID.hpp"


namespace App {

    using Function = std::move_only_function<void()>;


    [[nodiscard]]
    const std::string&
    get_user_agent();


    [[nodiscard]]
    const std::filesystem::path&
    get_content_dir();


    [[nodiscard]]
    const std::filesystem::path&
    get_config_dir();


    [[nodiscard]]
    const std::filesystem::path&
    get_data_dir();


    void
    initialize();

    void
    finalize();


    void
    run();


    void
    quit();


    void
    set_tab(TabID id);


    // Callbacks are called once every time around the main loop.

    void
    add_callback(std::string_view name,
                 Function func);


    // Tasks are called once, from the main loop.

    void
    add_task_real(std::string_view name,
                  Function func);

    template<typename F,
             typename... Args>
    inline
    void
    add_task(std::string_view name,
             F&& func,
             Args&&... args)
    {
        add_task_real(
            name,
            [
                func = std::forward<F>(func),
                ... args = std::forward<Args>(args)
            ]
                mutable
            {
                std::invoke(func, args...);
            }
        );
    }


    struct audio_spec {
        sdl::audio::format format{};
        unsigned channels{};
        unsigned rate{};
    };


    audio_spec
    get_audio_spec()
        noexcept;


    void
    play_audio(std::span<const std::byte> samples);


    void
    clear_audio()
        noexcept;


    std::size_t
    get_audio_queued_size()
        noexcept;

} // namespace App

#endif
