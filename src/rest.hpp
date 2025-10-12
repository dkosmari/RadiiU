/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef REST_HPP
#define REST_HPP

#include <functional>
#include <map>
#include <string>

#include <curlxx/easy.hpp>

#include "json.hpp"


namespace rest {

    using success_function_t = std::function<void (curl::easy& ez,
                                                   const std::string& response,
                                                   const std::string& content_type)>;

    using error_function_t = std::function<void (curl::easy& ez,
                                                 const std::exception& error)>;

    using json_success_function_t = std::function<void (curl::easy& ez,
                                                        const json::value& response)>;

    using request_params_t = std::map<std::string, std::string>;


    void
    initialize(const std::string& user_agent = "");


    void
    finalize();


    curl::easy&
    get(const std::string& url,
        success_function_t on_success = {},
        error_function_t on_error = {});


    curl::easy&
    get(const std::string& base_url,
        const request_params_t& params,
        success_function_t on_success = {},
        error_function_t on_error = {});


    std::string
    get_sync(const std::string& url);

    std::string
    get_sync(const std::string& url,
             const request_params_t& params);


    curl::easy&
    get_json(const std::string& url,
             json_success_function_t on_success = {},
             error_function_t on_error = {});

    curl::easy&
    get_json(const std::string& base_url,
             const request_params_t& params,
             json_success_function_t on_success = {},
             error_function_t on_error = {});


    json::value
    get_json_sync(const std::string& url);

    json::value
    get_json_sync(const std::string& url,
                  const request_params_t& params);


    void
    process();

} // namespace rest

#endif
