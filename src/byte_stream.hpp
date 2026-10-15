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

    template<typename T,
             std::size_t E>
    requires(sizeof(T) == 1)
    [[nodiscard]]
    std::span<T>
    read(std::span<T, E> dst)
        noexcept;


    template<typename C>
    [[nodiscard]]
    C
    read_all_as()
    {
        C result;
        result.resize(size());
        std::ignore = read(std::span{result});
        return result;
    }


    [[nodiscard]]
    std::span<std::byte>
    peek(std::span<std::byte> dst)
        const noexcept;

    template<typename T,
             std::size_t E>
    requires(sizeof(T) == 1)
    [[nodiscard]]
    std::span<T>
    peek(std::span<T, E> buf)
        const noexcept;


    std::size_t
    discard(std::size_t count)
        noexcept;


    // Write all or nothing.

    std::span<const std::byte>
    write(std::span<const std::byte> src);


    template<typename T,
             std::size_t E>
    requires (sizeof(T) == 1)
    std::span<const T>
    write(std::span<const T, E> src);


    // Write as much as possible.

    std::span<const std::byte>
    write_some(std::span<const std::byte> src);

    template<typename T,
             std::size_t E>
    requires (sizeof(T) == 1)
    [[nodiscard]]
    std::span<const T>
    write_some(std::span<const T> src);


    std::size_t
    consume(byte_stream& src);

    std::size_t
    consume(byte_stream& src,
            std::size_t count);

}; // class byte_stream


/*--------------------*/
/* Inline definitions */
/*--------------------*/

template<typename T,
         std::size_t E>
requires(sizeof(T) == 1)
inline
std::span<T>
byte_stream::read(std::span<T, E> dst)
    noexcept
{
    auto result = read(std::as_writable_bytes(dst));
    return {
        reinterpret_cast<T*>(result.data()),
        result.size()
    };
}


template<typename T,
         std::size_t E>
requires(sizeof(T) == 1)
inline
std::span<T>
byte_stream::peek(std::span<T, E> dst)
    const noexcept
{
    auto result = peek(std::as_writable_bytes(dst));
    return {
        reinterpret_cast<T*>(result.data()),
        result.size()
    };
}


template<typename T,
         std::size_t E>
requires (sizeof(T) == 1)
inline
std::span<const T>
byte_stream::write(std::span<const T, E> src)
{
    auto result = write(as_bytes(src));
    return {
        reinterpret_cast<const T*>(result.data()),
        result.size()
    };
}


template<typename T,
         std::size_t E>
requires (sizeof(T) == 1)
std::span<const T>
byte_stream::write_some(std::span<const T> src)
{
    auto result = write_some(as_bytes(src));
    return {
        reinterpret_cast<const T*>(result.data()),
        result.size()
    };
}

#endif
