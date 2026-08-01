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
#include <ostream>

#include "LogManager.hpp"

#include "App.hpp"
#include "async_queue.hpp"
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

        using Task = std::future<void>;
        using MessageVec = std::deque<Message>;
        using SafeMessageVec = thread_safe<MessageVec, std::recursive_mutex>;


        /*-----------*/
        /* Constants */
        /*-----------*/

        const MessageVec::size_type max_messages = 256;


        /*-----------*/
        /* Variables */
        /*-----------*/

        Timestamp timestamp;

        SafeMessageVec safe_messages;

        async_queue<Task> pending_tasks;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        template<typename F,
                 typename... Args>
        void
        add_task(F&& func,
                 Args&&... args);

        std::string
        create_log_filename();

        void
        dispatch_pending_tasks();

        void
        real_clear();

        void
        real_log(Message msg);

        void
        trim_messages(MessageVec& messages);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        template<typename F,
                 typename... Args>
        void
        add_task(F&& func,
                 Args&&... args)
        {
            pending_tasks.push(std::async(std::launch::deferred,
                                          std::forward<F>(func),
                                          std::forward<Args>(args)...));
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
        dispatch_pending_tasks()
        {
            while (auto t = pending_tasks.try_pop())
                try {
                    t->get();
                }
                catch (std::exception& e) {
                    cerr << "ERROR in LogManager::dispatch_pending_tasks(): "
                         << e.what()
                         << endl;
                }
        }


        void
        real_clear()
        {
            safe_messages.lock()->clear();
            timestamp = 0;
        }


        void
        real_log(Message msg)
        {
            ++timestamp;
            cout << "[LOG] [" << to_string(msg.level)<< "] " << msg.text << endl;
            auto messages = safe_messages.lock();
            messages->push_back(std::move(msg));
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
        add_task(real_clear);
    }


    void
    for_each(const CallbackFunction& func)
    {
        for_each(Level::debug, func);
    }


    void
    for_each(Level min_level,
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
        add_task(real_log, std::move(msg));
    }


    void
    process()
    {
        dispatch_pending_tasks();
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
            output << "["
                   << msg.level
                   << "] ["
                   << msg.location.file_name()
                   << ":"
                   << msg.location.line()
                   << "]: "
                   << msg.text
                   << '\n';
    }


    void
    capture_curl_debug(curl::easy& easy,
                       const std::source_location& location)
    {
        easy.set_debug_function(
            [location](CURL*,
                       curl_infotype type,
                       std::span<const char> data)
            {
                switch (type) {
                    case CURLINFO_TEXT:
                        log(Level::debug,
                            location,
                            "curl info: {}",
                            std::string_view(data.data(), data.size()));
                        break;

                    case CURLINFO_HEADER_IN:
                        log(Level::debug,
                            location,
                            "curl header in: {}",
                            std::string_view(data.data(), data.size()));
                        break;

                    case CURLINFO_HEADER_OUT:
                        log(Level::debug,
                            location,
                            "curl header out: {}",
                            std::string_view(data.data(), data.size()));
                        break;

                    default:
                        ;
                }
            }
        );
    }


    std::string
    to_string(Level level)
    {
        switch (level) {
            using enum Level;

            case debug:
                return "DEBUG";

            case info:
                return "INFO"s;

            case warning:
                return "WARN"s;

            case error:
                return "ERROR"s;

            default:
                return "<UNKNOWN>"s;
        }
    }

    std::ostream&
    operator <<(std::ostream& out,
                Level level)
    {
        return out << to_string(level);
    }

} // namespace LogManager
