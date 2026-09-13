/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "async_task_queue.hpp"


async_task_queue::error::error(std::string_view name_,
                               const char* message) :
    std::runtime_error{message},
    name{name_}
{}


async_task_queue::error::error(std::string_view name_,
                               const std::string& message) :
    std::runtime_error{message},
    name{name_}
{}


void
async_task_queue::clear()
    noexcept
{
    std::lock_guard guard{queued_tasks_mutex};

    queued_tasks.clear();
    deferred_tasks.clear();
    dispatch_tasks.clear();
}


bool
async_task_queue::empty()
    const noexcept
{
    std::lock_guard guard{queued_tasks_mutex};

    return queued_tasks.empty()
        && deferred_tasks.empty()
        && dispatch_tasks.empty();
}


std::size_t
async_task_queue::size()
    const noexcept
{
    std::lock_guard guard{queued_tasks_mutex};

    return queued_tasks.size()
        + deferred_tasks.size()
        + dispatch_tasks.size();
}


bool
async_task_queue::dispatch_one()
{
    {
        std::lock_guard guard{queued_tasks_mutex};

        if (!queued_tasks.empty()) {
            auto t = std::move(queued_tasks.front());
            queued_tasks.pop_front();

            try {
                if (t.function)
                    t.function();
            }
            catch (call_again&) {
                deferred_tasks.push_back(std::move(t));
            }
            catch (std::exception& e) {
                throw error{t.name, e.what()};
            }

            return true;
        }
    }

    try_requeue_deferred_tasks();

    return false;
}


bool
async_task_queue::try_dispatch_one()
{
    {
        std::unique_lock lock{queued_tasks_mutex, std::try_to_lock};
        if (!lock)
            return false;

        if (!queued_tasks.empty()) {
            auto t = std::move(queued_tasks.front());
            queued_tasks.pop_front();

            try {
                if (t.function)
                    t.function();
            }
            catch (call_again&) {
                deferred_tasks.push_back(std::move(t));
            }
            catch (std::exception& e) {
                throw error{t.name, e.what()};
            }

            return true;
        }
    }

    try_requeue_deferred_tasks();

    return false;
}


std::size_t
async_task_queue::dispatch_all()
{
    {
        std::lock_guard guard{queued_tasks_mutex};
        queued_tasks.swap(dispatch_tasks);
    }

    std::size_t result = 0;

    try {
        while (!dispatch_tasks.empty()) {
            ++result;
            auto t = std::move(dispatch_tasks.front());
            dispatch_tasks.pop_front();
            try {
                if (t.function)
                    t.function();
            }
            catch (call_again&) {
                deferred_tasks.push_back(std::move(t));
            }
            catch (std::exception& e) {
                throw error{t.name, e.what()};
            }
        }
    }
    catch (...) {
        // defer all tasks that we failed to dispatch
        while (!dispatch_tasks.empty()) {
            deferred_tasks.push_back(std::move(dispatch_tasks.front()));
            dispatch_tasks.pop_front();
        }
    }

    try_requeue_deferred_tasks();

    return result;
}


std::size_t
async_task_queue::try_dispatch_all()
{
    {
        std::unique_lock lock{queued_tasks_mutex, std::try_to_lock};
        if (!lock)
            return 0;
        queued_tasks.swap(dispatch_tasks);
    }

    std::size_t result = 0;

    try {
        while (!dispatch_tasks.empty()) {
            ++result;
            auto t = std::move(dispatch_tasks.front());
            dispatch_tasks.pop_front();
            try {
                if (t.function)
                    t.function();
            }
            catch (call_again&) {
                deferred_tasks.push_back(std::move(t));
            }
            catch (std::exception& e) {
                throw error{t.name, e.what()};
            }
        }
    }
    catch (...) {
        // defer all tasks that we failed to dispatch
        while (!dispatch_tasks.empty()) {
            deferred_tasks.push_back(std::move(dispatch_tasks.front()));
            dispatch_tasks.pop_front();
        }
    }

    try_requeue_deferred_tasks();

    return result;
}


void
async_task_queue::try_requeue_deferred_tasks()
{
    std::unique_lock lock{queued_tasks_mutex, std::try_to_lock};
    if (!lock)
        return;

    while (!deferred_tasks.empty()) {
        queued_tasks.push_back(std::move(deferred_tasks.front()));
        deferred_tasks.pop_front();
    }
}
