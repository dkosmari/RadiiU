/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APP_HPP
#define APP_HPP

#include <filesystem>
#include <functional>
#include <string>
#include <utility>

#include "TabID.hpp"


namespace App {

    using Function = std::move_only_function<void()>;


    [[nodiscard]]
    const std::string&
    get_user_agent();


    [[nodiscard]]
    const std::filesystem::path&
    get_content_path();


    [[nodiscard]]
    const std::filesystem::path&
    get_config_path();


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
    add_callback(const std::string& name,
                 Function func);


    // Tasks are called once, from the main loop.

    void
    add_task_real(const std::string& name,
                  Function func);

    template<typename F,
             typename... Args>
    inline
    void
    add_task(const std::string& name,
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

} // namespace App

#endif
