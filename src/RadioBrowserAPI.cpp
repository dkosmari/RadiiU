/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <memory>
#include <queue>
#include <random>
#include <ranges>
#include <set>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <unordered_set>
#include <utility>

#ifdef __WIIU__
#include <coreinit/time.h>
#endif

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
#include "TraceDuration.hpp"
#include "TraceFunction.hpp"
#include "TraceManager.hpp"
#include "tracer.hpp"


using namespace std::literals;


template<>
struct glz::meta<RadioBrowserAPI::CodecParams::Order> {
    using enum RadioBrowserAPI::CodecParams::Order;
    static constexpr
    auto value = enumerate(name, stationcount);
};


template<>
struct glz::meta<RadioBrowserAPI::CountryParams::Order> {
    using enum RadioBrowserAPI::CountryParams::Order;
    static constexpr
    auto value = enumerate(name, stationcount);
};


template<>
struct glz::meta<RadioBrowserAPI::SearchStationParams::Order> {
    using enum RadioBrowserAPI::SearchStationParams::Order;
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
struct glz::meta<RadioBrowserAPI::StationParams::Order> {
    using enum RadioBrowserAPI::StationParams::Order;
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
struct glz::meta<RadioBrowserAPI::TagParams::Order> {
    using enum RadioBrowserAPI::TagParams::Order;
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


        /*-----------*/
        /* Constants */
        /*-----------*/

        constexpr
        const glz::opts glz_options{ .error_on_unknown_keys = false, };

        const string start_server = "all.api.radio-browser.info";


        /*-----------*/
        /* Variables */
        /*-----------*/

