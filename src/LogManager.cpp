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
#include <string_view>
#include <thread>

#include "LogManager.hpp"

#include "App.hpp"
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


        struct MessageAttributes {

            LogLevel level;
            std::source_location location;
            std::string tag;


            constexpr
            bool
            operator ==(const MessageAttributes& other)
                const noexcept;

        }; // struct MessageAttributes


        /*-----------*/
        /* Constants */
        /*-----------*/

        const MessageVec::size_type max_messages = 1024;

        const std::array log_level_ansi_prefixes = {
            "\e[3;93m",       // DEBUG
            "\e[1;7;34m",       // INFO
            "\e[1;7;33m",       // WARN
            "\e[1;7;31m"         // ERROR
        };

        const char* ansi_reset = "\e[0m";


        /*-----------*/
        /* Variables */
        /*-----------*/

        std::thread::id main_thread_id;
        Timestamp timestamp;
        SafeMessageVec safe_messages;

        std::optional<MessageAttributes> last_msg_attributes;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        std::string
        create_log_filename();

        void
        task_clear();

        void
        task_log(Message& msg);

        void
        trim_messages(MessageVec& messages);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        constexpr
        bool
        MessageAttributes::operator ==(const MessageAttributes& other)
            const noexcept
        {
            return (level == other.level)
                && (std::string_view{location.file_name()} ==
                    std::string_view{other.location.file_name()})
                && (location.line() == other.location.line());
        }


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
        task_clear()
        {
            auto messages = safe_messages.lock();
            messages->clear();
            timestamp = 0;
        }


        void
        task_log(Message& msg)
        {
            MessageAttributes attr{
                msg.level,
                msg.location,
                msg.tag
            };

            if (last_msg_attributes != attr) {
                std::println(cout,
                             "{}{}{} [{}:{}] {}\n{}",
                             log_level_ansi_prefixes.at(std::to_underlying(msg.level)),
                             msg.level,
                             ansi_reset,
                             msg.location.file_name(),
                             msg.location.line(),
                             msg.tag,
                             msg.text);
                last_msg_attributes = std::move(attr);
            } else {
                // Skip repeating message attributes line
                std::println(cout,
                             "{}",
                             msg.text);
            }

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

        main_thread_id = std::this_thread::get_id();
        std::println("main thread is {}", main_thread_id);
    }


    void
    finalize()
    {
        TRACE_FUNC;
    }


    void
    clear()
    {
        if (std::this_thread::get_id() != main_thread_id)
            App::add_task("LogManager::task_clear()"sv,
                          task_clear);
        else
            task_clear();
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
        if (std::this_thread::get_id() != main_thread_id)
            App::add_task("LogManager::task_log()"sv,
                          task_log,
                          std::move(msg));
        else {
            // std::println("logging from the main thread");
            task_log(msg);
        }
    }


    void
    save()
    {
        TRACE_FUNC;

        auto logs_dir = App::get_data_dir() / "logs";
        if (!exists(logs_dir))
            create_directories(logs_dir);

        auto filename = logs_dir / create_log_filename();
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
