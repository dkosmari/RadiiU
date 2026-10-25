/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <format>
#include <memory>
#include <queue>
#include <random>
#include <ranges>
#include <set>
#include <thread>
#include <unordered_set>
#include <utility>

#ifdef __WIIU__
#include <coreinit/time.h>
#endif

#include <curlxx/escape.hpp>

#include <glaze/json.hpp>
#include <glaze/json/generic.hpp>
#include <glaze/exceptions/core_exceptions.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include "RadioBrowserAPI.hpp"

#include "async_task_queue.hpp"
#include "LogManager.hpp"
#include "net/address.hpp"
#include "net/resolver.hpp"
#include "rest.hpp"
#include "Settings.hpp"
#include "TraceDuration.hpp"
#include "TraceFunction.hpp"
#include "TraceManager.hpp"
#include "tracer.hpp"
#include "utils.hpp"


using namespace std::literals;

using Settings::cfg;
using utils::throw_if_stopped;


template<>
struct glz::meta<RadioBrowserAPI::CodecsParams::Order> {
    using enum RadioBrowserAPI::CodecsParams::Order;
    static constexpr
    auto value = enumerate(name, stationcount);
};


template<>
struct glz::meta<RadioBrowserAPI::CountriesParams::Order> {
    using enum RadioBrowserAPI::CountriesParams::Order;
    static constexpr
    auto value = enumerate(name, stationcount);
};


template<>
struct glz::meta<RadioBrowserAPI::LanguagesParams::Order> {
    using enum RadioBrowserAPI::LanguagesParams::Order;
    static constexpr
    auto value = enumerate(name, stationcount);
};


template<>
struct glz::meta<RadioBrowserAPI::SearchStationsParams::Order> {
    using enum RadioBrowserAPI::SearchStationsParams::Order;
    static constexpr
    auto value = enumerate(
        name,
        url,
        homepage,
        favicon,
        tags,
        country,
        state,
        language,
        votes,
        codec,
        bitrate,
        lastcheckok,
        lastchecktime,
        clicktimestamp,
        clickcount,
        clicktrend,
        changetimestamp,
        random
    );
};


template<>
struct glz::meta<RadioBrowserAPI::StationsParams::Order> {
    using enum RadioBrowserAPI::StationsParams::Order;
    static constexpr
    auto value = enumerate(
        name,
        url,
        homepage,
        favicon,
        tags,
        country,
        state,
        language,
        votes,
        codec,
        bitrate,
        lastcheckok,
        lastchecktime,
        clicktimestamp,
        clickcount,
        clicktrend,
        changetimestamp,
        random
    );
};


template<>
struct glz::meta<RadioBrowserAPI::TagsParams::Order> {
    using enum RadioBrowserAPI::TagsParams::Order;
    static constexpr
    auto value = enumerate(name, stationcount);
};


namespace RadioBrowserAPI {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        struct StatusResponse {
            bool result;
            std::string response;
        };


#if 0
        struct BusyGuard {

            BusyGuard(std::stop_token& stoken);

            ~BusyGuard()
                noexcept;

        }; // struct BusyGuard
#endif

        // RAII type for resources.
        struct Resources {
            std::string user_agent;
            std::minstd_rand random_engine;

            string current_server;
            MirrorsVec mirrors;

            async_task_queue pending_tasks;

            std::optional<rest::manager> rest_manager;

            std::jthread fetch_mirrors_thread;
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        constexpr
        const glz::opts glz_options{ .error_on_unknown_keys = false, };

        const string start_server = "all.api.radio-browser.info"s;


        /*-----------*/
        /* Variables */
        /*-----------*/

        std::optional<Resources> res;

        std::atomic_bool busy;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        fetch_mirrors_thread_function(std::stop_token stoken,
                                      FetchMirrorsResultFunction result_func,
                                      ErrorMsgFunction error_func)
            noexcept;

        rest::error_function_t
        make_error_func(ExceptionFunction func);

        template<typename F>
        rest::response_function_t
        make_response_func(F&& func);

        std::minstd_rand
        make_random_engine();

        void
        start_call();

        void
        task_get_codecs(const CodecsParams& params,
                        GetCodecsResultFunction result_func,
                        ExceptionFunction except_func);

        void
        task_get_countries(const CountriesParams& params,
                           GetCountriesResultFunction result_func,
                           ExceptionFunction except_func);

        void
        task_get_languages(const std::string& filter,
                           const LanguagesParams& params,
                           GetLanguagesResultFunction result_func,
                           ExceptionFunction except_func);

