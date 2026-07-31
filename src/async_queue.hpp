/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ASYNC_QUEUE_HPP
#define ASYNC_QUEUE_HPP

#include <condition_variable>
#include <expected>
#include <mutex>
#include <queue>
#include <stop_token>
#include <utility>              // forward(), move()


enum class async_queue_error {
    stop,
    empty,
    locked,
};


template<typename T,
         typename M = std::mutex,
         typename Q = std::queue<T>>
class async_queue {

    M mutex;
    std::condition_variable_any empty_cond;
    Q queue;
    bool should_stop = false;

public:

    async_queue() = default;


    template<typename... Args>
    explicit
    async_queue(Args&&... args) :
        queue(std::forward<Args>(args)...)
    {}


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
        empty_cond.notify_all(); // make sure all threads can see should_stop changed.
    }


    bool
    is_stopping()
        const
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


    void
    clear()
    {
        std::lock_guard guard{mutex};
        queue.clear();
    }


    template<typename U>
    void
    push(U&& x)
    {
        std::lock_guard guard{mutex};
        queue.push(std::forward<U>(x));
        empty_cond.notify_one();
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


    // Block until data is available to pop, or until a stop is requested.
    T
    pop()
    {
        std::unique_lock guard{mutex};
        // Wait until a stop is requested, or the queue has data.
        empty_cond.wait(
            guard,
            [this]
            {
                return should_stop || !queue.empty();
            }
        );

        if (should_stop)
            throw async_queue_error::stop;

        T result = std::move(queue.front());
        queue.pop();
        return result;
    }


    // Overload taking a stop_token to unblock.
    T
    pop(std::stop_token& stopper)
    {
        std::unique_lock guard{mutex};
        // Wait until a stop is requested, or the queue has data.
        empty_cond.wait(
            guard,
            stopper,
            [this]
            {
                return should_stop || !queue.empty();
            }
        );

        if (stopper.stop_requested() || should_stop)
            throw async_queue_error::stop;

        T result = std::move(queue.front());
        queue.pop();
        return result;
    }


    // Block until there's data to pop, or a stop is requested.
    std::expected<T, async_queue_error>
    try_pop_block(std::stop_token stopper)
        noexcept
    {
        std::unique_lock guard{mutex};
        // Wait until a stop is requested, or the queue has data.
        empty_cond.wait(
            guard,
            stopper,
            [this]
            {
                return should_stop || !queue.empty();
            }
        );

        if (stopper.stop_requested() || should_stop)
            return std::unexpected{async_queue_error::stop};

        T result = std::move(queue.front());
        queue.pop();
        return result;
    }


    // Return immediately, with popped value, or an error.
    std::expected<T, async_queue_error>
    try_pop()
        noexcept
    {
        std::unique_lock guard{mutex, std::try_to_lock};
        if (!guard)
            return std::unexpected{async_queue_error::locked};

        if (should_stop)
            return std::unexpected{async_queue_error::stop};

        if (queue.empty())
            return std::unexpected{async_queue_error::empty};

        T result = std::move(queue.front());
        queue.pop();
        return result;
    }


    // Block up to the specified timeout, return the popped value, or an error.
    template<typename Rep,
             typename Period>
    std::expected<T, async_queue_error>
    pop_for(const std::chrono::duration<Rep, Period>& timeout)
        noexcept
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        std::unique_lock guard{mutex, deadline};
        if (!guard)
            return std::unexpected{async_queue_error::locked};

        // Wait until a stop is requested, or the queue has data.
        empty_cond.wait_until(
            guard,
            deadline,
            [this]
            {
                return should_stop || !queue.empty();
            }
        );

        if (should_stop)
            return std::unexpected{async_queue_error::stop};

        if (queue.empty())
            return std::unexpected{async_queue_error::empty};

        T result = std::move(queue.front());
        queue.pop();
        return result;
    }


    // Overload for stop_token.
    template<typename Rep,
             typename Period>
    std::expected<T, async_queue_error>
    pop_for(std::stop_token& stopper,
            const std::chrono::duration<Rep, Period>& timeout)
        noexcept
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        std::unique_lock guard{mutex, deadline};
        if (!guard)
            return std::unexpected{async_queue_error::locked};

        // Wait until a stop is requested, or the queue has data.
        empty_cond.wait_until(
            guard,
            stopper,
            deadline,
            [this]
            {
                return should_stop || !queue.empty();
            }
        );

        if (stopper.stop_requested() || should_stop)
            return std::unexpected{async_queue_error::stop};

        if (queue.empty())
            return std::unexpected{async_queue_error::empty};

        T result = std::move(queue.front());
        queue.pop();
        return result;
    }

}; // class async_queue

#endif
