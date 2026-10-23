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
#include <print>
#include <ranges>
#include <string_view>
#include <thread>

#include "LogManager.hpp"

#include "App.hpp"
#include "thread_safe.hpp"
#include "tracer.hpp"


using std::cout;
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
            "\e[1;7m"sv,          // DEBUG
            "\e[1;7;34m"sv,       // INFO
            "\e[1;7;33m"sv,       // WARN
            "\e[1;7;31m"sv        // ERROR
        };

        const std::string_view ansi_reset = "\e[0m"sv;

        const std::string_view tag_ansi_prefix = "\e[0;3;95m"sv;


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
        print_header(const MessageAttributes& attr);

        void
        print_lines(std::string_view lines);

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
                && (location.line() == other.location.line())
                && (tag == other.tag);
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
        print_header(const MessageAttributes& attr)
        {
            if (attr.tag.empty()) {
                std::println(cout,
                             "{}{}{} [{}:{}]",
                             log_level_ansi_prefixes.at(std::to_underlying(attr.level)),
                             attr.level,
                             ansi_reset,
                             attr.location.file_name(),
                             attr.location.line());
            } else {
                std::println(cout,
                             "{}{}{} [{}:{}] {}{}{}",
                             log_level_ansi_prefixes.at(std::to_underlying(attr.level)),
                             attr.level,
                             ansi_reset,
                             attr.location.file_name(),
                             attr.location.line(),
                             tag_ansi_prefix,
                             attr.tag,
                             ansi_reset);
            }
        }


        void
        print_lines(std::string_view lines)
        {
            // avoid printing an empty last line
            if (lines.ends_with('\n'))
                lines.remove_suffix(1);
            for (auto line : lines | std::views::split('\n')) {
                std::string_view line_text{line.begin(), line.end()};
                if (line_text.ends_with('\r'))
                    line_text.remove_suffix(1);
                std::println(cout, "{}", line_text);
            }
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
                print_header(attr);
                last_msg_attributes = std::move(attr);
            }

            auto text = std::string_view{msg.text};
            bool multiline = text.ends_with("\r\n"sv)
                || text.ends_with("\r"sv)
                || text.ends_with("\n"sv);
            if (!multiline)
                std::println(cout, "{}", text);
            else
                print_lines(text);

            cout.flush();

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

        cout.sync_with_stdio(false);
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
            App::add_task(
                "LogManager::task_log()"sv,
                task_log,
                std::move(msg)
            );
        else
            task_log(msg);
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
