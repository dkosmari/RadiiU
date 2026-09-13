/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <chrono>
#include <format>
#include <utility>

#include <curlxx/url.hpp>

#include "rest.hpp"

#include "LogManager.hpp"
#include "LogManagerCurl.hpp"
#include "mime_type.hpp"
#include "TraceManager.hpp"
#include "tracer.hpp"


using namespace std::literals;


// New API

namespace rest {

    namespace {

        // Function declarations

        std::string
        make_url(const std::string& base_url,
                 const std::string& path,
                 const get_params_t& params = {});


        void
        task_process_error(request_ptr req);

        void
        task_process_response(request_ptr req);


        // Function definitions

        std::string
        make_url(const std::string& base_url,
                 const std::string& path,
                 const get_params_t& params)
        {
            curl::url url{base_url};
            if (!path.empty())
                url.set_path(path);

            for (const auto& [key, val] : params)
                url.append_query(key + "=" + val, CURLU_URLENCODE);

            return url.get_url();
        }


        void
        task_process_error(request_ptr req)
        {
            req->process_error();
        }


        void
        task_process_response(request_ptr req)
        {
            req->process_response();
        }

    } // namespace


    // Public functions

    error::error(const std::string& msg,
                 std::string content_,
                 std::string content_type_) :
        std::runtime_error{msg},
        content{std::move(content_)},
        content_type{std::move(content_type_)}
    {}


    request::request(params_t params_) :
        status_{status::pending},
        params{std::move(params_)}
    {
        easy.set_verbose(params.verbose);
        LogManagerCurl::capture_curl_debug(easy);

        easy.set_ssl_verify_peer(params.ssl_verify_peer);
        easy.set_buffer_size(params.buffer_size);
        easy.set_url(params.url);
        // LOG_DEBUG("request url: {:?}", params.url);

        if (params.user_agent)
            easy.set_user_agent(*params.user_agent);

        if (params.post_fields) {
            easy.set_post(true);
            easy.set_post_fields(*params.post_fields);
            // LOG_DEBUG("request has post_fields:\n<fields>\n{}\n</fields>",
            //           *params.post_fields);
        }

        if (params.accept_content_type)
            easy.append_http_header("Accept: " + *params.accept_content_type);

        if (params.request_content_type)
            easy.append_http_header("Content-Type: " + *params.request_content_type);

        easy.set_accept_encoding("");
        easy.set_auto_referer(true);
        easy.set_fail_on_error(true);
        easy.set_follow_location(true);
        easy.set_http_version(curl::easy::http_version::none);
        easy.set_tcp_no_delay(false);
        easy.set_transfer_encoding(true);

        // Set up write callback.
        easy.set_write_function(std::bind_front(&request::easy_write_func, this));
    }


    void
    request::cancel()
    {
        status_ = status::canceled;
    }


    std::string_view
    request::get_content()
        const noexcept
    {
        return content;
    }


    std::string_view
    request::get_content_type()
        const noexcept
    {
        return content_type;
    }


    std::exception_ptr
    request::get_error()
        const noexcept
    {
        return error_ptr;
    }


    curl::easy&
    request::get_easy()
        noexcept
    {
        return easy;
    }


    CURL*
    request::get_handle()
        const noexcept
    {
        if (!easy)
            return nullptr;
        return easy.data();
    }


    const request::params_t&
    request::get_params()
        const noexcept
    {
        return params;
    }


    request::status
    request::get_status()
        const noexcept
    {
        return status_;
    }


    void
    request::process()
    {
        try {
            easy.perform();
            process_response();
        }
        catch (...) {
            error_ptr = std::current_exception();
            process_error();
        }
    }


    void
    request::process_error()
        noexcept
    {
        status_ = status::finished;

        try {
            if (!error_ptr)
                throw std::logic_error{"BUG: no error"};
            std::rethrow_exception(error_ptr);
        }
        catch (std::exception& e) {
            invoke_error_func(e);
        }
        catch (...) {
            invoke_error_func(std::logic_error{"Unknown exception type"});
        }
    }


    void
    request::process_response()
        noexcept
    {
        status_ = status::finished;

        try {
            // Enforce content type
            if (params.accept_content_type) {
                if (!mime_type::match(content_type, *params.accept_content_type)) {
                    throw error{
                        std::format("Content-Type mismatch: asked for {:?} but got {:?}",
                                    *params.accept_content_type,
                                    content_type),
                        content,
                        content_type
                    };
                }
            }

            if (params.response_func)
                params.response_func(*this);
        }
        catch (...) {
            error_ptr = std::current_exception();
            process_error();
        }
    }


    void
    request::set_error(std::exception_ptr e)
    {
        error_ptr = std::move(e);
    }


    std::size_t
    request::easy_write_func(std::span<const char> data)
    {
        if (status_ == status::pending) {
            // First recv
            status_ = status::receiving;
            content_type = easy.try_get_content_type().value_or(""s);
            // If length is known, reserve that.
            if (auto content_length_header = easy.try_get_header("Content-Length")) {
                auto size = std::stoull(content_length_header->value);
                content.reserve(size);
            }
        }
        content.append(data.data(), data.size());
        return data.size();
    }


    void
    request::invoke_error_func(const std::exception& e)
        noexcept
    {
        try {
            if (params.error_func)
                params.error_func(e);
        }
        catch (std::exception& ee) {
            LOG_ERROR("rest::request: error_func leaked exception: {}", ee.what());
        }
        catch (...) {
            LOG_ERROR("rest::request: error_func leaked unknown exception");
        }
    }


