/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ASYNC_TASK_QUEUE_HPP
#define ASYNC_TASK_QUEUE_HPP

#include <cstddef>
#include <future>
#include <utility>
#include <stop_token>
#include <utility>

#include "async_queue.hpp"


struct async_task_queue {

    using task_t = std::future<void>;
    using queue_t = async_queue<task_t>;


    async_task_queue()
        noexcept = default;

    template<typename... Args>
    explicit
    async_task_queue(Args&&... args) :
        queue(std::forward<Args>(args)...)
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
    add(F&& func,
        Args&&... args)
    {
        queue.push(std::async(std::launch::deferred,
                              std::forward<F>(func),
                              std::forward<Args>(args)...));
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

    queue_t queue;

}; // task_queue



#endif
