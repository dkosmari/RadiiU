/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ASYNC_TASK_QUEUE_HPP
#define ASYNC_TASK_QUEUE_HPP

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>


struct async_task_queue {

    struct error : std::runtime_error {

        std::string name;

        error(std::string_view name_,
              const char* message);

        error(std::string_view name_,
              const std::string& message);

    }; // struct error


    struct call_again {};


    struct task_type {

        using function_type = std::move_only_function<void()>;

        std::string_view name;
        function_type function;

    }; // struct task_type


    using queue_type = std::deque<task_type>;


    async_task_queue()
        noexcept = default;

    template<typename... Args>
    explicit
    async_task_queue(Args&&... args) :
        queued_tasks(std::forward<Args>(args)...)
    {}


    void
    clear()
        noexcept;


    bool
    empty()
        const noexcept;


    std::size_t
    size()
        const noexcept;


    template<typename F,
             typename... Args>
    void
    add(std::string_view name,
        F&& func,
        Args&&... args)
    {
        std::lock_guard guard{queued_tasks_mutex};

        queued_tasks.emplace_back(
            name,
            [
                func = std::forward<F>(func),
                ... args = std::forward<Args>(args)
            ]
                mutable
            {
                func(std::move(args)...);
            }
        );
    }


    bool
    dispatch_one();


    bool
    try_dispatch_one();


    std::size_t
    dispatch_all();


    std::size_t
    try_dispatch_all();


private:

    mutable std::mutex queued_tasks_mutex;
    queue_type queued_tasks;

    queue_type deferred_tasks;
    queue_type dispatch_tasks;


    void
    try_requeue_deferred_tasks();

}; // task_queue



#endif
