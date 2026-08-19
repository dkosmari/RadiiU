/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TASK_QUEUE_HPP
#define TASK_QUEUE_HPP

#include <cstddef>
#include <future>
#include <queue>
#include <utility>


struct task_queue {

    using task_t = std::future<void>;
    using queue_t = std::queue<task_t>;


    task_queue()
        noexcept = default;

    template<typename... Args>
    explicit
    task_queue(Args&&... args) :
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


    std::size_t
    dispatch_all();


private:

    queue_t queue;

}; // task_queue

#endif
