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

#include "rest.hpp"

#include "byte_stream.hpp"
#include "tracer.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


// TODO: implement data streaming too


namespace rest {

    /* --------------------- */
    /* function declarations */
    /* --------------------- */

    curl::easy
    make_easy(const std::string& url);

    std::string
    make_url(const std::string& base_url,
             const get_params_t& params);


    /* -------------- */
    /* struct request */
    /* -------------- */

    struct request_base {

        status current_status = status::pending;
        curl::easy easy;
        success_function_t success_func;
        error_function_t error_func;
        byte_stream response_stream;

        // forbid moving
        request_base(request_base&& other) = delete;

        explicit
        request_base();

        request_base(success_function_t success_func,
                     error_function_t error_func);


        virtual
        ~request_base()
            noexcept = default;

        CURL*
        get_id()
            const noexcept;

        void
        finish()
            noexcept;

        virtual
        void
        handle_success(const std::string& response,
                       const std::string& content_type)
            noexcept;

        void
        handle_error(const std::exception& e)
            noexcept;

    }; // struct request


    struct request_get : virtual request_base {

        request_get(const std::string& url,
                    const get_params_t& params,
                    success_function_t success_func = {},
                    error_function_t error_func = {});

    }; // struct request_get


    struct request_post : virtual request_base {

        request_post(const std::string& url,
                     const std::string& post_body,
                     success_function_t success_func = {},
                     error_function_t error_func = {});

    }; // struct request_post


    struct json_request_base : virtual request_base {
        json_success_function_t json_success_func;

        json_request_base(json_success_function_t json_success_func);

        void
        handle_success(const std::string& response,
                       const std::string& content_type)
            noexcept override;

    }; // struct json_request


    struct json_request_get : request_get, json_request_base {

        json_request_get(const std::string& url,
                         const get_params_t& params,
                         json_success_function_t json_success_func,
                         error_function_t error_func);

    }; // struct json_request_get


    struct json_request_post : request_post, json_request_base {

        json_request_post(const std::string& url,
                          const std::string& body,
                          json_success_function_t json_success_func,
                          error_function_t error_func);

    }; // struct json_request_post


    /* ---------------- */
    /* struct resources */
    /* ---------------- */

    struct resources {

        curl::multi multi;
        std::map<CURL*, std::shared_ptr<request_base>> requests;

        resources();

        ~resources()
            noexcept;

        void
        add(std::shared_ptr<request_base> req);

        void
        remove(std::shared_ptr<request_base>& req);

        void
        process();

    }; // struct resources


    /* --------- */
    /* variables */
    /* --------- */

    std::string user_agent;
    std::optional<resources> res;
    unsigned init_counter;


    /* ------------- */
    /* error methods */
    /* ------------- */

    error::error(const std::string& msg,
                 const std::string& response,
                 const std::string& content_type) :
        std::runtime_error{msg},
        response{std::move(response)},
        content_type{std::move(content_type)}
    {}


    /* -------------------- */
    /* request_base methods */
    /* -------------------- */

    request_base::request_base()
    {
        abort();
    }


    request_base::request_base(success_function_t success_func,
                               error_function_t error_func) :
        success_func{std::move(success_func)},
        error_func{std::move(error_func)}
    {
        // cout << "creating request for '" << url << "'" << endl;
        easy.set_write_function(
            [this](std::span<const char> data)
            {
                return response_stream.write(data);
            });
    }


    CURL*
    request_base::get_id()
        const noexcept
    {
        return easy.data();
    }


    void
    request_base::finish()
        noexcept
    {
        std::string response;
        std::string content_type;
        try {
            current_status = status::finished;
            response = response_stream.read_str();
            if (auto h = easy.try_get_header("Content-Type"))
                content_type = h->value;
            handle_success(response, content_type);
        }
        catch (std::exception& e) {
            handle_error(error{e.what(), response, content_type});
        }
    }


    void
    request_base::handle_success(const std::string& response,
                                 const std::string& content_type)
        noexcept
    try {
        if (success_func)
            success_func(response, content_type);
    }
    catch (std::exception& e) {
        TRACE_FUNC;
        cout << "ERROR: " << e.what() << endl;
        handle_error(error{e.what(), response, content_type});
    }
    catch (...) {
        TRACE_FUNC;
        cout << "ERROR: caught unknown exception!" << endl;
        cout << "response was:\n<response>\n" << response << "\n</response>" << endl;
        handle_error(error{"unknown exception", response, content_type});
    }