    manager::manager(config cfg_,
                     const std::string& base_url_) :
        cfg{std::move(cfg_)},
        base_url{base_url_}
    {
        worker.multi.set_max_connections(cfg.max_connections);
        worker.multi.set_max_total_connections(cfg.max_total_connections);

        worker.thread = std::jthread{
            std::bind_front(&manager::worker_thread_function, this)
        };

    }


    manager::~manager()
        noexcept
    {
        worker.thread = {};
        if (!tasks.empty())
            LOG_ERROR("Destroying rest::manager that had {} pending tasks.",
                      tasks.size());

        // Remove all easy handles from the multi.
        for (auto& [key, req] : worker.active)
            worker.multi.remove(req->get_easy());

    }


    void
    manager::process()
    {
        tasks.dispatch_all();
    }


    void
    manager::set_base_url(const std::string& base_url_)
    {
        base_url = base_url_;
    }


    request_ptr
    manager::add(request::params_t params)
    {
        auto req = std::make_shared<request>(std::move(params));
        new_requests.push(req);
        return req;
    }


    request_ptr
    manager::get(const std::string& path,
                 response_function_t response_func,
                 error_function_t error_func)
    {
        return get(
            path,
            {},
            std::move(response_func),
            std::move(error_func)
        );
    }


    request_ptr
    manager::get(const std::string& path,
                 const get_params_t& get_params,
                 response_function_t response_func,
                 error_function_t error_func)
    {
        return add(
            {
                .error_func = std::move(error_func),
                .response_func = std::move(response_func),
                .url = make_url(base_url, path, get_params),
                .user_agent = cfg.user_agent,
            }
        );
    }


    request_ptr
    manager::get_json(const std::string& path,
                      response_function_t response_func,
                      error_function_t error_func)
    {
        return get_json(
            path,
            {},
            std::move(response_func),
            std::move(error_func)
        );
    }


    request_ptr
    manager::get_json(const std::string& path,
                      const get_params_t& get_params,
                      response_function_t response_func,
                      error_function_t error_func)
    {
        return add(
            {
                .accept_content_type = "application/json",
                .error_func = std::move(error_func),
                .response_func = std::move(response_func),
                .url = make_url(base_url, path, get_params),
                .user_agent = cfg.user_agent,
            }
        );
    }


    request_ptr
    manager::post(const std::string& path,
                  std::string post_body,
                  response_function_t response_func,
                  error_function_t error_func)
    {
        return add(
            {
                .error_func = std::move(error_func),
                .post_fields = std::move(post_body),
                .response_func = std::move(response_func),
                .url = make_url(base_url, path),
                .user_agent = cfg.user_agent
            }
        );
    }


    request_ptr
    manager::post_json(const std::string& path,
                       std::string post_body,
                       response_function_t response_func,
                       error_function_t error_func)
    {
        return add(
            {
                .accept_content_type = "application/json",
                .error_func = std::move(error_func),
                .post_fields = std::move(post_body),
                .request_content_type = "application/json",
                .response_func = std::move(response_func),
                .url = make_url(base_url, path),
                .user_agent = cfg.user_agent,
            }
        );
    }


    void
    manager::worker_thread_function(std::stop_token stopper)
    {
        try {
            TraceManager::thread_name("rest worker thread"sv);

            while (!stopper.stop_requested()) {

                bool idle = true;

                while (auto maybe_new_req = new_requests.try_pop()) {
                    idle = false;
                    auto& new_req = *maybe_new_req;
                    if (new_req->get_status() != request::status::pending)
                        continue;
                    worker.active[new_req->get_handle()] = new_req;
                    worker.multi.add(new_req->get_easy());
                }

                if (stopper.stop_requested())
                    break;

                // Remove all canceled requests.
                std::erase_if(worker.active,
                              [this](auto& item) -> bool
                              {
                                  auto& [key, req] = item;
                                  if (req->get_status() == request::status::canceled) {
                                      worker.multi.remove(req->get_easy());
                                      return true;
                                  }
                                  return false;
                              });

                if (worker.multi.perform() > 0)
                    idle = false;

                if (stopper.stop_requested())
                    break;

                for (auto& [easy, err] : worker.multi.get_done()) {
                    idle = false;
                    auto key = easy->data();
                    auto it = worker.active.find(key);
                    if (it == worker.active.end()) {
                        LOG_ERROR("BUG: finished transfer for unknown handle!");
                        continue;
                    }
                    auto req = std::move(it->second);
                    worker.multi.remove(req->get_easy());
                    worker.active.erase(key);

                    // Queue the appropriate task to the main thread.
                    if (err) {
                        req->set_error(
                            std::make_exception_ptr(curl::error{err})
                        );
                        tasks.add("rest::task_process_error()"sv,
                                  task_process_error,
                                  req);
                    } else {
                        tasks.add("rest::task_process_response()"sv,
                                  task_process_response,
                                  req);
                    }
                }

                if (stopper.stop_requested())
                    break;

                // Preserve CPU: if there was no work done this iteration, sleep for a while.
                if (idle)
                    std::this_thread::sleep_for(100ms);

            }
        }
        catch (std::exception& e) {
            LOG_ERROR("rest::manager thread stopped by exception: {}", e.what());
        }
        catch (...) {
            LOG_ERROR("rest::manager thread stopped by unknown exception.");
        }
    }


    request_ptr
    get_sync(request::params_t params)
    {
        auto req = std::make_shared<request>(std::move(params));
        req->process();
        return req;
    }


    request_ptr
    post_sync(request::params_t params)
    {
        auto req = std::make_shared<request>(std::move(params));
        req->process();
        return req;
    }


} // namespace rest
