/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cstdlib>
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


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        fetch_mirrors_thread_function(std::stop_token stopper,
                                      FetchMirrorsResultFunction result_func,
                                      ErrorMsgFunction error_func)
            noexcept;

        rest::error_function_t
        finish_exception(ExceptionFunction except_func);

        template<typename F>
        rest::json_success_function_t
        finish_result(F&& result_func);

        std::minstd_rand
        make_random_engine();

        string
        make_url(const string& endpoint);

        void
        start_call();

        void
        task_get_codecs(const CodecParams& params,
                        GetCodecsResultFunction& result_func,
                        ExceptionFunction& except_func);

        void
        task_get_countries(const CountryParams& params,
                           GetCountriesResultFunction& result_func,
                           ExceptionFunction& except_func);

        void
        task_get_server_stats(GetServerStatsResultFunction& result_func,
                              ExceptionFunction& except_func);

        void
        task_get_station(const string& uuid,
                         GetStationResultFunction& result_func,
                         ExceptionFunction& except_func);

        void
        task_get_tags(const TagParams& params,
                      GetTagsResultFunction& result_func,
                      ExceptionFunction& except_func);

        void
        task_search_stations(const SearchStationParams& params,
                             SearchStationsResultFunction& result_func,
                             ExceptionFunction& except_func);

        void
        task_send_click(const string& uuid,
                        SendClickResultFunction& result_func,
                        ExceptionFunction& except_func);

        void
        task_send_vote(const string& uuid,
                       SendVoteResultFunction& result_func,
                       ExceptionFunction& except_func);

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
                    pending_tasks.add("fetch_mirrors_thread()::result_func",
                                      std::move(result_func),
                                      MirrorsVec{names.begin(), names.end()});
                }
            }
            catch (std::exception& e) {
                string msg = e.what();
                LOG_ERROR("{}", msg);
                if (error_func)
                    pending_tasks.add("fetch_mirrors_thread()::error_func",
                                      std::move(error_func),
                                      std::move(msg));
            }
        }


        // Common code to clear the busy flag.
        rest::error_function_t
        finish_exception(ExceptionFunction except_func)
        {
            return
                [except_func = std::move(except_func)]
                (const std::exception& e)
                    mutable
                {
                    busy = false;
                    if (except_func)
                        except_func(e);
                };
        }


        // Common code to clear the busy flag.
        template<typename F>
        rest::json_success_function_t
        finish_result(F&& result_func)
        {
            return
                [result_func = std::forward<F>(result_func)]
                (const string& json)
                    mutable
                {
                    busy = false;
                    result_func(json);
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


        string
        make_url(const string& endpoint)
        {
            std::string server = current_server.empty() ? start_server : current_server;
            return "http://"s + server + endpoint;
        }


        void
        start_call()
        {
            if (busy)
                throw async_task_queue::call_again{};
            busy = true;
        }


        void
        task_get_codecs(const CodecParams& params,
                        GetCodecsResultFunction& result_func,
                        ExceptionFunction& except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest::post_json_async(
                make_url("/json/codecs"),
                params_json,
                finish_result(
                    [result_func = std::move(result_func)]
                    (const string& json)
                        mutable
                    {
                        CodecVec result;
                        glz::ex::read<glz_options>(result, json);
                        if (result_func)
                            result_func(std::move(result));
                    }
                ),
                finish_exception(std::move(except_func))
            );
        }


        void
        task_get_countries(const CountryParams& params,
                           GetCountriesResultFunction& result_func,
                           ExceptionFunction& except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest::post_json_async(
                make_url("/json/countries"),
                params_json,
                finish_result(
                    [result_func = std::move(result_func)]
                    (const std::string& json)
                        mutable
                    {
                        CountryVec result;
                        glz::ex::read<glz_options>(result, json);
                        if (result_func)
                            result_func(std::move(result));
                    }
                ),
                finish_exception(std::move(except_func))
            );
        }


        void
        task_get_server_stats(GetServerStatsResultFunction& result_func,
                              ExceptionFunction& except_func)
        {
            start_call();

            rest::get_json_async(
                make_url("/json/stats"),
                {},
                finish_result(
                    [result_func = std::move(result_func)]
                    (const std::string& json)
                        mutable
                    {
                        ServerStats result;
                        glz::ex::read<glz_options>(result, json);
                        if (result_func)
                            result_func(std::move(result));
                    }
                ),
                finish_exception(std::move(except_func))
            );
        }


        void
        task_get_station(const string& uuid,
                         GetStationResultFunction& result_func,
                         ExceptionFunction& except_func)
        {
            start_call();

            StationUUIDParams params { .uuids = uuid };
            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest::post_json_async(
                make_url("/json/stations/byuuid"),
                params_json,
                finish_result(
                    [result_func = std::move(result_func)]
                    (const std::string& json)
                        mutable
                    {
                        StationVec result;
                        glz::ex::read<glz_options>(result, json);
                        if (result.size() != 1)
                            throw Error{"incorrect array size: " + std::to_string(result.size())};
                        if (result_func)
                            result_func(std::move(result[0]));
                    }
                ),
                finish_exception(std::move(except_func))
            );
        }


        void
        task_get_tags(const TagParams& params,
                      GetTagsResultFunction& result_func,
                      ExceptionFunction& except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest::post_json_async(
                make_url("/json/tags"),
                params_json,
                finish_result(
                    [result_func = std::move(result_func)]
                    (const std::string& json)
                        mutable
                    {
                        TagVec result;
                        glz::ex::read<glz_options>(result, json);
                        if (result_func)
                            result_func(std::move(result));
                    }
                ),
                finish_exception(std::move(except_func))
            );
        }


        void
        task_search_stations(const SearchStationParams& params,
                             SearchStationsResultFunction& result_func,
                             ExceptionFunction& except_func)
        {
            start_call();

            std::string params_json;
            glz::ex::write_json(params, params_json);

            rest::post_json_async(
                make_url("/json/stations/search"),
                params_json,
                finish_result(
                    [result_func = std::move(result_func)]
                    (const std::string& json)
                        mutable
                    {
                        StationVec result;
                        glz::ex::read<glz_options>(result, json);
                        if (result_func)
                            result_func(std::move(result));
                    }
                ),
                finish_exception(std::move(except_func))
            );
        }


        void
        task_send_click(const string& uuid,
                        SendClickResultFunction& result_func,
                        ExceptionFunction& except_func)
        {
            start_call();

            // Note: clicking does not support GET/POST parameters.
            rest::get_json_async(
                make_url("/json/url/" + uuid),
                {},
                finish_result(
                    [result_func = std::move(result_func)]
                    (const std::string& json)
                        mutable
                    {
                        ClickResult result;
                        glz::ex::read<glz_options>(result, json);
                        if (result_func)
                            result_func(std::move(result));
                    }
                ),
                finish_exception(std::move(except_func))
            );
        }


        void
        task_send_vote(const string& uuid,
                       SendVoteResultFunction& result_func,
                       ExceptionFunction& except_func)
        {
            start_call();

            // NOTE: voting does not support GET/POST parameters.
            rest::get_json_async(
                make_url("/json/vote/" + uuid),
                {},
                finish_result(
                    [result_func = std::move(result_func)]
                    (const std::string& response)
                        mutable
                    {
                        VoteResult result;
                        glz::ex::read<glz_options>(result, response);
                        if (result_func)
                            result_func(std::move(result));
                    }
                ),
                finish_exception(std::move(except_func))
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

        rest::initialize(user_agent);

        fetch_mirrors_thread = {};
        current_server.clear();
        if (!server.empty())
            current_server = server;
        else
            update_mirrors_and_select_random();
    }


    void
    finalize()
    {
        TRACE_FUNC;

        fetch_mirrors_thread = {};

        rest::finalize();
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

        rest::process();
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
                    current_server.clear();
                else {
                    std::vector<string> samples(1);
                    std::ranges::sample(mirrors,
                                        samples.begin(),
                                        1,
                                        random_engine);
                    current_server = std::move(samples[0]);
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
        pending_tasks.add("task_get_codecs()",
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
        pending_tasks.add("task_get_countries()",
                          task_get_countries,
                          params,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    get_server_stats(GetServerStatsResultFunction result_func,
                     ExceptionFunction except_func)
    {
        pending_tasks.add("task_get_server_stats()",
                          task_get_server_stats,
                          std::move(result_func),
                          std::move(except_func));
    }


    void
    get_station(const string& uuid,
                GetStationResultFunction result_func,
                ExceptionFunction except_func)
    {
        pending_tasks.add("task_get_station()",
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
        pending_tasks.add("task_get_tags()",
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
        pending_tasks.add("task_search_stations()",
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
        pending_tasks.add("task_send_click()",
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
        pending_tasks.add("task_send_vote()",
                          task_send_vote,
                          uuid,
                          std::move(result_func),
                          std::move(except_func));
    }

} // namespace RadioBrowserAPI
