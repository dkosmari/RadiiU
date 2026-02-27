/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <curlxx/curl.hpp>

#include "rest.hpp"

#include "byte_stream.hpp"


using std::cout;
using std::endl;


namespace rest {

    std::string user_agent;

    curl::multi multi;


    struct request {

        byte_stream stream;
        curl::easy easy;
        success_function_t on_success;
        error_function_t on_error;


        // disallow moving, since the read function needs to hold this
        request(request&& other) = delete;

        request(const std::string& url,
                success_function_t on_success_arg,
                error_function_t on_error_arg) :
            on_success{std::move(on_success_arg)},
            on_error{std::move(on_error_arg)}
        {
            cout << "preparing request for '" << url << "'" << endl;
            easy.set_url(url);
            easy.set_verbose(false);
            if (!user_agent.empty())
                easy.set_user_agent(user_agent);
            easy.set_follow_location(true);
            easy.set_ssl_verify_peer(false);
            easy.set_write_function(std::bind_front(&request::on_write, this));
        }


        std::size_t
        on_write(std::span<const char> buf)
        {
            return stream.write(buf);
        }

    }; // struct request


    std::vector<std::unique_ptr<request>> requests;


    void
    initialize(const std::string& ua)
    {
        cout << "Running rest::initialize()" << endl;
        user_agent = ua;
        multi.set_max_total_connections(5);
        multi.set_max_connections(5);
    }


    void
    finalize()
    {
        cout << "Running rest::finalize()" << endl;
        for (auto& req : requests)
            multi.remove(req->easy);
        multi.destroy();
        requests.clear();
        cout << "Done cleaning up rest module" << endl;
    }


    std::string
    concat(const request_params_t& params)
    {
        std::string result;
        const char* separator = "?";
        for (auto& [key, val] : params) {
            result += separator + curl::escape(key) + "=" + curl::escape(val);
            separator = "&";
        }
        return result;
    }


    curl::easy&
    get(const std::string& url,
        success_function_t on_success,
        error_function_t on_error)
    {
        auto req = std::make_unique<request>(url,
                                             std::move(on_success),
                                             std::move(on_error));
        curl::easy& ez = req->easy;
        multi.add(ez);
        requests.push_back(std::move(req));
        return ez;
    }


    curl::easy&
    get(const std::string& base_url,
        const request_params_t& params,
        success_function_t on_success,
        error_function_t on_error)
    {
        std::string full_url = base_url + concat(params);
        return get(full_url,
                   std::move(on_success),
                   std::move(on_error));
    }


    std::string
    get_sync(const std::string& url)
    {
        curl::easy ez;
        ez.set_url(url);
        ez.set_verbose(false);
        if (!user_agent.empty())
            ez.set_user_agent(user_agent);
        ez.set_follow_location(true);
        ez.set_ssl_verify_peer(false);
        byte_stream stream;
        ez.set_write_function([&stream](std::span<const char> buf)
                              {
                                  return stream.write(buf);
                              });
        ez.perform();
        return stream.read_str();
    }


    std::string
    get_sync(const std::string& base_url,
             const request_params_t& params)
    {
        return get_sync(base_url + concat(params));
    }


    curl::easy&
    get_json(const std::string& url,
             json_success_function_t on_success,
             error_function_t on_error)
    {
        auto& ez = get(url,
                       [on_success=std::move(on_success)](curl::easy& ez,
                                                          const std::string& response,
                                                          const std::string& content_type)
                       {
                           if (!content_type.starts_with("application/json"))
                               throw std::runtime_error{"Content-Type should be application/json, but got "
                                                        + content_type
                                                        + "\nContent:\n"
                                                        + response.substr(0, 256)
                                                        + (response.size() > 256 ? "\n..." : "\n<<EOF>>")};
                           if (!content_type.starts_with("application/json")) {
                               cout << "ERROR: response was not JSON!" << endl;
                               return;
                           }
                           try {
                               json::value response_value = json::parse(response);
                               if (on_success)
                                   on_success(ez, response_value);
                           }
                           catch (std::exception& e) {
                               cout << "ERROR: parsing JSON: " << e.what() << endl;
                               throw;
                           }
                       },
                       on_error);
        ez.set_http_headers({ "Accept: application/json" });
        return ez;
    }


    curl::easy&
    get_json(const std::string& base_url,
             const request_params_t& params,
             json_success_function_t on_success,
             error_function_t on_error)
    {
        std::string full_url = base_url + concat(params);
        return get_json(full_url, std::move(on_success), std::move(on_error));
    }


    json::value
    get_json_sync(const std::string& url)
    {
        curl::easy ez;
        ez.set_url(url);
        ez.set_verbose(false);
        if (!user_agent.empty())
            ez.set_user_agent(user_agent);
        ez.set_follow_location(true);
        ez.set_ssl_verify_peer(false);
        ez.set_http_headers({ "Accept: application/json" });
        byte_stream stream;
        ez.set_write_function([&stream](std::span<const char> buf)
                              {
                                  return stream.write(buf);
                              });
        ez.perform();
        auto content_type_header = ez.try_get_header("Content-Type");
        if (content_type_header)
            if (!content_type_header->value.starts_with("application/json"))
                throw std::runtime_error{"Content-Type should be application/json, but got "
                                         + content_type_header->value
                                         + "\nContent:\n"
                                         + stream.read_str(256)
                                         + (!stream.empty() ? "\n..." : "\n<<EOF>>")};
        return json::parse(stream.read_str());
    }


    json::value
    get_json_sync(const std::string& base_url,
                  const request_params_t& params)
    {
        return get_json_sync(base_url + concat(params));
    }


    std::vector<std::unique_ptr<request>>::iterator
    find(curl::easy* ez)
    {
        for (auto it = requests.begin(); it != requests.end(); ++it)
            if (&(*it)->easy == ez)
                return it;
        return requests.end();
    }


    void
    process()
    {
        multi.perform();

        // For each DONE transfer, dispatch to the on_success() or on_error() callbacks.
        for (auto [ez, error_code] : multi.get_done()) {
            auto it = find(ez);
            if (it == requests.end()) {
                cout << "BUG: could not find request!" << endl;
                continue;
            }

            std::unique_ptr<request> req = std::move(*it);
            requests.erase(it);
            multi.remove(*ez);

            if (!error_code) {
                if (req->on_success) {
                    std::string response = req->stream.read_str();
                    std::string content_type;
                    auto content_type_header = req->easy.try_get_header("Content-Type");
                    if (content_type_header)
                        content_type = content_type_header->value;

                    try {
                        if (req->on_success)
                            req->on_success(req->easy, response, content_type);
                    }
                    catch (std::exception& e) {
                        if (req->on_error)
                            req->on_error(req->easy, e);
                    }
                }
            } else {
                curl::error e{error_code};
                if (req->on_error)
                    req->on_error(req->easy, e);
            }

        }

    }

} // namespace rest
