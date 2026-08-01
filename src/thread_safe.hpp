/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef THREAD_SAFE_HPP
#define THREAD_SAFE_HPP

#include <mutex>
#include <type_traits>
#include <utility>


template<typename T,
         typename M = std::mutex>
class thread_safe {

    mutable M mutex;
    T data;

public:

    using mutex_type = M;
    using data_type = T;


    template<typename U>
    class guard {

        std::unique_lock<mutex_type> guard_;
        U* data_ = nullptr;

        guard(mutex_type& m,
              U* d) :
            guard_{m},
            data_{d}
        {}

        guard(std::unique_lock<mutex_type>&& locker,
              U* d) :
            guard_{std::move(locker)},
            data_{d}
        {}

        friend class thread_safe<std::remove_cv_t<U>, mutex_type>;

    public:

        guard(guard&& other) :
            guard_{std::move(other.guard_)},
            data_{other.data_}
        {
            other.data_ = nullptr;
        }

        guard&
        operator =(guard&& other)
            noexcept
        {
            if (this != other) {
                guard_ = std::move(other.guard_);
                data_ = other.data_;
                other.data_ = nullptr;
            }
            return *this;
        }

        U&
        operator *()
            noexcept
        {
            return *data_;
        }

        U*
        operator ->()
            const noexcept
        {
            return data_;
        }

        explicit
        operator bool()
            const noexcept
        {
            return guard_.owns_lock();
        }

    }; // class guard


    // thread_safe() = default;


    template<typename... Args>
    thread_safe(Args&& ...args) :
        data(std::forward<Args>(args)...)
    {}


    [[nodiscard]]
    guard<data_type>
    lock()
        &
    {
        return guard<data_type>{mutex, &data};
    }


    [[nodiscard]]
    guard<const data_type>
    lock()
        const &
    {
        return guard<const data_type>{mutex, &data};
    }


    [[nodiscard]]
    guard<const data_type>
    c_lock()
        const &
    {
        return guard<const data_type>{mutex, &data};
    }


    [[nodiscard]]
    guard<data_type>
    try_lock()
        &
    {
        return {std::unique_lock{mutex, std::try_to_lock}, &data};
    }


    [[nodiscard]]
    data_type
    load()
        const &
    {
        return *lock();
    }


    template<typename U>
    void
    store(U&& new_data)
        &
    {
        *lock() = std::forward<U>(new_data);
    }

}; // class thread_safe<T, M>

#endif
