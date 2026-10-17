/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BYTE_STREAM_HPP
#define BYTE_STREAM_HPP

#include <cstddef>
#include <deque>
#include <span>
#include <utility>
#include <vector>


class byte_stream {

    std::deque<std::byte> data;
    const std::size_t max_size = 0;
    std::vector<std::byte> consume_buffer;

public:

    byte_stream(std::size_t max_size_ = 0);


    void
    clear();


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

    template<typename C>
    [[nodiscard]]
    C
    read_all_as()
    {
        C result;
        result.resize(size());
        std::ignore = read(as_writable_bytes(std::span{result}));
        return result;
    }


    [[nodiscard]]
    std::span<std::byte>
    peek(std::span<std::byte> dst)
        const noexcept;


    std::size_t
    discard(std::size_t count)
        noexcept;


    // Write all or nothing.

    std::span<const std::byte>
    write(std::span<const std::byte> src);


    // Write as much as possible.

    std::span<const std::byte>
    write_some(std::span<const std::byte> src);


    std::size_t
    consume(byte_stream& src);

    std::size_t
    consume(byte_stream& src,
            std::size_t count);

}; // class byte_stream

#endif
