/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef REST_HPP
#define REST_HPP

#include <map>
#include <functional>
#include <string>

#include <curlxx/easy.hpp>

#include "json.hpp"


namespace rest {

    using success_callback_t = void (curl::easy& ez,
                                     const std::string& response,
                                     const std::string& content_type);
    using success_function_t = std::function<success_callback_t>;


    using error_callback_t = void (curl::easy& ez,
                                   const std::exception& error);
    using error_function_t = std::function<error_callback_t>;


    using json_success_callback_t = void(curl::easy& ez,
                                         const json::value& response);
    using json_success_function_t = std::function<json_success_callback_t>;


    using request_params_t = std::map<std::string, std::string>;


    void
    initialize(const std::string& user_agent = "");


    void
    finalize();


    // TODO: expose the curl::easy object for each request.


    void
    get(const std::string& url,
        success_function_t on_success,
        error_function_t on_error = {});


    void
    get(const std::string& base_url,
        const request_params_t& params,
        success_function_t on_success,
        error_function_t on_error = {});


    void
    get_json(const std::string& url,
             json_success_function_t on_success,
             error_function_t on_error = {});

    void
    get_json(const std::string& base_url,
             const request_params_t& params,
             json_success_function_t on_success,
             error_function_t on_error = {});



    void
    process();

} // namespace rest

#endif
