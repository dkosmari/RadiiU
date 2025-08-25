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

    using response_callback_t = void (curl::easy& ez,
                                      const std::string& response,
                                      const std::string& content_type);
    using response_function_t = std::function<response_callback_t>;


    using error_callback_t = void (curl::easy& ez,
                                   const curl::error& error);
    using error_function_t = std::function<error_callback_t>;


    using json_response_callback_t = void(curl::easy& ez,
                                          const json::value& response);
    using json_response_function_t = std::function<json_response_callback_t>;


    void
    initialize(const std::string& user_agent = "");


    void
    finalize();


    void
    get(const std::string& url,
        response_function_t on_response = {},
        error_function_t on_error = {});


    void
    get(const std::string& base_url,
        const std::map<std::string, std::string>& args,
        response_function_t on_response = {},
        error_function_t on_error = {});


    void
    get_json(const std::string& url,
             json_response_function_t on_response = {},
             error_function_t on_error = {});

    void
    get_json(const std::string& base_url,
             const std::map<std::string, std::string>& args,
             json_response_function_t on_response = {},
             error_function_t on_error = {});



    void
    process();

} // namespace rest

#endif
