/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <print>

#include "LogManager.hpp"

#include "App.hpp"
#include "async_task_queue.hpp"
#include "thread_safe.hpp"
#include "tracer.hpp"


using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;


namespace LogManager {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        using MessageVec = std::deque<Message>;
        using SafeMessageVec = thread_safe<MessageVec, std::recursive_mutex>;


        /*-----------*/
        /* Constants */
        /*-----------*/

        const MessageVec::size_type max_messages = 1024;


        /*-----------*/
        /* Variables */
        /*-----------*/

        Timestamp timestamp;
        SafeMessageVec safe_messages;
        async_task_queue pending_tasks;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        std::string
        create_log_filename();

        void
        real_clear();

        void
        real_log(const Message& msg);

        void
        trim_messages(MessageVec& messages);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        std::string
        create_log_filename()
        {
            auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
            auto today = std::chrono::floor<std::chrono::days>(now);
            std::chrono::year_month_day date{today};
            std::chrono::hh_mm_ss time{now - today};
            return std::format("{:%F}-{:%H-%M-%S}.log", date, time);
        }


        void
        real_clear()
        {
            auto messages = safe_messages.lock();
            messages->clear();
            timestamp = 0;
        }


        void
        real_log(const Message& msg)
        {
            // TODO: set ANSI colors for each level
            std::println(cout,
                         "[LOG:{}] [{}:{}] {}\n{}",
                         msg.level,
                         msg.location.file_name(),
                         msg.location.line(),
                         msg.tag,
                         msg.text);

            auto messages = safe_messages.lock();
            messages->push_back(std::move(msg));
            ++timestamp;
            trim_messages(*messages);
        }


        void
        trim_messages(MessageVec& messages)
        {
            if (messages.size() > max_messages) {
                auto excess = messages.size() - max_messages;
                messages.erase(messages.begin(),
                               std::next(messages.begin(), excess));
            }
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize()
    {
        TRACE_FUNC;
    }


    void
    finalize()
    {
        TRACE_FUNC;
    }


    void
    clear()
    {
        pending_tasks.add(real_clear);
    }


    void
    for_each(const CallbackFunction& func)
    {
        for_each(LogLevel::debug, func);
    }


    void
    for_each(LogLevel min_level,
             const CallbackFunction& func)
    {
        auto messages = safe_messages.c_lock();
        for (auto& msg : *messages)
            if (msg.level >= min_level)
                func(msg);
    }


    Timestamp
    get_timestamp()
    {
        return timestamp;
    }


    void
    log(Message msg)
    {
        pending_tasks.add(real_log, std::move(msg));
    }


    void
    process()
    {
        try {
            pending_tasks.dispatch_all();
        }
        catch (std::exception& e) {
            cerr << "ERROR dispatching LogManager task: "<< e.what() << endl;
        }
    }


    void
    save()
    {
        TRACE_FUNC;

        auto cfg_path = App::get_config_path();
        auto logs_path = cfg_path / "logs";
        if (!exists(logs_path))
            create_directories(logs_path);

        auto filename = logs_path / create_log_filename();
        std::ofstream output{filename, std::ios::out | std::ios::trunc};

        auto messages = safe_messages.c_lock();
        for (auto& msg : *messages)
            std::println(output, "[{}] [{}:{}] {}: {}",
                         msg.level,
                         msg.location.file_name(),
                         msg.location.line(),
                         msg.tag,
                         msg.text);
    }

} // namespace LogManager
