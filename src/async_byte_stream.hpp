/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ASYNC_BYTE_STREAM_HPP
#define ASYNC_BYTE_STREAM_HPP

#include <mutex>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <deque>

class async_byte_stream {

    mutable std::mutex mutex;
    std::condition_variable_any empty_cond;

    std::deque<std::byte> data;
    const std::size_t max_size;
    bool closed = false;

public:

    async_byte_stream(std::size_t max_size_ = 0);

    async_byte_stream(std::size_t max_size_,
                      std::span<const std::byte> initial_data);

    ~async_byte_stream()
        noexcept;

    void
    close()
        noexcept;

    [[nodiscard]]
    bool
    is_eof()
        const noexcept;

    void
    clear()
        noexcept;

    [[nodiscard]]
    bool
    empty()
        const noexcept;

    [[nodiscard]]
    std::size_t
    size()
        const noexcept;

    [[nodiscard]]
    std::span<std::byte>
    read(std::span<std::byte> dst)
        noexcept;

    [[nodiscard]]
    std::span<std::byte>
    read(std::span<std::byte> dst,
         std::stop_token& stopper)
        noexcept;


    [[nodiscard]]
    std::span<std::byte>
    read_all(std::span<std::byte> dst)
        noexcept;

    [[nodiscard]]
    std::span<std::byte>
    read_all(std::span<std::byte> dst,
             std::stop_token& stopper)
        noexcept;


    [[nodiscard]]
    std::span<std::byte>
    peek(std::span<std::byte> dst)
        const noexcept;


    std::size_t
    discard(std::size_t count)
        noexcept;

    std::span<const std::byte>
    write(std::span<const std::byte> src);

}; // class async_byte_stream

#endif
