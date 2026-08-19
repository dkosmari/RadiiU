/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "task_queue.hpp"


void
task_queue::clear()
    noexcept
{
    while (!queue.empty())
        queue.pop();
}


bool
task_queue::empty()
    const noexcept
{
    return queue.empty();
}


std::size_t
task_queue::size()
    const noexcept
{
    return queue.size();
}


bool
task_queue::dispatch_one()
{
    if (!empty()) {
        auto task = std::move(queue.front());
        queue.pop();
        task.get();
        return true;
    }
    return false;
}


std::size_t
task_queue::dispatch_all()
{
    std::size_t result = 0;
    while (dispatch_one())
        ++result;
    return result;
}
