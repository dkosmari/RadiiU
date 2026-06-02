/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <curlxx/curl.hpp>

#include <glaze/json.hpp>
#include <glaze/json/generic.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include "rest.hpp"

#include "byte_stream.hpp"
#include "tracer.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace rest {

    /* --------------------- */
    /* function declarations */
    /* --------------------- */

    curl::easy
    make_easy(const std::string& url);

    std::string
    serialize(const params_t& params);

    [[noreturn]]
    void
    throw_error(const std::string& msg);

    [[noreturn]]
    void
    throw_error(const std::string& msg,
                glz::error_ctx e);

    template<typename Buffer>
    [[noreturn]]
    void
    throw_error(const std::string& msg,
                glz::error_ctx e,
                Buffer&& source);


    /* -------------- */
    /* struct request */
    /* -------------- */

    struct request {

        status current_status = status::pending;
        curl::easy easy;
        success_function_t success_func;
        error_function_t error_func;
        byte_stream response_stream;

        // forbid moving
        request(request&& other) = delete;

        request(const std::string& url,
                success_function_t success_func,
                error_function_t error_func);

        request(const std::string& url,
                const glz::generic_u64& args,
                success_function_t success_func,
                error_function_t error_func);


        virtual
        ~request()
            noexcept = default;

        CURL*
        get_id()
            const noexcept;

        void
        finish()
            noexcept;

        void
        on_success(const std::string& data,
                   const std::string& content_type)
            noexcept;

        void
        on_error(const std::exception& e)
            noexcept;

    }; // struct request


    struct json_request : request {

        json_success_function_t json_success_func;

        json_request(const std::string& url,
                     json_success_function_t json_success_func,
                     error_function_t error_func);

        json_request(const std::string& url,
                     const glz::generic_u64& args,
                     json_success_function_t json_success_func,
                     error_function_t error_func);

        void
        handle_success(const std::string& response,
                       const std::string& content_type);

    }; // struct json_request


    /* ---------------- */
    /* struct resources */
    /* ---------------- */

    struct resources {

        curl::multi multi;
        std::map<CURL*, std::shared_ptr<request>> requests;

        resources();

        ~resources()
            noexcept;

        void
        add(std::shared_ptr<request> req);

        void
        remove(std::shared_ptr<request>& req);

        void
        process();

    }; // struct resources


    /* --------- */
    /* variables */
    /* --------- */

    std::string user_agent;
    std::optional<resources> res;
    unsigned init_counter;


    /* --------------- */
    /* request methods */
    /* --------------- */

    request::request(const std::string& url,
                     success_function_t success_func,
                     error_function_t error_func) :
        easy{make_easy(url)},
        success_func{std::move(success_func)},
        error_func{std::move(error_func)}
    {
        // cout << "creating request for '" << url << "'" << endl;
        easy.set_write_function([this](std::span<const char> data)
                                {
                                    return response_stream.write(data);
                                });
    }


    request::request(const std::string& url,
                     const glz::generic_u64& args,
                     success_function_t success_func,
                     error_function_t error_func) :
        request{url,
                std::move(success_func),
                std::move(error_func)}
    {
        std::string post_body;
        if (auto e = glz::write_json(args, post_body))
            throw_error("glz::write_json() failed", e);
        easy.set_post(true);
        easy.set_copy_post_fields(post_body);
    }


    CURL*
    request::get_id()
        const noexcept
    {
        return easy.data();
    }


    void
    request::finish()
        noexcept
    try {
        current_status = status::finished;
        std::string response = response_stream.read_str();
        std::string content_type;
        if (auto h = easy.try_get_header("Content-Type"))
            content_type = h->value;
        on_success(response, content_type);
    }
    catch (std::exception& e) {
        on_error(e);
    }


    void
    request::on_success(const std::string& response,
                        const std::string& content_type)
        noexcept
    try {
        if (success_func)
            success_func(response, content_type);
    }
    catch (std::exception& e){
        cout << "ERROR: request::on_success(): " << e.what() << endl;
    }
    catch (...) {
        cout << "ERROR: request::on_success() caught an exception!" << endl;
    }


    void
    request::on_error(const std::exception& e)
        noexcept
    try {
        if (error_func)
            error_func(e);
    }
    catch (std::exception& ee) {
        cout << "ERROR: request::on_error(): " << ee.what() << endl;
    }
    catch (...) {
        cout << "ERROR: request::on_error() caught an exception!" << endl;
    }


    /* -------------------- */
    /* json_request methods */
    /* -------------------- */

    json_request::json_request(const std::string& url,
                               json_success_function_t json_success_func,
                               error_function_t error_func) :
        request{url,
                [this](const std::string& response,
                       const std::string& content_type)
                {
                    handle_success(response, content_type);
                },
                std::move(error_func)},
        json_success_func{std::move(json_success_func)}
    {
        easy.set_http_headers("Accept: application/json");
    }


    json_request::json_request(const std::string& url,
                               const glz::generic_u64& args,
                               json_success_function_t json_success_func,
                               error_function_t error_func) :
        json_request{url, std::move(json_success_func), std::move(error_func)}
    {
        easy.set_http_headers("Accept: application/json",
                              "Content-Type: application/json");

        std::string post_body;
        if (auto e = glz::write_json(args, post_body))
            throw_error("glz::write_json() failed", e);
        easy.set_post(true);
        easy.set_copy_post_fields(post_body);
    }


    void
    json_request::handle_success(const std::string& response,
                                 const std::string& content_type)
    {
        if (!mime_type::match(content_type, "application/json"))
            throw_error("Invalid content type response: "s + content_type);

        glz::generic_u64 json;
        if (auto e = glz::read_json(json, response))
            throw_error("glz::read_json() failed", e, response);

        if (json_success_func)
            json_success_func(json, response);
    }


    /* ----------------- */
    /* resources methods */
    /* ----------------- */

    resources::resources()
    {
        multi.set_max_total_connections(5);
        multi.set_max_connections(5);
    }


    resources::~resources()
        noexcept
    {
        for (auto& [key, val] : requests)
            if (val)
                multi.remove(val->easy);
    }


    void
    resources::add(std::shared_ptr<request> req)
    {
        assert(req);
        multi.add(req->easy);
        requests.emplace(req->get_id(), std::move(req));
    }


    void
    resources::remove(std::shared_ptr<request>& req)
    {
        assert(req);
        multi.remove(req->easy);
        requests.erase(req->get_id());
    }


    void
    resources::process()
    {
        multi.perform();
        for (auto& [easy, error] : multi.get_done()) {
            auto id = easy->data();
            auto it = requests.find(id);
            if (it == requests.end()) {
                cout << "BUG: finished an unknown handle!" << endl;
                continue;
            }
            auto req = std::move(it->second);
            remove(req);
            if (error)
                req->on_error(curl::error{error});
            else
                req->finish();
        }
    }


    /* ------------- */
    /* token methods */
    /* ------------- */

    token::token(std::shared_ptr<request> req)
        noexcept :
        req{std::move(req)}
    {}


    token::~token()
        noexcept
    {
        detach();
    }


    status
    token::get_status()
        const noexcept
    {
        if (!req)
            return status::invalid;
        return req->current_status;
    }


    void
    token::cancel()
    {
        if (req && req->current_status != status::finished) {
            req->current_status = status::canceled;
            res->remove(req);
        }
    }


    void
    token::detach()
        noexcept
    {
        req.reset();
    }


    bool
    token::is_pending()
        const noexcept
    {
        if (!req)
            return false;
        return req->current_status == status::pending;
    }


    /* -------------- */
    /* rest functions */
    /* -------------- */

    void
    initialize(const std::string& ua)
    {
        TRACE_FUNC;

        if (init_counter++)
            return;

        user_agent = ua;

        res.emplace();
    }


    void
    finalize()
    {
        TRACE_FUNC;

        if (--init_counter)
            return;

        res.reset();
    }


    void
    process()
    {
        assert(res);
        res->process();
    }


    token
    get_async(const std::string& url,
              success_function_t success_func,
              error_function_t error_func)
    {
        auto req = std::make_shared<request>(url,
                                             std::move(success_func),
                                             std::move(error_func));
        res->add(req);
        return token{std::move(req)};
    }


    token
    get_async(const std::string& base_url,
              const params_t& params,
              success_function_t success_func,
              error_function_t error_func)
    {
        std::string full_url = base_url + serialize(params);
        return get_async(full_url,
                         std::move(success_func),
                         std::move(error_func));
    }


    response_and_type_t
    get_sync(const std::string& url)
    {
        TRACE_FUNC;

        curl::easy easy = make_easy(url);
        byte_stream response_stream;
        easy.set_write_function(
            [&response_stream](std::span<const char> buf)
            {
                return response_stream.write(buf);
            });
        easy.perform();
        // TODO: throw exception when HTTP error
        std::string content_type;
        if (auto h = easy.try_get_header("Content-Type"))
            content_type = h->value;
        return {
            response_stream.read_str(),
            content_type
        };
    }


    response_and_type_t
    get_sync(const std::string& base_url,
             const params_t& params)
    {
        return get_sync(base_url + serialize(params));
    }


    response_and_type_t
    post_sync(const std::string& url,
              const glz::generic_u64& params)
    {
        TRACE_FUNC;

        curl::easy easy = make_easy(url);

        std::string post_body;
        if (auto e = glz::write_json(params, post_body))
            throw_error("glz::write_json() failed", e);
        easy.set_post(true);
        easy.set_copy_post_fields(post_body);

        byte_stream response_stream;
        easy.set_write_function([&response_stream](std::span<const char> buf)
                                {
                                    return response_stream.write(buf);
                                });
        easy.perform();
        // TODO: throw exception when HTTP error
        std::string content_type;
        if (auto h = easy.try_get_header("Content-Type"))
            content_type = h->value;
        return {
            response_stream.read_str(),
            content_type
        };
    }


    token
    get_json_async(const std::string& url,
                   json_success_function_t json_success_func,
                   error_function_t error_func)
    {
        auto req = std::make_shared<json_request>(url,
                                                  std::move(json_success_func),
                                                  std::move(error_func));
        res->add(req);
        return token{std::move(req)};
    }


    token
    get_json_async(const std::string& base_url,
                   const params_t& params,
                   json_success_function_t json_success_func,
                   error_function_t error_func)
    {
        std::string full_url = base_url + serialize(params);
        return get_json_async(full_url,
                              std::move(json_success_func),
                              std::move(error_func));
    }


    token
    post_json_async(const std::string& url,
                    const glz::generic_u64& params,
                    json_success_function_t success_func,
                    error_function_t error_func)
    {
        auto req = std::make_shared<json_request>(url,
                                                  params,
                                                  std::move(success_func),
                                                  std::move(error_func));
        res->add(req);
        return token{std::move(req)};
    }


    token
    post_json_async(const std::string& url,
                    const glz::raw_json& params,
                    json_success_function_t success_func,
                    error_function_t error_func)
    {
        glz::generic_u64 params_generic;
        glz::ex::read_json(params_generic, params.str);
        return post_json_async(url,
                               params_generic,
                               std::move(success_func),
                               std::move(error_func));
    }


    glz::generic_u64
    get_json_sync(const std::string& url)
    {
        curl::easy easy = make_easy(url);
        easy.set_http_headers("Accept: application/json");
        byte_stream response_stream;
        easy.set_write_function(
            [&response_stream](std::span<const char> buf)
            {
                return response_stream.write(buf);
            });
        easy.perform();
        std::string content_type = easy.get_header("Content-Type").value;
        if (!mime_type::match(content_type, "application/json"))
            throw_error("Invalid content type response: "s + content_type);
        glz::generic_u64 json;
        std::string response = response_stream.read_str();
        if (auto error = glz::read_json(json, response))
            throw_error("glz::read_json() failed", error, response);
        return json;
    }


    glz::generic_u64
    get_json_sync(const std::string& base_url,
                  const params_t& params)
    {
        return get_json_sync(base_url + serialize(params));
    }


    glz::generic_u64
    post_json_sync(const std::string& url,
                   const glz::generic_u64& params)
    {
        curl::easy easy = make_easy(url);
        easy.set_http_headers("Accept: application/json",
                              "Content-Type: application/json");

        std::string post_body;
        if (auto e = glz::write_json(params, post_body))
            throw_error("glz::write_json() failed", e);
        easy.set_post(true);
        easy.set_copy_post_fields(post_body);

        byte_stream response_stream;
        easy.set_write_function([&response_stream](std::span<const char> buf)
                              {
                                  return response_stream.write(buf);
                              });
        easy.perform();
        std::string content_type = easy.get_header("Content-Type").value;
        if (!mime_type::match(content_type, "application/json"))
            throw_error("Invalid content type response: "s + content_type);
        glz::generic_u64 json;
        std::string response = response_stream.read_str();
        if (auto e = glz::read_json(json, response))
            throw_error("glz::read_json() failed: "s, e, response);
        return json;
    }


    /* ---------------- */
    /* helper functions */
    /* ---------------- */

    curl::easy
    make_easy(const std::string& url)
    {
        curl::easy easy;
        easy.set_verbose(false);
        easy.set_http_version(curl::easy::http_version::none);
        easy.set_url(url);
        if (!user_agent.empty())
            easy.set_user_agent(user_agent);
        easy.set_follow_location(true);
        easy.set_auto_referer(true);
        easy.set_ssl_verify_peer(false);
        easy.set_accept_encoding("");
        easy.set_transfer_encoding(true);
        easy.set_buffer_size(65536);
        easy.set_tcp_no_delay(false);
        return easy;
    }


    std::string
    serialize(const params_t& params)
    {
        std::string result;
        const char* separator = "?";
        for (auto& [key, val] : params) {
            result += separator + curl::escape(key) + "=" + curl::escape(val);
            separator = "&";
        }
        return result;
    }


    void
    throw_error(const std::string& msg)
    {
        throw std::runtime_error{msg};
    }


    void
    throw_error(const std::string& msg,
                glz::error_ctx e)
    {
        throw std::runtime_error{msg + ": "s + glz::format_error(e)};
    }


    template<typename Buffer>
    void
    throw_error(const std::string& msg,
                glz::error_ctx e,
                Buffer&& source)
    {
        throw std::runtime_error{msg + ": "s + glz::format_error(e, source)};
    }

} // namespace rest
