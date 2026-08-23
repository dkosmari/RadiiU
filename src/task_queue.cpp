/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "task_queue.hpp"


task_queue::error::error(const std::string& name_,
             const char* message) :
    std::runtime_error{message},
    name{name_}
{}


task_queue::error::error(const std::string& name_,
                         const std::string& message) :
    std::runtime_error{message},
    name{name_}
{}


void
task_queue::clear()
    noexcept
{
    while (!tasks.empty())
        tasks.pop();
    while (!deferred_tasks.empty())
        deferred_tasks.pop();
}


bool
task_queue::empty()
    const noexcept
{
    return tasks.empty() && deferred_tasks.empty();
}


std::size_t
task_queue::size()
    const noexcept
{
    return tasks.size() + deferred_tasks.size();
}


bool
task_queue::dispatch_one()
{
    if (!tasks.empty()) {
        auto t = std::move(tasks.front());
        tasks.pop();

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


std::size_t
task_queue::dispatch_all()
{
    std::size_t result = 0;
    while (dispatch_one())
        ++result;
    return result;
}


void
task_queue::promote_deferred_tasks()
{
    // When no more tasks, transfer deferred_tasks to tasks.
    while (!deferred_tasks.empty()) {
        tasks.push(std::move(deferred_tasks.front()));
        deferred_tasks.pop();
    }
}
