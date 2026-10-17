/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <iterator>

#include "byte_stream.hpp"


byte_stream::byte_stream(std::size_t max_size_) :
    max_size{max_size_}
{}


void
byte_stream::clear()
{
    data.clear();
    std::vector<std::byte> dummy;
    consume_buffer.swap(dummy);
}


bool
byte_stream::empty()
    const noexcept
{
    return data.empty();
}


std::size_t
byte_stream::size()
    const noexcept
{
    return data.size();
}


std::span<std::byte>
byte_stream::read(std::span<std::byte> dst)
    noexcept
{
    std::size_t count = std::min(data.size(), dst.size());

    auto [data_end, dst_end] =
        std::ranges::copy_n(data.begin(),
                            count,
                            dst.begin());
    data.erase(data.begin(), data_end);

    return {dst.begin(), dst_end};
}


std::span<std::byte>
byte_stream::peek(std::span<std::byte> dst)
    const noexcept
{
    std::size_t count = std::min(data.size(), dst.size());

    auto [data_end, dst_end] =
        std::ranges::copy_n(data.begin(),
                            count,
                            dst.begin());

    return {dst.begin(), dst_end};
}


std::size_t
byte_stream::discard(std::size_t count)
    noexcept
{
    if (count > 0) {
        if (count > data.size())
            count = data.size();

        auto new_begin = std::next(data.begin(), count);
        data.erase(data.begin(), new_begin);
    }

    return count;
}


std::span<const std::byte>
byte_stream::write(std::span<const std::byte> src)
{
    // limit check
    if (max_size && data.size() + src.size() > max_size) [[unlikely]]
        return {};

    data.append_range(src);

    return src;
}


std::span<const std::byte>
byte_stream::write_some(std::span<const std::byte> src)
{
    std::size_t count = src.size();
    // limit check
    if (max_size && data.size() + count > max_size) [[unlikely]]
        count = max_size - data.size();

    return write(src.first(count));
}


std::size_t
byte_stream::consume(byte_stream& src)
{
    if (this == &src)
        return 0;

    if (consume_buffer.empty())
        consume_buffer.resize(4 * 1024);

    std::size_t consumed = 0;
    while (!src.empty()) {
        auto chunk = src.peek(consume_buffer);
        auto written = write_some(chunk);
        src.discard(written.size());
        consumed += written.size();
        // limit check
        if (max_size && data.size() >= max_size) [[unlikely]]
            break;
    }
    return consumed;
}


std::size_t
byte_stream::consume(byte_stream& src,
                     std::size_t count)
{
    if (this == &src)
        return 0;

    if (consume_buffer.empty())
        consume_buffer.resize(4 * 1024);

    std::size_t consumed = 0;
    while (!src.empty() && consumed < count) {
        const std::size_t pending = count - consumed;
        std::size_t read_size = std::min(consume_buffer.size(), pending);
        auto chunk = src.peek(std::span{consume_buffer.data(), read_size});
        auto written = write_some(chunk);
        src.discard(written.size());
        consumed += written.size();
        // limit check
        if (max_size && data.size() >= max_size) [[unlikely]]
            break;
    }
    return consumed;
}
