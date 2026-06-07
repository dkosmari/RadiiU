/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef REST_HPP
#define REST_HPP

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include <curlxx/easy.hpp>

#include "mime_type.hpp"


namespace rest {

    struct error : std::runtime_error {

        std::string response;
        std::string content_type;

        error(const std::string& msg,
              const std::string& response = {},
              const std::string& content_type = {});

    }; // struct error


    using success_function_sig = void (const std::string& response,
                                       const std::string& content_type);
    using success_function_t = std::move_only_function<success_function_sig>;

    using error_function_sig = void (const std::exception& err);
    using error_function_t = std::move_only_function<error_function_sig>;


    using json_success_function_sig = void (const std::string& json_response);
    using json_success_function_t = std::move_only_function<json_success_function_sig>;


    using get_params_t = std::map<std::string, std::string>;


    struct request_base;

    enum class status {
        invalid,
        pending,
        finished,
        canceled,
    }; // enum class status


    class token {

        std::shared_ptr<request_base> req;

    public:

        constexpr
        token()
        noexcept = default;

        explicit
        token(std::shared_ptr<request_base> req)
            noexcept;

        ~token()
            noexcept;

        status
        get_status()
            const noexcept;

        void
        cancel();

        void
        detach()
            noexcept;

        bool
        is_pending()
            const noexcept;

    }; // class token


    void
    initialize(const std::string& user_agent = "");


    void
    finalize();


    void
    process();


    /* ----------------- */
    /* Untyped functions */
    /* ----------------- */

    token
    get_async(const std::string& base_url,
              const get_params_t& params,
              success_function_t success_func,
              error_function_t error_func = {});


    token
    post_async(const std::string& url,
               const std::string& body,
               success_function_t success_func,
               error_function_t error_func = {});


    struct response_and_type_t {
        std::string response;
        std::string content_type;
    };

    response_and_type_t
    get_sync(const std::string& base_url,
             const get_params_t& params = {});


    response_and_type_t
    post_sync(const std::string& url,
              const std::string& body);

    /* -------------- */
    /* JSON functions */
    /* -------------- */

    token
    get_json_async(const std::string& base_url,
                   const get_params_t& params,
                   json_success_function_t success_func,
                   error_function_t error_func = {});

    token
    post_json_async(const std::string& url,
                    const std::string& params,
                    json_success_function_t success_func,
                    error_function_t error_func = {});


    std::string
    get_json_sync(const std::string& base_url,
                  const get_params_t& params = {});

    std::string
    post_json_sync(const std::string& url,
                   const std::string& body);

} // namespace rest

#endif
