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
#include <string>

#include <curlxx/easy.hpp>

#include <glaze/json/generic_fwd.hpp>

#include "mime_type.hpp"


namespace rest {

    using success_function_sig = void (const std::string& response,
                                       const std::string& content_type);
    using success_function_t = std::move_only_function<success_function_sig>;

    using error_function_sig = void (const std::exception& error);
    using error_function_t = std::move_only_function<error_function_sig>;


    using json_success_function_sig = void (const glz::generic_u64& response,
                                            const std::string& response_raw);
    using json_success_function_t = std::move_only_function<json_success_function_sig>;


    using params_t = std::map<std::string, std::string>;


    struct request;

    enum class status {
        invalid,
        pending,
        finished,
        canceled,
    }; // enum class status


    class token {

        std::shared_ptr<request> req;

    public:

        constexpr
        token()
        noexcept = default;

        explicit
        token(std::shared_ptr<request> req)
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


    token
    get_async(const std::string& url,
              success_function_t success_func,
              error_function_t error_func = {});


    token
    get_async(const std::string& base_url,
              const params_t& params,
              success_function_t success_func,
              error_function_t error_func = {});


    struct response_and_type_t {
        std::string response;
        std::string content_type;
    };

    response_and_type_t
    get_sync(const std::string& url);


    response_and_type_t
    get_sync(const std::string& base_url,
             const params_t& params);


    response_and_type_t
    post_sync(const std::string& url,
              const glz::generic_u64& params);

    response_and_type_t
    post_sync(const std::string& url,
              const glz::raw_json& params);


    token
    get_json_async(const std::string& url,
                   json_success_function_t success_func,
                   error_function_t error_func = {});


    token
    get_json_async(const std::string& base_url,
                   const params_t& params,
                   json_success_function_t success_func,
                   error_function_t error_func = {});

    token
    post_json_async(const std::string& url,
                    const glz::generic_u64& params,
                    json_success_function_t success_func,
                    error_function_t error_func = {});

    token
    post_json_async(const std::string& url,
                    const glz::raw_json& params,
                    json_success_function_t success_func,
                    error_function_t error_func = {});


    glz::generic_u64
    get_json_sync(const std::string& url);

    glz::generic_u64
    get_json_sync(const std::string& url,
                  const params_t& params);

    glz::generic_u64
    post_json_sync(const std::string& url,
                   const glz::generic_u64& params);

    glz::generic_u64
    post_json_sync(const std::string& url,
                   const glz::raw_json& params);

} // namespace rest

#endif
