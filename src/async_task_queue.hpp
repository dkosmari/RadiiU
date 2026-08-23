/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ASYNC_TASK_QUEUE_HPP
#define ASYNC_TASK_QUEUE_HPP

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <utility>

#include "async_queue.hpp"


struct async_task_queue {

    struct error : std::runtime_error {

        std::string name;

        error(const std::string& name,
              const char* message);

        error(const std::string& name,
              const std::string& message);

    }; // struct error


    struct call_again {};


    struct task_type {

        using function_type = std::move_only_function<void()>;

        std::string name;
        function_type function;

    }; // struct task_type


    using queue_type = async_queue<task_type>;


    async_task_queue()
        noexcept = default;

    template<typename... Args>
    explicit
    async_task_queue(Args&&... args) :
        tasks(std::forward<Args>(args)...)
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
    add(const std::string& name,
        F&& func,
        Args&&... args)
    {
        tasks.emplace(
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


    bool
    dispatch_one();

    bool
    dispatch_one(std::stop_token& stopper);


    bool
    try_dispatch_one();


    std::size_t
    dispatch_all();

    std::size_t
    dispatch_all(std::stop_token& stopper);


    std::size_t
    try_dispatch_all();


private:

    queue_type tasks;
    queue_type deferred_tasks;


    void
    promote_deferred_tasks();

}; // task_queue



#endif