        bool busy;
        string current_server;
        std::minstd_rand random_engine;
        MirrorsVec mirrors;
        std::jthread fetch_mirrors_thread;
        async_task_queue pending_tasks;
        std::optional<rest::manager> rest_manager;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        fetch_mirrors_thread_function(std::stop_token stopper,
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
        task_get_codecs(const CodecParams& params,
                        GetCodecsResultFunction result_func,
                        ExceptionFunction except_func);

        void
        task_get_countries(const CountryParams& params,
                           GetCountriesResultFunction result_func,
                           ExceptionFunction except_func);

        void
        task_get_server_stats(GetServerStatsResultFunction result_func,
                              ExceptionFunction except_func);

        void
        task_get_station(const string& uuid,
                         GetStationResultFunction result_func,
                         ExceptionFunction except_func);

        void
        task_get_tags(const TagParams& params,
                      GetTagsResultFunction result_func,
                      ExceptionFunction except_func);

        void
        task_search_stations(const SearchStationParams& params,
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

        void
        throw_if_stopped(std::stop_token& stopper);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        fetch_mirrors_thread_function(std::stop_token stopper,
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

                    throw_if_stopped(stopper);

                    if (ar.error.message)
                        throw Error{"failed resolving \""
                                    + server
                                    + "\": "
                                    + *ar.error.message};
                    for (const auto& entry : ar.result.entries)
                        addresses.insert(entry.addr);
                }

                LOG_DEBUG("Found {} mirrors.", addresses.size());

                throw_if_stopped(stopper);

                // Step 2: find the canonical names for each IP
                std::set<string> names;
                {
                    TraceDuration trace_canonical{
                        "fetch_mirrors_thread_function()/resolve canonical names"sv,
                        "RadioBrowserAPI,fetch_mirrors_thread"sv
                    };
                    net::resolver::name_resolver nr;
                    for (const auto& addr : addresses) {
                        throw_if_stopped(stopper);
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

                throw_if_stopped(stopper);

                // Step 3: Invoke the result callback.
                if (result_func) {
                    pending_tasks.add("fetch_mirrors_thread()::result_func"sv,
                                      std::move(result_func),
                                      MirrorsVec{names.begin(), names.end()});
                }
            }
            catch (std::exception& e) {
                string msg = e.what();
                LOG_ERROR("{}", msg);
                if (error_func)
                    pending_tasks.add("fetch_mirrors_thread()::error_func"sv,
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
            assert(rest_manager);

            if (busy)
                throw async_task_queue::call_again{};
            busy = true;
        }


        void
        task_get_codecs(const CodecParams& params,
                        GetCodecsResultFunction result_func,
                        ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest_manager->post_json(
                "/json/codecs",
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
        task_get_countries(const CountryParams& params,
                           GetCountriesResultFunction result_func,
                           ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest_manager->post_json(
                "/json/countries",
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
        task_get_server_stats(GetServerStatsResultFunction result_func,
                              ExceptionFunction except_func)
        {
            start_call();

            rest_manager->get_json(
                "/json/stats",
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

            rest_manager->post_json(
                "/json/stations/byuuid",
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
        task_get_tags(const TagParams& params,
                      GetTagsResultFunction result_func,
                      ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest_manager->post_json(
                "/json/tags",
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
        task_search_stations(const SearchStationParams& params,
                             SearchStationsResultFunction result_func,
                             ExceptionFunction except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest_manager->post_json(
                "/json/stations/search",
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
            rest_manager->get_json(
                "/json/url/" + uuid,
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
            rest_manager->get_json(
                "/json/vote/" + uuid,
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


        void
        throw_if_stopped(std::stop_token& stopper)
        {
            if (stopper.stop_requested())
                throw Error{"stop requested"};
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

        random_engine = make_random_engine();

        busy = false;

        fetch_mirrors_thread = {};
        current_server.clear();
        if (!server.empty())
            current_server = server;
        else
            update_mirrors_and_select_random();

        rest::manager::config rest_config;
        rest_config.user_agent = user_agent;
        rest_manager.emplace(std::move(rest_config), get_server());
    }


    void
    finalize()
    {
        TRACE_FUNC;

        fetch_mirrors_thread = {};

        rest_manager.reset();
    }


    void
    process()
    {
        try {
            pending_tasks.try_dispatch_one();
        }
        catch (async_task_queue::error& e) {
            LOG_ERROR("Dispatching RadioBrowerAPI task {}: {}", e.name, e.what());
        }

        assert(rest_manager);
        rest_manager->process();
    }


    bool
    is_busy()
    {
        return busy;
    }


    void
    set_server(const string& server)
    {
        current_server = server;
        rest_manager->set_base_url("http://"s + get_server());
    }


    string
    get_server()
    {
        return current_server.empty() ? start_server : current_server;
    }


    void
    fetch_mirrors(FetchMirrorsResultFunction result_func,
                  ErrorMsgFunction error_func)
    {
        TRACE_FUNC;

        fetch_mirrors_thread = std::jthread{fetch_mirrors_thread_function,
                                            std::move(result_func),
                                            std::move(error_func)};
    }


    void
    update_mirrors()
    {
        fetch_mirrors(
            [](MirrorsVec result)
            {
                mirrors = std::move(result);
            }
        );
    }


    void
    update_mirrors_and_select_random()
    {
        fetch_mirrors(
            [](MirrorsVec result)
            {
                mirrors = std::move(result);
                if (mirrors.empty())
                    set_server("");
                else {
                    std::vector<string> samples(1);
                    std::ranges::sample(mirrors,
                                        samples.begin(),
                                        1,
                                        random_engine);
                    set_server(samples[0]);
                }
            }
        );
    }


    void
    for_each_mirror(ForEachMirrorFunction func)
    {
        if (func)
            for (const auto& server : mirrors)
                func(server);
    }


    void
    get_codecs(const CodecParams& params,
               GetCodecsResultFunction result_func,
               ExceptionFunction except_func)
    {
        pending_tasks.add("task_get_codecs()"sv,
                          task_get_codecs,
                          params,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    get_countries(const CountryParams& params,
                  GetCountriesResultFunction result_func,
                  ExceptionFunction except_func)
    {
        pending_tasks.add("task_get_countries()"sv,
                          task_get_countries,
                          params,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    get_server_stats(GetServerStatsResultFunction result_func,
                     ExceptionFunction except_func)
    {
        pending_tasks.add("task_get_server_stats()"sv,
                          task_get_server_stats,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    get_station(const string& uuid,
                GetStationResultFunction result_func,
                ExceptionFunction except_func)
    {
        pending_tasks.add("task_get_station()"sv,
                          task_get_station,
                          uuid,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    get_tags(const TagParams& params,
             GetTagsResultFunction result_func,
             ExceptionFunction except_func)
    {
        pending_tasks.add("task_get_tags()"sv,
                          task_get_tags,
                          params,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    search_stations(const SearchStationParams& params,
                    SearchStationsResultFunction result_func,
                    ExceptionFunction except_func)
    {
        pending_tasks.add("task_search_stations()"sv,
                          task_search_stations,
                          params,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    send_click(const string& uuid,
               SendClickResultFunction result_func,
               ExceptionFunction except_func)
    {
        pending_tasks.add("task_send_click()"sv,
                          task_send_click,
                          uuid,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    send_vote(const string& uuid,
              SendVoteResultFunction result_func,
              ExceptionFunction except_func)
    {
        pending_tasks.add("task_send_vote()"sv,
                          task_send_vote,
                          uuid,
                          std::move(result_func),
                          std::move(except_func));
    }

} // namespace RadioBrowserAPI
