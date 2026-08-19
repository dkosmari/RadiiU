/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "async_task_queue.hpp"


void
async_task_queue::clear()
    noexcept
{
    queue.clear();
}


bool
async_task_queue::empty()
    const noexcept
{
    return queue.empty();
}


std::size_t
async_task_queue::size()
    const noexcept
{
    return queue.size();
}


bool
async_task_queue::dispatch_one()
{
    if (!empty()) {
        auto task = queue.pop();
        task.get();
        return true;
    }
    return false;
}


bool
async_task_queue::dispatch_one(std::stop_token& stopper)
{
    if (!empty()) {
        auto task = queue.pop(stopper);
        task.get();
        return true;
    }
    return false;
}


bool
async_task_queue::try_dispatch_one()
{
    if (auto task = queue.try_pop()) {
        task->get();
        return true;
    }
    return false;
}


std::size_t
async_task_queue::dispatch_all()
{
    std::size_t result = 0;
    while (dispatch_one())
        ++result;
    return result;
}


std::size_t
async_task_queue::dispatch_all(std::stop_token& stopper)
{
    std::size_t result = 0;
    while (dispatch_one(stopper))
        ++result;
    return result;
}


std::size_t
async_task_queue::try_dispatch_all()
{
    std::size_t result = 0;
    while (try_dispatch_one())
        ++result;
    return result;
}
