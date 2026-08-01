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
#include <string>
#include <utility>
#include <iosfwd>
#include <source_location>

#include <curlxx/easy.hpp>


namespace LogManager {

    enum class Level : unsigned {
        debug,
        info,
        warning,
        error,
    };


    struct Message {
        Level level;
        std::source_location location;
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
    for_each(Level min_level,
             const CallbackFunction& func);


    Timestamp
    get_timestamp();


    void
    log(Message msg);


    template<typename... Args>
    inline
    void
    log(Level level,
        std::source_location location,
        std::format_string<Args...> fmt,
        Args&&... args)
    {
        log(
            Message{level,
                    std::move(location),
                    std::format(std::move(fmt),
                                std::forward<Args>(args)...)}
        );
    }


    void
    process();


    void
    save();


    void
    capture_curl_debug(curl::easy& easy,
                       const std::source_location& location = std::source_location::current());


    std::string
    to_string(Level level);


    std::ostream&
    operator <<(std::ostream& out,
                Level level);

} // namespace LogManager


#define LOG_DEBUG(...)                                  \
    LogManager::log(LogManager::Level::debug,           \
                    std::source_location::current(),    \
                    __VA_ARGS__)

#define LOG_INFO(...)                                   \
    LogManager::log(LogManager::Level::info,            \
                    std::source_location::current(),    \
                    __VA_ARGS__)


#define LOG_WARN(...)                                   \
    LogManager::log(LogManager::Level::warning,         \
                    std::source_location::current(),    \
                    __VA_ARGS__)


#define LOG_ERROR(...)                                  \
    LogManager::log(LogManager::Level::error,           \
                    std::source_location::current(),    \
                    __VA_ARGS__)

#endif
