/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LOG_MANAGER_HPP
#define LOG_MANAGER_HPP

#include <cstdint>
#include <format>
#include <functional>
#include <source_location>
#include <string>
#include <utility>

#include "LogLevel.hpp"

namespace LogManager {

    struct Message {
        LogLevel level;
        std::source_location location;
        std::string tag;
        std::string text;
    };


    using CallbackSignature = void(const Message& msg);
    using CallbackFunction = std::function<CallbackSignature>;

    using Timestamp = std::uint64_t;


    void
    initialize();

    void
    finalize();


    void
    clear();


    void
    for_each(const CallbackFunction& func);

    void
    for_each(LogLevel min_level,
             const CallbackFunction& func);


    Timestamp
    get_timestamp();


    void
    log(Message msg);


    template<typename... Args>
    inline
    void
    log(LogLevel level,
        std::source_location location,
        std::string tag,
        std::format_string<Args...> fmt,
        Args&&... args)
    {
        log(
            Message{level,
                    std::move(location),
                    tag,
                    std::format(std::move(fmt),
                                std::forward<Args>(args)...)}
        );
    }


    void
    process();


    void
    save();

} // namespace LogManager


// TAG-less macros

#define LOG_DEBUG(...)                                  \
    LogManager::log(LogLevel::debug,                    \
                    std::source_location::current(),    \
                    "",                                 \
                    __VA_ARGS__)

#define LOG_INFO(...)                                   \
    LogManager::log(LogLevel::info,                     \
                    std::source_location::current(),    \
                    "",                                 \
                    __VA_ARGS__)


#define LOG_WARN(...)                                   \
    LogManager::log(LogLevel::warning,                  \
                    std::source_location::current(),    \
                    "",                                 \
                    __VA_ARGS__)


#define LOG_ERROR(...)                                  \
    LogManager::log(LogLevel::error,                    \
                    std::source_location::current(),    \
                    "",                                 \
                    __VA_ARGS__)


// TAG macros

#define LOG_DEBUG_TAG(tag, ...)                         \
    LogManager::log(LogLevel::debug,                    \
                    std::source_location::current(),    \
                    tag,                                \
                    __VA_ARGS__)

#define LOG_INFO_TAG(tag, ...)                          \
    LogManager::log(LogLevel::info,                     \
                    std::source_location::current(),    \
                    tag,                                \
                    __VA_ARGS__)


#define LOG_WARN_TAG(tag, ...)                          \
    LogManager::log(LogLevel::warning,                  \
                    std::source_location::current(),    \
                    tag,                                \
                    __VA_ARGS__)


#define LOG_ERROR_TAG(tag, ...)                         \
    LogManager::log(LogLevel::error,                    \
                    std::source_location::current(),    \
                    tag,                                \
                    __VA_ARGS__)

#endif
