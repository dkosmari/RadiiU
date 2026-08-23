/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "async_task_queue.hpp"


async_task_queue::error::error(const std::string& name_,
                               const char* message) :
    std::runtime_error{message},
    name{name_}
{}


async_task_queue::error::error(const std::string& name_,
                               const std::string& message) :
    std::runtime_error{message},
    name{name_}
{}


void
async_task_queue::clear()
    noexcept
{
    tasks.clear();
    deferred_tasks.clear();
}


bool
async_task_queue::empty()
    const noexcept
{
    return tasks.empty() && deferred_tasks.empty();
}


std::size_t
async_task_queue::size()
    const noexcept
{
    return tasks.size() + deferred_tasks.size();
}


bool
async_task_queue::dispatch_one()
{
    if (!tasks.empty()) {
        auto t = tasks.pop();

        try {
            if (t.function)
                t.function();
        }
        catch (call_again&) {
            deferred_tasks.push(std::move(t));
        }
        catch (std::exception& e) {
            throw error{t.name, e.what()};
        }

        return true;
    } else
        promote_deferred_tasks();

    return false;
}


bool
async_task_queue::dispatch_one(std::stop_token& stopper)
{
    if (!tasks.empty()) {
        auto t = tasks.pop(stopper);

        try {
            if (t.function)
                t.function();
        }
        catch (call_again&) {
            deferred_tasks.push(std::move(t));
        }
        catch (std::exception& e) {
            throw error{t.name, e.what()};
        }

        return true;
    } else
        promote_deferred_tasks();

    return false;
}


bool
async_task_queue::try_dispatch_one()
{
    if (auto t = tasks.try_pop()) {
        try {
            if (t->function)
                t->function();
        }
        catch (call_again&) {
            deferred_tasks.push(std::move(*t));
        }
        catch (std::exception& e) {
            throw error{t->name, e.what()};
        }
        return true;
    } else if (t.error() == async_queue_error::empty)
        promote_deferred_tasks();

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


void
async_task_queue::promote_deferred_tasks()
{
    // When no more tasks, transfer deferred_tasks to tasks
    while (auto t = deferred_tasks.try_pop())
        tasks.push(std::move(*t));
}
