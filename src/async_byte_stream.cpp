/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <iterator>

#include "async_byte_stream.hpp"


async_byte_stream::async_byte_stream(std::size_t max_size_) :
    max_size{max_size_}
{}


async_byte_stream::async_byte_stream(std::size_t max_size_,
                                     std::span<const std::byte> initial_data) :
    data{initial_data.begin(), initial_data.end()},
    max_size{max_size_}
{}


async_byte_stream::~async_byte_stream()
    noexcept
{
    close();
}


void
async_byte_stream::close()
    noexcept
{
    std::lock_guard guard{mutex};
    closed = true;
    empty_cond.notify_all();
}


bool
async_byte_stream::is_eof()
    const noexcept
{
    std::lock_guard guard{mutex};
    return data.empty() && closed;
}


void
async_byte_stream::clear()
    noexcept
{
    std::lock_guard guard{mutex};
    data.clear();
    closed = false;
}


bool
async_byte_stream::empty()
    const noexcept
{
    std::lock_guard guard{mutex};
    return data.empty();
}


std::size_t
async_byte_stream::size()
    const noexcept
{
    std::lock_guard guard{mutex};
    return data.size();
}


std::span<std::byte>
async_byte_stream::read(std::span<std::byte> dst)
    noexcept
{
    if (dst.empty())
        return {};

    std::unique_lock lock{mutex};

    empty_cond.wait(
        lock,
        [this]
        {
            return !data.empty() || closed;
        }
    );
    std::size_t count = std::min(data.size(), dst.size());
    auto [data_end, dst_end] =
        std::ranges::copy_n(data.begin(),
                            count,
                            dst.begin());
    data.erase(data.begin(), data_end);

    return {dst.begin(), dst_end};
}



std::span<std::byte>
async_byte_stream::read(std::span<std::byte> dst,
                        std::stop_token& stoken)
    noexcept
{
    if (dst.empty())
        return {};

    std::unique_lock lock{mutex};

    empty_cond.wait(
        lock,
        stoken,
        [this]
        {
            return !data.empty() || closed;
        }
    );

    if (stoken.stop_requested())
        return {};

    std::size_t count = std::min(data.size(), dst.size());
    auto [data_end, dst_end] =
        std::ranges::copy_n(data.begin(),
                            count,
                            dst.begin());
    data.erase(data.begin(), data_end);

    return {dst.begin(), dst_end};
}


std::span<std::byte>
async_byte_stream::read_all(std::span<std::byte> dst)
    noexcept
{
    const std::size_t requested = dst.size();
    std::size_t total = 0;
    auto orig_dst = dst;

    std::unique_lock lock{mutex};

    while (total < requested) {

        empty_cond.wait(
            lock,
            [this]
            {
                return !data.empty() || closed;
            }
        );

        if (data.empty() && closed)
            break;

        std::size_t count = std::min(data.size(), dst.size());
        auto [data_end, dst_end] =
            std::ranges::copy_n(data.begin(),
                                count,
                                dst.begin());
        data.erase(data.begin(), data_end);
        dst = dst.subspan(count);
        total += count;

    }

    return orig_dst.subspan(0, total);
}


std::span<std::byte>
async_byte_stream::read_all(std::span<std::byte> dst,
                            std::stop_token& stoken)
    noexcept
{
    const std::size_t requested = dst.size();
    std::size_t total = 0;
    auto orig_dst = dst;

    std::unique_lock lock{mutex};

    while (total < requested) {

        empty_cond.wait(
            lock,
            stoken,
            [this]
            {
                return !data.empty() || closed;
            }
        );

        if (stoken.stop_requested())
            break;
        if (data.empty() && closed)
            break;

        std::size_t count = std::min(data.size(), dst.size());
        auto [data_end, dst_end] =
            std::ranges::copy_n(data.begin(),
                                count,
                                dst.begin());
        data.erase(data.begin(), data_end);
        dst = dst.subspan(count);
        total += count;

    }

    return orig_dst.subspan(0, total);
}


std::span<std::byte>
async_byte_stream::peek(std::span<std::byte> dst)
    const noexcept
{
    if (dst.empty())
        return {};

    std::lock_guard guard{mutex};

    std::size_t count = std::min(data.size(), dst.size());

    auto [data_end, dst_end] =
        std::ranges::copy_n(data.begin(),
                            count,
                            dst.begin());
    return {dst.begin(), dst_end};
}


std::size_t
async_byte_stream::discard(std::size_t count)
    noexcept
{
    if (count > 0) {
        std::lock_guard guard{mutex};

        if (count > data.size())
            count = data.size();

        auto new_begin = std::next(data.begin(), count);
        data.erase(data.begin(), new_begin);
    }

    return count;
}



std::span<const std::byte>
async_byte_stream::write(std::span<const std::byte> src)
{
    if (src.empty())
        return src;

    std::lock_guard guard{mutex};

    if (closed)
        return {};

    if (max_size && src.size() + data.size() > max_size)
        return {};

    data.append_range(src);
    empty_cond.notify_one();

    return src;
}
