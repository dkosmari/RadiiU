/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APP_HPP
#define APP_HPP

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#include <sdl2xx/audio.hpp>

#include "TabID.hpp"


namespace App {

    using dbl_milliseconds = std::chrono::duration<double, std::milli>;

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


    bool
    is_running()
        noexcept;


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


    struct AudioSpec {
        sdl::audio::format format{};
        unsigned channels{};
        unsigned rate{};
    };


    std::optional<AudioSpec>
    get_audio_spec()
        noexcept;


    void
    play_music(std::span<const std::byte> samples,
               std::stop_token& stoken);


    void
    clear_music()
        noexcept;


    dbl_milliseconds
    get_queued_music_duration()
        noexcept;


    void
    detected_activity();

} // namespace App

#endif