    void
    request_base::handle_error(const std::exception& e)
        noexcept
    try {
        if (error_func)
            error_func(e);
    }
    catch (std::exception& ee) {
        TRACE_FUNC;
        cout << "ERROR: " << ee.what() << endl;
    }
    catch (...) {
        TRACE_FUNC;
        cout << "ERROR: caught unknown exception!" << endl;
    }


    /* ------------------- */
    /* request_get methods */
    /* ------------------- */

    request_get::request_get(const std::string& url,
                             const get_params_t& params,
                             success_function_t success_func,
                             error_function_t error_func) :
        request_base{std::move(success_func), std::move(error_func)}
    {
        easy.set_url(make_url(url, params));
    }


    /* -------------------- */
    /* request_post methods */
    /* -------------------- */

    request_post::request_post(const std::string& url,
                               const std::string& body,
                               success_function_t success_func,
                               error_function_t error_func) :
        request_base{std::move(success_func), std::move(error_func)}
    {
        easy.set_url(url);
        easy.set_post(true);
        easy.set_copy_post_fields(body);
    }


    /* ------------------------- */
    /* json_request_base methods */
    /* ------------------------- */

    json_request_base::json_request_base(json_success_function_t json_success_func) :
        request_base{},
        json_success_func{std::move(json_success_func)}
    {
        easy.set_http_headers("Accept: application/json");
    }


    void
    json_request_base::handle_success(const std::string& response,
                                      const std::string& content_type)
        noexcept
    try {
        if (!mime_type::match(content_type, "application/json")) {
            handle_error(error{
                    "invalid content type",
                    response,
                    content_type
                });
            return;
        }
        if (json_success_func)
            json_success_func(response);
    }
    catch (std::exception& e) {
        handle_error(error{e.what(), response, content_type});
    }
    catch (...) {
        handle_error(error{"unknown exception", response, content_type});
    }


    /*--------------------------*/
    /* json_request_get methods */
    /*--------------------------*/

    json_request_get::json_request_get(const std::string& base_url,
                                       const get_params_t& params,
                                       json_success_function_t json_success_func,
                                       error_function_t error_func) :
        request_base{{}, std::move(error_func)},
        request_get{base_url, params},
        json_request_base{std::move(json_success_func)}
    {}


    /* ------------------------- */
    /* json_request_post methods */
    /* ------------------------- */

