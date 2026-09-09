/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef REST_HPP
#define REST_HPP

#include <atomic>
#include <flat_map>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include <curlxx/easy.hpp>
#include <curlxx/multi.hpp>

#include "async_task_queue.hpp"


namespace rest {

    struct error : std::runtime_error {

        std::string content;
        std::string content_type;

        error(const std::string& msg,
              std::string content_ = {},
              std::string content_type_ = {});

    }; // struct error


    struct request;
    struct manager;


    using response_callback_signature = void (request& req);

    using response_function_t = std::move_only_function<response_callback_signature>;


    using error_callback_signature = void (const std::exception& e);

    using error_function_t = std::move_only_function<error_callback_signature>;


    using get_params_t = std::flat_map<std::string, std::string>;


    struct request {

    public:

        enum class status {
            pending,
            receiving,
            finished,
            canceled,
        };


        struct params_t {
            std::optional<std::string>  accept_content_type  = {};
            unsigned                    buffer_size          = 65536;
            mutable error_function_t    error_func           = {};
            std::optional<std::string>  post_fields          = {};
            std::optional<std::string>  request_content_type = {};
            mutable response_function_t response_func        = {};
            bool                        ssl_verify_peer      = false;
            std::string                 url;
            std::optional<std::string>  user_agent           = {};
            bool                        verbose              = true;
        }; // struct params_t


        request(params_t params_);


        void
        cancel();


        std::string_view
        get_content()
            const noexcept;


        std::string_view
        get_content_type()
            const noexcept;


        std::exception_ptr
        get_error()
            const noexcept;

        curl::easy&
        get_easy()
            noexcept;


        CURL*
        get_handle()
            const noexcept;


        const params_t&
        get_params()
            const noexcept;


        status
        get_status()
            const noexcept;


        void
        process();


        void
        process_error()
            noexcept;


        void
        process_response()
            noexcept;


        void
        set_error(std::exception_ptr e);


    private:

        std::atomic<status> status_;
        const params_t params;
        std::string content;
        std::string content_type;
        curl::easy easy;
        std::exception_ptr error_ptr;

        std::size_t
        easy_write_func(std::span<const char> data);

        void
        invoke_error_func(const std::exception& e)
            noexcept;

    }; // struct request


    using request_ptr = std::shared_ptr<request>;



    class manager {

    public:

        struct config {
            unsigned        max_connections       = 5;
            unsigned        max_total_connections = 5;
            std::optional<std::string> user_agent;
        };


        manager(config cfg_,
                const std::string& base_url_);

        // Prevent moving
        manager(manager&&) = delete;


        ~manager()
            noexcept;


        void
        process();


        void
        set_base_url(const std::string& base_url_);


        request_ptr
        add(request::params_t params);


        request_ptr
        get(const std::string& path,
            response_function_t response_func,
            error_function_t error_func);

        request_ptr
        get(const std::string& path,
            const get_params_t& get_params,
            response_function_t response_func,
            error_function_t error_func);


        request_ptr
        get_json(const std::string& path,
                 response_function_t response_func,
                 error_function_t error_func);

        request_ptr
        get_json(const std::string& path,
                 const get_params_t& get_params,
                 response_function_t response_func,
                 error_function_t error_func);


        request_ptr
        post(const std::string& path,
             std::string post_body,
             response_function_t response_func,
             error_function_t error_func);


        request_ptr
        post_json(const std::string& path,
                  std::string post_json_body,
                  response_function_t response_func,
                  error_function_t error_func);


    private:

        const config cfg;

        std::string base_url;

        async_queue<request_ptr> new_requests;
        async_task_queue tasks;

        struct {
            std::jthread thread;
            std::unordered_map<CURL*, request_ptr> active;
            curl::multi multi;
        } worker;


        void
        worker_thread_function(std::stop_token stopper);

    }; // class manager


    // Synchronous functions.

    request_ptr
    get_sync(request::params_t params);


    request_ptr
    post_sync(request::params_t params);

} // namespace rest

#endif
