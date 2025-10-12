/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ASYNC_QUEUE_HPP
#define ASYNC_QUEUE_HPP

#include <condition_variable>
#include <expected>
#include <mutex>
#include <queue>
#include <utility>              // forward(), move()


enum class async_queue_exception {
    stop,
    empty,
    locked,
};


template<typename T,
         typename Q = std::queue<T>>
class async_queue {

    std::mutex mutex;
    std::condition_variable empty_cond;
    Q queue;
    bool should_stop = false;

public:

    // Makes the pool usable again after a stop().
    void
    reset()
    {
        std::lock_guard guard{mutex};
        should_stop = false;
    }


    // This will make all future pop() calls throw async_queue_exception::stop.
    // It also wakes up all threads waiting on empty_cond.
    void
    stop()
    {
        std::lock_guard guard{mutex};
        should_stop = true;
        empty_cond.notify_all(); // make sure all threads can see the updated flag
    }


    bool
    is_stopping() const
    {
        std::lock_guard guard{mutex};
        return should_stop;
    }


    bool
    empty()
        const
    {
        std::lock_guard guard{mutex};
        return queue.empty();
    }


    template<typename U>
    void
    push(U&& x)
    {
        std::lock_guard guard{mutex};
        queue.push(std::forward<U>(x));
        empty_cond.notify_one();
    }


    T
    pop()
    {
        std::unique_lock guard{mutex};
        // Wait until a stop is requested, or the queue has data.
        empty_cond.wait(guard, [this] { return should_stop || !queue.empty(); });
        if (should_stop)
            throw async_queue_exception::stop;
        T result = std::move(queue.front());
        queue.pop();
        return result;
    }


    template<typename U>
    bool
    try_push(U&& x)
    {
        std::unique_lock guard{mutex, std::try_to_lock};
        if (!guard)
            return false;
        queue.push(std::forward<U>(x));
        return true;
    }


    std::expected<T, async_queue_exception>
    try_pop()
        noexcept
    {
        std::unique_lock guard{mutex, std::try_to_lock};
        if (!guard)
            return std::unexpected{async_queue_exception::locked};
        if (should_stop)
            return std::unexpected{async_queue_exception::stop};
        if (queue.empty())
            return std::unexpected{async_queue_exception::empty};
        T result = std::move(queue.front());
        queue.pop();
        return result;
    }

};


#endif