    json_request_post::json_request_post(const std::string& url,
                                         const std::string& body,
                                         json_success_function_t json_success_func,
                                         error_function_t error_func) :
        request_base{{}, std::move(error_func)},
        request_post{url, body},
        json_request_base{std::move(json_success_func)}
    {
        cout << "DEBUG: making json post request\n"
             << "    URL: " << url << "\n"
             << "    body: " << body
             << endl;
        easy.append_http_header("Content-Type: application/json");
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
    resources::add(std::shared_ptr<request_base> req)
    {
        assert(req);
        multi.add(req->easy);
        requests.emplace(req->get_id(), std::move(req));
    }


    void
    resources::remove(std::shared_ptr<request_base>& req)
    {
        assert(req);
        multi.remove(req->easy);
        requests.erase(req->get_id());
    }


    void
    resources::process()
    {
        multi.perform();
        for (auto& [easy, err] : multi.get_done()) {
            auto id = easy->data();
            auto it = requests.find(id);
            if (it == requests.end()) {
                cout << "BUG: finished an unknown handle!" << endl;
                continue;
            }
            auto req = std::move(it->second);
            remove(req);
            if (err)
                req->handle_error(curl::error{err});
            else
                req->finish();
        }
    }


    /* ------------- */
    /* token methods */
    /* ------------- */

    token::token(std::shared_ptr<request_base> req)
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
    get_async(const std::string& base_url,
              const get_params_t& params,
              success_function_t success_func,
              error_function_t error_func)
    {
        auto req = std::make_shared<request_get>(base_url,
                                                 params,
                                                 std::move(success_func),
                                                 std::move(error_func));
        res->add(req);
        return token{std::move(req)};
    }


    token
    post_async(const std::string& url,
               const std::string& body,
               success_function_t success_func,
               error_function_t error_func)
    {
        auto req = std::make_shared<request_post>(url,
                                                  body,
                                                  std::move(success_func),
                                                  std::move(error_func));
        res->add(req);
        return token{std::move(req)};
    }


    response_and_type_t
    get_sync(const std::string& base_url,
             const get_params_t& params)
    {
        std::string url = make_url(base_url, params);
        curl::easy easy = make_easy(url);
        byte_stream response_stream;
        easy.set_write_function(
            [&response_stream](std::span<const char> buf)
            {
                return response_stream.write(buf);
            });
        easy.perform();
        std::string response = response_stream.read_str();
        std::string content_type;
        if (auto h = easy.try_get_header("Content-Type"))
            content_type = h->value;
        return {std::move(response), std::move(content_type)};
    }


    response_and_type_t
    post_sync(const std::string& url,
              const std::string& body)
    {
        curl::easy easy = make_easy(url);
        easy.set_post(true);
        easy.set_copy_post_fields(body);

        byte_stream response_stream;
        easy.set_write_function(
            [&response_stream](std::span<const char> buf)
            {
                return response_stream.write(buf);
            });
        easy.perform();
        std::string response = response_stream.read_str();
        std::string content_type;
        if (auto h = easy.try_get_header("Content-Type"))
            content_type = h->value;
        return {std::move(response), std::move(content_type)};
    }


    token
    get_json_async(const std::string& base_url,
                   const get_params_t& params,
                   json_success_function_t json_success_func,
                   error_function_t error_func)
    {
        auto req = std::make_shared<json_request_get>(base_url,
                                                      params,
                                                      std::move(json_success_func),
                                                      std::move(error_func));
        res->add(req);
        return token{std::move(req)};
    }


    token
    post_json_async(const std::string& url,
                    const std::string& body,
                    json_success_function_t success_func,
                    error_function_t error_func)
    {
        auto req = std::make_shared<json_request_post>(url,
                                                       body,
                                                       std::move(success_func),
                                                       std::move(error_func));
        res->add(req);
        return token{std::move(req)};
    }


    std::string
    get_json_sync(const std::string& base_url,
                  const get_params_t& params)
    {
        std::string url = make_url(base_url, params);
        curl::easy easy = make_easy(url);
        easy.set_http_headers("Accept: application/json");
        byte_stream response_stream;
        easy.set_write_function(
            [&response_stream](std::span<const char> buf)
            {
                return response_stream.write(buf);
            });
        easy.perform();
        std::string response = response_stream.read_str();
        std::string content_type = easy.get_header("Content-Type").value;
        if (!mime_type::match(content_type, "application/json"))
            throw error{"Invalid content type", response, content_type};
        return response;
    }


    std::string
    post_json_sync(const std::string& url,
                   const std::string& body)
    {
        curl::easy easy = make_easy(url);
        easy.set_http_headers("Accept: application/json",
                              "Content-Type: application/json");

        easy.set_post(true);
        easy.set_copy_post_fields(body);

        byte_stream response_stream;
        easy.set_write_function(
            [&response_stream](std::span<const char> buf)
            {
                return response_stream.write(buf);
            });
        easy.perform();
        std::string response = response_stream.read_str();
        std::string content_type = easy.get_header("Content-Type").value;
        if (!mime_type::match(content_type, "application/json"))
            throw error{"Invalid content type", response, content_type};
        return response;
    }


    /* ---------------- */
    /* helper functions */
    /* ---------------- */

    curl::easy
    make_easy(const std::string& url)
    {
        curl::easy easy;
        easy.set_verbose(true);
        if (!user_agent.empty())
            easy.set_user_agent(user_agent);
        easy.set_accept_encoding("");
        easy.set_auto_referer(true);
        easy.set_buffer_size(65536);
        easy.set_fail_on_error(true);
        easy.set_follow_location(true);
        easy.set_http_version(curl::easy::http_version::none);
        easy.set_ssl_verify_peer(false);
        easy.set_tcp_no_delay(false);
        easy.set_transfer_encoding(true);
        easy.set_url(url);
        return easy;
    }


    std::string
    make_url(const std::string& base_url,
             const get_params_t& params)
    {
        if (params.empty())
            return base_url;
        std::string result = base_url;
        const char* separator = "?";
        for (auto& [key, val] : params) {
            result += separator + curl::escape(key) + "=" + curl::escape(val);
            separator = "&";
        }
        return result;
    }

} // namespace rest
