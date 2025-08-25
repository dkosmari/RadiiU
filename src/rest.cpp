/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <utility>
#include <vector>
#include <memory>
#include <iostream>

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
        response_function_t on_response;
        error_function_t on_error;


        // disallow moving, since the read function needs to hold this
        request(request&& other) = delete;

        request(const std::string& url,
                response_function_t on_response_arg,
                error_function_t on_error_arg) :
            on_response{std::move(on_response_arg)},
            on_error{std::move(on_error_arg)}
        {
            std::cout << "preparing request for '" << url << "'" << endl;

            easy.set_url(url);
            easy.set_verbose(false);
            if (!user_agent.empty())
                easy.set_user_agent(user_agent);
            easy.set_follow(true);
            easy.set_ssl_verify_peer(false);
            easy.set_write_function(std::bind_front(&request::on_write, this));
        }


        std::size_t
        on_write(std::span<const char> buf)
        {
            return stream.write(buf.data(), buf.size());
        }


    };


    std::vector<std::unique_ptr<request>> requests;


    void
    initialize(const std::string& ua)
    {
        cout << "Running rest::initialize()" << endl;
        user_agent = ua;
        multi.set_max_total_connections(4);
        multi.set_max_connections(10);
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


    void
    get(const std::string& url,
        response_function_t on_response,
        error_function_t on_error)
    {
        auto req = std::make_unique<request>(url,
                                             std::move(on_response),
                                             std::move(on_error));
        multi.add(req->easy);
        requests.push_back(std::move(req));
    }


    void
    get(const std::string& base_url,
        const std::map<std::string, std::string>& args,
        response_function_t on_response,
        error_function_t on_error)
    {
        std::string full_url = base_url;
        const char* separator = "?";
        for (auto& [key, val] : args) {
            full_url += separator + curl::escape(key) + "=" + curl::escape(val);
            separator = "&";
        }
        get(full_url, std::move(on_response), std::move(on_error));
    }


    void
    get_json(const std::string& url,
             json_response_function_t on_response,
             error_function_t on_error)
    {
        get(url,
            [on_response=std::move(on_response)](curl::easy& ez,
                                                 const std::string& response,
                                                 const std::string& content_type)
            {
                if (content_type != "application/json") {
                    cout << "ERROR: response wasn't JSON!" << endl;
                    return;
                }
                try {
                    json::value response_value = json::parse(response);
                    if (on_response)
                        on_response(ez, response_value);
                }
                catch (std::exception& e) {
                    cout << "ERROR parsing JSON: " << e.what() << endl;
                }
            },
            on_error);
    }


    void
    get_json(const std::string& base_url,
             const std::map<std::string, std::string>& args,
             json_response_function_t on_response,
             error_function_t on_error)
    {
        std::string full_url = base_url;
        const char* separator = "?";
        for (auto& [key, val] : args) {
            full_url += separator + curl::escape(key) + "=" + curl::escape(val);
            separator = "&";
        }
        get_json(full_url, std::move(on_response), std::move(on_error));
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

        // For each DONE transfer, dispatch to the on_response() or on_error() callbacks.
        for (auto [ez, error_code] : multi.get_done()) {
            auto it = find(ez);
            if (it == requests.end()) {
                cout << "BUG: could find request!" << endl;
                continue;
            }

            std::unique_ptr<request> req = std::move(*it);
            requests.erase(it);
            multi.remove(*ez);

            if (!error_code) {
                if (req->on_response) {
                    std::string response = req->stream.read_str();
                    std::string content_type;
                    auto content_type_header = req->easy.try_get_header("Content-Type");
                    if (content_type_header)
                        content_type = content_type_header->value;

                    if (req->on_response)
                        req->on_response(req->easy, response, content_type);
                }
            } else {
                curl::error e{error_code};
                if (req->on_error)
                    req->on_error(req->easy, e);
            }

        }

    }

}