        void
        task_get_server_stats(GetServerStatsResultFunction result_func,
                              ExceptionFunction except_func);

        void
        task_get_station(const string& uuid,
                         GetStationResultFunction result_func,
                         ExceptionFunction except_func);

        void
        task_get_tags(const TagsParams& params,
                      GetTagsResultFunction result_func,
                      ExceptionFunction except_func);

        void
        task_search_stations(const SearchStationsParams& params,
                             SearchStationsResultFunction result_func,
                             ExceptionFunction except_func);

        void
        task_send_click(const string& uuid,
                        SendClickResultFunction result_func,
                        ExceptionFunction except_func);

        void
        task_send_vote(const string& uuid,
                       SendVoteResultFunction result_func,
                       ExceptionFunction except_func);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

#if 0
        BusyGuard::BusyGuard(std::stop_token& stoken)
        {
            bool expected_false = false;
            while (!busy.compare_exchange_strong(expected_false, true)) {
                throw_if_stopped(stoken);
                std::this_thread::sleep_for(100ms);
                expected_false = false;
            }
        }


        BusyGuard::~BusyGuard()
            noexcept
        {
            busy = false;
        }
#endif


        void
        fetch_mirrors_thread_function(std::stop_token stoken,
                                      FetchMirrorsResultFunction result_func,
                                      ErrorMsgFunction error_func)
            noexcept
        {
            TraceManager::thread_name("fetch_mirrors_thread"sv);

            TraceFunction tf{"RadioBrowserAPI,fetch_mirrors_thread"sv};

            try {
                // Step 1: resolve all IP addresses
                std::unordered_set<net::address> addresses;
                LOG_DEBUG("Querying {}", start_server);
                {
                    TraceDuration duration_resolve_ip{
                        "fetch_mirrors_thread_function()/resolving IPs"sv,
                        "RadioBrowserAPI,fetch_mirrors_thread"sv
                    };
                    net::resolver::address_resolver ar;
                    ar.param.type = net::socket::type::tcp;
                    string server = start_server;
                    ar.process(server);

                    throw_if_stopped(stoken);

                    if (ar.error.message)
                        throw Error{"failed resolving \""
                                    + server
                                    + "\": "
                                    + *ar.error.message};
                    for (const auto& entry : ar.result.entries)
                        addresses.insert(entry.addr);
                }

                LOG_DEBUG("Found {} mirrors.", addresses.size());

                throw_if_stopped(stoken);

                // Step 2: find the canonical names for each IP
                std::set<string> names;
                {
                    TraceDuration trace_canonical{
                        "fetch_mirrors_thread_function()/resolve canonical names"sv,
                        "RadioBrowserAPI,fetch_mirrors_thread"sv
                    };
                    net::resolver::name_resolver nr;
                    for (const auto& addr : addresses) {
                        throw_if_stopped(stoken);
                        LOG_DEBUG("Querying canonical name for {}", addr);
                        try {
                            nr.process(addr);
                            if (nr.error.message)
                                throw Error{"Failed name lookup for \""
                                            + to_string(addr) + "\": "
                                            + *nr.error.message};
                            if (nr.result.name) {
                                LOG_DEBUG("{} -> {:?}", addr, *nr.result.name);
                                names.insert(std::move(*nr.result.name));
                            }
                        }
                        catch (std::exception& e) {
                            LOG_ERROR("{}", e.what());
                        }
                    }
                }

                LOG_DEBUG("Found {} servers.", names.size());

                throw_if_stopped(stoken);

                // Step 3: Invoke the result callback.
                if (result_func) {
                    res->pending_tasks.add("fetch_mirrors_thread()::result_func"sv,
                                           std::move(result_func),
                                           MirrorsVec{names.begin(), names.end()});
                }
            }
            catch (std::exception& e) {
                string msg = e.what();
                LOG_ERROR("{}", msg);
                if (error_func)
                    res->pending_tasks.add("fetch_mirrors_thread()::error_func"sv,
                                           std::move(error_func),
                                           std::move(msg));
            }
        }


        // Common code to clear the busy flag.
        rest::error_function_t
        make_error_func(ExceptionFunction func)
        {
            return
                [func = std::move(func)]
                (const std::exception& e)
                    mutable
                {
                    busy = false;
                    if (func)
                        func(e);
                };
        }


        // Common code to clear the busy flag.
        template<typename F>
        rest::response_function_t
        make_response_func(F&& func)
        {
            return
                [func = std::forward<F>(func)]
                (const rest::request& req)
                    mutable
                {
                    busy = false;
                    func(req);
                };
        }


