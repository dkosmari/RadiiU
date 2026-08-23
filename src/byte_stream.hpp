/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BYTE_STREAM_HPP
#define BYTE_STREAM_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>


class byte_stream {

    std::deque<std::byte> data;

public:

    void
    clear();


    bool
    empty() const noexcept;

    std::size_t
    size() const noexcept;


    std::size_t
    read(void* buf,
         std::size_t count)
        noexcept;

    template<typename T,
             std::size_t E>
    requires(sizeof(T) == 1)
    [[nodiscard]]
    std::span<const T>
    read(std::span<T, E> buf)
        noexcept;

    [[nodiscard]]
    std::vector<std::byte>
    read();

    [[nodiscard]]
    std::vector<std::byte>
    read(std::size_t count);


    template<typename T>
    requires(sizeof(T) == 1)
    [[nodiscard]]
    std::vector<T>
    read_as(std::size_t count);

    template<typename T>
    requires(sizeof(T) == 1)
    [[nodiscard]]
    std::vector<T>
    read_as();


    [[nodiscard]]
    std::string
    read_str(std::size_t count);

    [[nodiscard]]
    std::string
    read_str();


    [[nodiscard]]
    std::size_t
    peek(void* buf,
         std::size_t count)
        const noexcept;

    template<typename T,
             std::size_t E>
    requires(sizeof(T) == 1)
    [[nodiscard]]
    std::span<const T>
    peek(std::span<T, E> buf)
        const noexcept;


    std::size_t
    discard(std::size_t count)
        noexcept;


    std::optional<std::uint8_t>
    try_load_u8();


    std::size_t
    write(const void* buf,
          std::size_t size);

    template<typename T,
             std::size_t E>
    std::size_t
    write(std::span<T, E> buf);

    std::size_t
    write(std::string_view sv);


    std::size_t
    consume(byte_stream& other);

    std::size_t
    consume(byte_stream& other, std::size_t count);

}; // class byte_stream


/*--------------------*/
/* Inline definitions */
/*--------------------*/

template<typename T,
         std::size_t E>
requires(sizeof(T) == 1)
inline
std::span<const T>
byte_stream::read(std::span<T, E> buf)
    noexcept
{
    std::size_t valid = read(buf.data(), buf.size_bytes());
    return std::span{buf.data(), valid};
}


template<typename T>
requires(sizeof(T) == 1)
inline
std::vector<T>
byte_stream::read_as(std::size_t count)
{
    std::vector<T> result(count);
    auto occupied = read(std::span(result));
    result.resize(occupied.size());
    return result;
}


template<typename T>
requires(sizeof(T) == 1)
inline
std::vector<T>
byte_stream::read_as()
{
    return read_as<T>(size());
}


template<typename T,
         std::size_t E>
requires(sizeof(T) == 1)
inline
std::span<const T>
byte_stream::peek(std::span<T, E> buf)
    const noexcept
{
    auto valid = peek(buf.data(), buf.size_bytes());
    return std::span{buf.data(), valid};
}


template<typename T,
         std::size_t E>
inline
std::size_t
byte_stream::write(std::span<T, E> buf)
{
    return write(buf.data(), buf.size_bytes());
}

#endif