        std::minstd_rand
        make_random_engine()
        {
#ifdef __WIIU__
            std::uint64_t now = OSGetTime();
            std::seed_seq seeder{
                static_cast<std::uint32_t>(now >> 32),
                static_cast<std::uint32_t>(now >> 0 )
            };
#else
            std::random_device rnd_dev;
            std::seed_seq seeder{
                rnd_dev(),
                rnd_dev()
            };
#endif
            return std::minstd_rand{seeder};
        }


        void
        start_call()
        {
            assert(res);
            assert(res->rest_manager);

            bool expected = false;
            if (!busy.compare_exchange_strong(expected, true))
                throw async_task_queue::call_again{};
        }


        void
        task_get_codecs(const CodecsParams& params,
                        GetCodecsResultFunction result_func,
                        ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            res->rest_manager->post_json(
                "/json/codecs"s,
                std::move(params_json),
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        CodecVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(std::move(result));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }


        void
        task_get_countries(const CountriesParams& params,
                           GetCountriesResultFunction result_func,
                           ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            res->rest_manager->post_json(
                "/json/countries"s,
                std::move(params_json),
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        CountryVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(std::move(result));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }


        void
        task_get_languages(const std::string& filter,
                           const LanguagesParams& params,
                           GetLanguagesResultFunction result_func,
                           ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            std::string endpoint = "/json/languages"s;
            if (!filter.empty())
                endpoint += "/"s + curl::escape(filter);

            res->rest_manager->post_json(
                std::move(endpoint),
                std::move(params_json),
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        LanguageVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(std::move(result));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }


        void
        task_get_server_stats(GetServerStatsResultFunction result_func,
                              ExceptionFunction except_func)
        {
            start_call();

            res->rest_manager->get_json(
                "/json/stats"s,
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        ServerStats result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(std::move(result));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }


        void
        task_get_station(const string& uuid,
                         GetStationResultFunction result_func,
                         ExceptionFunction except_func)
        {
            start_call();

            StationUUIDParams params { .uuids = uuid };
            std::string params_json;
            glz::ex::write_json(params, params_json);

            res->rest_manager->post_json(
                "/json/stations/byuuid"s,
                std::move(params_json),
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        StationVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (result.size() != 1)
                            throw Error{
                                std::format("incorrect array size: {}\n"
                                            "<content>\n{}\n</content>",
                                            result.size(),
                                            req.get_content())
                            };
                        if (func)
                            func(std::move(result[0]));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }


        void
        task_get_tags(const TagsParams& params,
                      GetTagsResultFunction result_func,
                      ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            res->rest_manager->post_json(
                "/json/tags"s,
                std::move(params_json),
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        TagVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(std::move(result));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }


        void
        task_search_stations(const SearchStationsParams& params,
                             SearchStationsResultFunction result_func,
                             ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            res->rest_manager->post_json(
                "/json/stations/search"s,
                std::move(params_json),
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        StationVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(std::move(result));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }


        void
        task_send_click(const string& uuid,
                        SendClickResultFunction result_func,
                        ExceptionFunction except_func)
        {
            start_call();

            // Note: clicking does not support GET/POST parameters.
            res->rest_manager->get_json(
                "/json/url/"s + uuid,
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        ClickResult result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(std::move(result));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }


        void
        task_send_vote(const string& uuid,
                       SendVoteResultFunction result_func,
                       ExceptionFunction except_func)
        {
            start_call();

            // NOTE: voting does not support GET/POST parameters.
            res->rest_manager->get_json(
                "/json/vote/"s + uuid,
                make_response_func(
                    [func = std::move(result_func)]
                    (const rest::request& req)
                        mutable
                    {
                        VoteResult result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(std::move(result));
                    }
                ),
                make_error_func(std::move(except_func))
            );
        }

    } // namespace


    /*-------------------*/
    /* Public functions. */
    /*-------------------*/

    Error::Error(const string& msg) :
        std::runtime_error{msg}
    {}


    void
    initialize(const string& user_agent,
               const string& server)
    {
        TRACE_FUNC;

        res.emplace();

        res->user_agent = user_agent;

        res->random_engine = make_random_engine();

        busy = false;

        if (!server.empty())
            res->current_server = server;
        else
            update_mirrors_and_select_random();

        rest::manager::config rest_config;
        rest_config.ssl_verify = !cfg.ignore_ssl_errors;
        rest_config.user_agent = user_agent;
        rest_config.verbose    = cfg.verbose_rest_logs;
        res->rest_manager.emplace(std::move(rest_config), get_server());
    }


    void
    finalize()
    {
        TRACE_FUNC;

        res.reset();
    }


    void
    process()
    {
        assert(res);

        try {
            res->pending_tasks.try_dispatch_one();
        }
        catch (async_task_queue::error& e) {
            LOG_ERROR("Dispatching RadioBrowerAPI task {}: {}", e.name, e.what());
        }

        assert(res->rest_manager);
        res->rest_manager->process();
    }


    bool
    is_busy()
    {
        return busy;
    }


    void
    set_server(const string& server)
    {
        assert(res);

        res->current_server = server;
        res->rest_manager->set_base_url("http://"s + get_server());
    }


    string
    get_server()
    {
        assert(res);

        return res->current_server.empty() ? start_server : res->current_server;
    }


    void
    fetch_mirrors(FetchMirrorsResultFunction result_func,
                  ErrorMsgFunction error_func)
    {
        TRACE_FUNC;

        assert(res);

        res->fetch_mirrors_thread = std::jthread{
            fetch_mirrors_thread_function,
            std::move(result_func),
            std::move(error_func)
        };
    }


    void
    update_mirrors()
    {
        fetch_mirrors(
            [](MirrorsVec result)
            {
                assert(res);

                res->mirrors = std::move(result);
            }
        );
    }


    void
    update_mirrors_and_select_random()
    {
        fetch_mirrors(
            [](MirrorsVec result)
            {
                assert(res);

                res->mirrors = std::move(result);
                if (res->mirrors.empty())
                    set_server("");
                else {
                    std::vector<string> samples(1);
                    std::ranges::sample(res->mirrors,
                                        samples.begin(),
                                        1,
                                        res->random_engine);
                    set_server(samples[0]);
                }
            }
        );
    }


    void
    for_each_mirror(ForEachMirrorFunction func)
    {
        assert(res);

        if (func)
            for (const auto& server : res->mirrors)
                func(server);
    }


    void
    get_codecs(const CodecsParams& params,
               GetCodecsResultFunction result_func,
               ExceptionFunction except_func)
    {
        assert(res);

        res->pending_tasks.add(
            "task_get_codecs()"sv,
            task_get_codecs,
            params,
            std::move(result_func),
            std::move(except_func)
        );
    }


    void
    get_countries(const CountriesParams& params,
                  GetCountriesResultFunction result_func,
                  ExceptionFunction except_func)
    {
        res->pending_tasks.add(
            "task_get_countries()"sv,
            task_get_countries,
            params,
            std::move(result_func),
            std::move(except_func)
        );
    }


    void
    get_countries_sync(std::stop_token& stoken,
                       const CountriesParams& params,
                       GetCountriesResultFunction result_func,
                       ExceptionFunction except_func)
    {
        assert(res);

        try {
            std::string params_json;
            glz::ex::write_json(params, params_json);

            throw_if_stopped(stoken);

            rest::request::params_t req_params{
                .accept_content = "application/json"s,
                .error_func =
                    [&except_func]
                    (const std::exception& e)
                        mutable
                    {
                        if (except_func)
                            except_func(e);
                    },
                .post_fields = std::move(params_json),
                .request_content = "application/json"s,
                .response_func =
                    [&stoken, func = std::move(result_func)]
                    (rest::request& req)
                        mutable
                    {
                        throw_if_stopped(stoken);
                        CountryVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        throw_if_stopped(stoken);
                        if (func)
                            func(result);
                    },
                .ssl_verify = !cfg.ignore_ssl_errors,
                .url = get_server() + "/json/countries"s,
                .user_agent = res->user_agent,
                .verbose = cfg.verbose_rest_logs,
            };

            throw_if_stopped(stoken);

            rest::post_sync(std::move(req_params));
        }
        catch (std::exception& e) {
            if (except_func)
                except_func(e);
        }
    }


    void
    get_languages(const std::string& filter,
                  const LanguagesParams& params,
                  GetLanguagesResultFunction result_func,
                  ExceptionFunction except_func)
    {
        res->pending_tasks.add(
            "task_get_languages()"sv,
            task_get_languages,
            filter,
            params,
            std::move(result_func),
            std::move(except_func)
        );
    }


    void
    get_languages_sync(std::stop_token& stoken,
                       const std::string& filter,
                       const LanguagesParams& params,
                       GetLanguagesResultFunction result_func,
                       ExceptionFunction except_func)
    {
        assert(res);

        try {
            std::string url = get_server() + "/json/languages"s;
            if (!filter.empty())
                url += "/"s + curl::escape(filter);

            std::string params_json;
            glz::ex::write_json(params, params_json);

            throw_if_stopped(stoken);

            rest::request::params_t req_params{
                .accept_content = "application/json"s,
                .error_func =
                    [&except_func]
                    (const std::exception& e)
                        mutable
                    {
                        if (except_func)
                            except_func(e);
                    },
                .post_fields = std::move(params_json),
                .request_content = "application/json"s,
                .response_func =
                    [&stoken, func = std::move(result_func)]
                    (rest::request& req)
                        mutable
                    {
                        throw_if_stopped(stoken);
                        LanguageVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        if (func)
                            func(result);
                    },
                .ssl_verify = !cfg.ignore_ssl_errors,
                .url = url,
                .user_agent = res->user_agent,
                .verbose = cfg.verbose_rest_logs,
            };

            throw_if_stopped(stoken);

            rest::post_sync(std::move(req_params));
        }
        catch (std::exception& e) {
            if (except_func)
                except_func(e);
        }
    }


    void
    get_server_stats(GetServerStatsResultFunction result_func,
                     ExceptionFunction except_func)
    {
        assert(res);

        res->pending_tasks.add(
            "task_get_server_stats()"sv,
            task_get_server_stats,
            std::move(result_func),
            std::move(except_func)
        );
    }


    void
    get_station(const string& uuid,
                GetStationResultFunction result_func,
                ExceptionFunction except_func)
    {
        assert(res);

        res->pending_tasks.add(
            "task_get_station()"sv,
            task_get_station,
            uuid,
            std::move(result_func),
            std::move(except_func)
        );
    }


    void
    get_tags(const TagsParams& params,
             GetTagsResultFunction result_func,
             ExceptionFunction except_func)
    {
        assert(res);
        res->pending_tasks.add(
            "task_get_tags()"sv,
            task_get_tags,
            params,
            std::move(result_func),
            std::move(except_func)
        );
    }


    void
    get_tags_sync(std::stop_token& stoken,
                  const TagsParams& params,
                  GetTagsResultFunction result_func,
                  ExceptionFunction except_func)
    {
        assert(res);

        try {
            // BusyGuard bg{stoken};
            std::string params_json;
            glz::ex::write_json(params, params_json);

            throw_if_stopped(stoken);

            rest::request::params_t req_params{
                .accept_content = "application/json"s,
                .error_func =
                   [&except_func]
                   (const std::exception& e)
                       mutable
                   {
                       if (except_func)
                           except_func(e);
                   },
                .post_fields = std::move(params_json),
                .request_content = "application/json"s,
                .response_func =
                    [&stoken, func = std::move(result_func)]
                    (rest::request& req)
                        mutable
                    {
                        throw_if_stopped(stoken);
                        TagVec result;
                        glz::ex::read<glz_options>(result, req.get_content());
                        throw_if_stopped(stoken);
                        if (func)
                            func(result);
                    },
                .ssl_verify = !cfg.ignore_ssl_errors,
                .url = get_server() + "/json/tags"s,
                .user_agent = res->user_agent,
                .verbose = cfg.verbose_rest_logs,
            };

            throw_if_stopped(stoken);

            rest::post_sync(std::move(req_params));
        }
        catch (std::exception& e) {
            if (except_func)
                except_func(e);
        }
    }


    void
    search_stations(const SearchStationsParams& params,
                    SearchStationsResultFunction result_func,
                    ExceptionFunction except_func)
    {
        assert(res);

        res->pending_tasks.add(
            "task_search_stations()"sv,
            task_search_stations,
            params,
            std::move(result_func),
            std::move(except_func)
        );
    }


    void
    send_click(const string& uuid,
               SendClickResultFunction result_func,
               ExceptionFunction except_func)
    {
        assert(res);

        res->pending_tasks.add(
            "task_send_click()"sv,
            task_send_click,
            uuid,
            std::move(result_func),
            std::move(except_func)
        );
    }


    void
    send_vote(const string& uuid,
              SendVoteResultFunction result_func,
              ExceptionFunction except_func)
    {
        assert(res);

        res->pending_tasks.add(
            "task_send_vote()"sv,
            task_send_vote,
            uuid,
            std::move(result_func),
            std::move(except_func)
        );
    }

} // namespace RadioBrowserAPI
