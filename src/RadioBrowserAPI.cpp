#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <random>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <future>
#include <stop_token>

#ifdef __WIIU__
#include <coreinit/time.h>
#endif

#include <glaze/json.hpp>
#include <glaze/json/generic.hpp>
#include <glaze/exceptions/core_exceptions.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include "RadioBrowserAPI.hpp"

#include "net/address.hpp"
#include "net/resolver.hpp"
#include "rest.hpp"
#include "thread_safe.hpp"
#include "tracer.hpp"


using std::cout;
using std::endl;

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
    auto value = enumerate(name,
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
                           random);
};


template<>
struct glz::meta<RadioBrowserAPI::StationParams::Order> {
    using enum RadioBrowserAPI::StationParams::Order;
    static constexpr
    auto value = enumerate(name,
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
                           random);
};


template<>
struct glz::meta<RadioBrowserAPI::TagParams::Order> {
    using enum RadioBrowserAPI::TagParams::Order;
    static constexpr
    auto value = enumerate(name, stationcount);
};


namespace RadioBrowserAPI {

    namespace {

        enum class State {
            disconnected,
            connecting,
            connected,
        };


        struct StatusResponse {
            bool result;
            std::string response;
        };


        string
        to_string(State st);


        constexpr
        const glz::opts glz_options{
            .error_on_unknown_keys = false,
            .prettify = true,
        };


        State state;
        bool searching;
        thread_safe<string> server;
        thread_safe<std::minstd_rand> random_engine;
        thread_safe<MirrorsVec> mirrors;     // TODO: consider not caching the mirrors.
        std::jthread connect_thread;
        std::jthread mirrors_thread;

        using pending_call_t = std::future<void>;
        using pending_call_list_t = std::deque<pending_call_t>;
        thread_safe<pending_call_list_t> pending_calls;


        string
        make_url(const string& endpoint)
        {
            string current_server = server.load();
            if (current_server.empty()) {
                auto m = mirrors.lock();
                if (m->empty())
                    throw error{"no server, no mirrors to build URL"};
                current_server = m->front();
            }
            return "http://"s + current_server + endpoint;
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


        MirrorsVec
        get_mirrors_sync(std::stop_token stopper)
        {
            std::vector<net::address> addresses;
            std::unordered_set<string> mirrors_set; // this will eliminate dupes

            // find all IP addresses
            {
                net::resolver::address_resolver ar;
                ar.param.type = net::socket::type::tcp;
                string address = "all.api.radio-browser.info";
                ar.process(address);

                if (stopper.stop_requested())
                    throw error{"stop requested"};

                if (ar.error.message)
                    throw error{"failed resolving \""
                                + address
                                + "\": "
                                + *ar.error.message};
                for (const auto& entry : ar.result.entries)
                    addresses.push_back(entry.addr);
            }

            // cout << "Found " << addresses.size() << " mirrors" << endl;

            // now find the canonical names for each IP address
            {
                net::resolver::name_resolver nr;
                for (const auto& addr : addresses) {
                    try {
                        nr.process(addr);

                        if (stopper.stop_requested())
                            throw error{"stop requested"};

                        if (nr.error.message)
                            throw error{"failed to look up name for \""
                                        + to_string(addr) + "\""};
                        if (nr.result.name)
                            mirrors_set.insert(std::move(*nr.result.name));
                    }
                    catch (std::exception& e) {
                        cout << "ERROR: " << e.what() << endl;
                    }
                }
            }

            if (stopper.stop_requested())
                throw error{"stop requested"};

            MirrorsVec result;
            result.reserve(mirrors_set.size());
            for (const auto& name : mirrors_set)
                result.push_back(name);

            // Make sure the result is randomized.
            auto re = random_engine.lock();
            std::ranges::shuffle(result, *re);

            return result;
        }


        StatusResponse
        test_server(const std::string& srv)
        {
            std::string result;
            try {
                result = rest::get_json_sync("https://" + srv + "/json/stats");
                // cout << "connect() thread obtained server stats:\n"
                //      << glz::prettify_json(result)
                //      << endl;
                return {true, std::move(result)};
            }
            catch (std::exception& e) {
                cout << "Failed to connect to " << srv << ": " << e.what() << endl;
                return {false, std::move(result)};
            }
        }


        template<typename F,
                 typename... Args>
        void
        defer_call(F&& func,
                   Args&& ...args)
        {
            if (func) {
                auto pc = pending_calls.lock();
                auto fut = std::async(std::launch::deferred,
                                      std::forward<F>(func),
                                      std::forward<Args>(args)...);
                pc->push_back(std::move(fut));
            }
        }


        void
        perform_deferred_calls()
        {
            pending_call_list_t local_pending_calls;
            if (auto pc = pending_calls.try_lock()) {
                if (!pc->empty())
                    local_pending_calls = std::move(*pc);
            }
            for (auto& item : local_pending_calls)
                try {
                    cout << "making deferred call" << endl;
                    item.get(); // make the pending calls
                }
                catch (std::exception& e) {
                    cout << "BUG: issuing pending call threw: " << e.what() << endl;
                }
        }


        string
        to_string(State st)
        {
            switch (st) {
                using enum State;
                case disconnected:
                    return "disconnected";
                case connecting:
                    return "connecting";
                case connected:
                    return "connected";
                default:
                    return "<ERROR>";
            }
        }


        struct shared_error_function_t {

            std::shared_ptr<error_function_t> error_func;

            shared_error_function_t(error_function_t error_func) :
                error_func{std::make_shared<error_function_t>(std::move(error_func))}
            {}

            // Copy constructor.
            shared_error_function_t(const shared_error_function_t& other)
                noexcept = default;

            // Move constructor.
            shared_error_function_t(shared_error_function_t&& other)
                noexcept = default;

            // Copy assignment.
            shared_error_function_t&
            operator =(const shared_error_function_t& other)
                noexcept = default;

            // Move assignment.
            shared_error_function_t&
            operator =(shared_error_function_t&& other)
                noexcept = default;

            void
            operator ()(const std::exception& e)
            {
                if (error_func && *error_func)
                    (*error_func)(e);
            }

        }; // struct shared_error_function_t


        /*
         * This performs a connect() IF necessary, and then a given API call.
         */
        template<typename P,
                 typename... R>
        void
        when_connected(auto api_func,
                       const P& params,
                       result_function_t<R...> result_func,
                       auto error_func)
        {
            switch (state) {
                case State::connecting:
                    // If in connecting state, just defer the call.
                    defer_call(std::move(api_func),
                               params,
                               std::move(result_func),
                               std::move(error_func));
                    break;
                case State::disconnected: {
                    // If disconnected, do a connect then call the handlers.

                    // NOTE: error_func is move-only, so we move it into a shared_ptr in order
                    // to use it twice.
                    shared_error_function_t shared_error_func{std::move(error_func)};
                    connect(
                        [api_func,
                         params,
                         result_func = std::move(result_func),
                         shared_error_func]
                        mutable
                        {
                            std::invoke(api_func,
                                        params,
                                        std::move(result_func),
                                        std::move(*shared_error_func.error_func));
                        },
                        shared_error_func);
                    break;
                }

                case State::connected:
                    // If connected: should not have called when_connected(), but let's
                    // tolerate it.
                    std::invoke(api_func,
                                params,
                                std::move(result_func),
                                std::move(error_func));
                    break;

                default:
                    std::abort();
            }
        }


        /*
         * Overload when there are no params for the API call.
         */
        template<typename... R>
        void
        when_connected(auto api_func,
                       result_function_t<R...> result_func,
                       auto error_func)
        {
            switch (state) {
                case State::connecting:
                    // If in connecting state, just defer the call.
                    defer_call(std::move(api_func),
                               std::move(result_func),
                               std::move(error_func));
                    break;

                case State::disconnected: {
                    // If disconnected, do a connect then call the handlers.

                    // NOTE: error_func is move-only, so we move it into a shared_ptr in order
                    // to use it twice.
                    shared_error_function_t shared_error_func{std::move(error_func)};
                    connect(
                        [api_func,
                         result_func = std::move(result_func),
                         shared_error_func]
                        mutable
                        {
                            std::invoke(api_func,
                                        std::move(result_func),
                                        std::move(*shared_error_func.error_func));
                        },
                        shared_error_func);
                    break;
                }

                case State::connected:
                    // If connected: should not have called when_connected(), but let's
                    // tolerate it.
                    std::invoke(api_func,
                                std::move(result_func),
                                std::move(error_func));
                    break;

                default:
                    std::abort();
            }
        }

    } // namespace


    error::error(const std::exception& e) :
        std::runtime_error{e.what()}
    {}



    /*
     * This performs a connection "test" on a background thread:
     * - if there's no server set:
     *   - two DNS lookups are used to find a list of all mirrors.
     *   - the list is randomized
     *   - a synchronous server stats call is done on each until one mirror succeeds.
     * - if there's a server:
     *   - a synchronous server stats call is done to see if it's online
     *
     * Then, back on the main thread (during a RadioBrowser::process() call) either the
     * `result_func' or the `error_func' is invoked.
     */
    void
    connect(result_function_t<> result_func,
            error_function_t error_func)
    {
        TRACE_FUNC;

        // TODO: allow connect() again after it's already connected?
        if (state == State::connected) {
            if (result_func)
                result_func();
            return;
        }

        if (state == State::connecting) {
            if (error_func)
                error_func(error{"connection in progress"});
            return;
        }

        connect_thread = std::jthread{
            [](std::stop_token stopper,
               result_function_t<> result_func,
               error_function_t error_func)
            {
                try {
                    auto srv = server.load();
                    if (srv.empty()) {
                        // No server set, use a random one from the mirrors list.
                        auto local_mirrors = mirrors.load();
                        if (local_mirrors.empty()) {
                            // If no mirrors list yet, fetch it.
                            local_mirrors = get_mirrors_sync(stopper);
                            mirrors.store(local_mirrors);
                        }
                        bool success = false;
                        for (const auto& mirror : local_mirrors) {
                            if (stopper.stop_requested())
                                throw error{"stop requested"};
                            auto [test_success, test_response] = test_server(mirror);
                            if (test_success) {
                                server.store(mirror);
                                success = true;
                                break;
                            }
                        }
                        if (!success)
                            throw error{"no working mirror found"};
                        // Defer the result_func call, after updating the connection state.
                        defer_call(
                            [](result_function_t<> result_func)
                            {
                                cout << "DEBUG: connection succeeded [1]!" << endl;
                                state = State::connected;
                                cout << "state = " << to_string(state) << endl;
                                if (result_func)
                                    std::invoke(result_func);
                            },
                            std::move(result_func));
                    } else {
                        // We have a server.
                        auto [test_success, test_response] = test_server(srv);
                        if (!test_success)
                            throw error{"mirror "s + srv + " failed with:\n"s + test_response};

                        server.store(srv);
                        // Defer the result_func call, after updating the connection state.
                        defer_call(
                            [](result_function_t<> result_func)
                            {
                                cout << "DEBUG: connection succeeded [2]!" << endl;
                                state = State::connected;
                                cout << "state = " << to_string(state) << endl;
                                if (result_func)
                                    std::invoke(result_func);
                            },
                            std::move(result_func));
                    }
                }
                catch (std::exception& e) {
                    // Defer the error_func call, after updating the connection state.
                    defer_call(
                        [](error_function_t error_func,
                            const error& e)
                        {
                            state = State::disconnected;
                            cout << "state = " << to_string(state) << endl;
                            if (error_func)
                                std::invoke(error_func, e);
                        },
                        std::move(error_func),
                        error{e});
                }
            },
            std::move(result_func),
            std::move(error_func)
        };
    }


    MirrorsVec
    current_mirrors()
    {
        return mirrors.load();
    }



    void
    initialize(const string& user_agent)
    {
        TRACE_FUNC;

        random_engine.store(make_random_engine());

        state = State::disconnected;
        searching = false;

        rest::initialize(user_agent);
    }


    void
    finalize()
    {
        TRACE_FUNC;

        connect_thread = {};
        mirrors_thread = {};

        searching = false;
        state = State::disconnected;

        rest::finalize();
    }


    void
    process()
    {
        perform_deferred_calls();
        rest::process();
    }


    bool
    is_searching()
    {
        return searching;
    }


    void
    set_server(const string& new_server)
    {
        switch (state) {

            case State::disconnected:
                break;

            case State::connecting:
                connect_thread = {};
                if (new_server.empty())
                    state = State::disconnected;
                else
                    state = State::connected;
                break;

            default:
                std::abort();

            case State::connected:
                if (new_server.empty())
                    state = State::disconnected;
                break;

        }

        auto s = server.lock();
        *s = new_server;
    }


    string
    get_server()
    {
        return server.load();
    }


    void
    get_mirrors(result_function_t<> result_func,
                error_function_t error_func)
    {
        TRACE_FUNC;

        mirrors_thread = std::jthread{
            [](std::stop_token stopper,
               result_function_t<> result_func,
               error_function_t error_func)
            {
                try {
                    auto new_mirrors = get_mirrors_sync(stopper);
                    mirrors.store(std::move(new_mirrors));
                    defer_call(std::move(result_func));
                }
                catch (std::exception& e) {
                    defer_call(std::move(error_func), error{e});
                }
            },
            std::move(result_func),
            std::move(error_func)
        };
    }


    void
    get_codecs(const CodecParams& params,
               result_function_t<CodecVec> result_func,
               error_function_t error_func)
    {
        if (state != State::connected) {
            when_connected(get_codecs,
                           params,
                           std::move(result_func),
                           std::move(error_func));
            return;
        }

        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/codecs"),
            params_json,
            [result_func = std::move(result_func)](const std::string& response)
                mutable
            {
                CodecVec result;
                glz::ex::read<glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }


    void
    get_countries(const CountryParams& params,
                  result_function_t<CountryVec> result_func,
                  error_function_t error_func)
    {
        if (state != State::connected) {
            when_connected(get_countries,
                           params,
                           std::move(result_func),
                           std::move(error_func));
            return;
        }

        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/countries"),
            params_json,
            [result_func = std::move(result_func)](const std::string& response)
                mutable
            {
                CountryVec result;
                glz::ex::read<glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }


    void
    get_server_stats(result_function_t<ServerStats> result_func,
                     error_function_t error_func)
    {
        if (state != State::connected) {
            cout << "DEBUG: calling get_server_stats() after connection" << endl;
            when_connected(get_server_stats,
                           std::move(result_func),
                           std::move(error_func));
            return;
        }

        rest::get_json_async(
            make_url("/json/stats"),
            {},
            [result_func = std::move(result_func)](const std::string& response)
                mutable
            {
                ServerStats result;
                glz::ex::read<glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }


    void
    get_station(const string& uuid,
                result_function_t<Station> result_func,
                error_function_t error_func)
    {
        if (state != State::connected) {
            when_connected(get_station,
                           uuid,
                           std::move(result_func),
                           std::move(error_func));
            return;
        }

        StationUUIDParams params { .uuids = uuid };
        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/stations/byuuid"),
            params_json,
            [result_func = std::move(result_func)](const std::string& response)
                mutable
            {
                StationVec result;
                glz::ex::read<glz_options>(result, response);
                if (result.size() != 1)
                    throw error{"incorrect array size: " + std::to_string(result.size())};
                if (result_func)
                    result_func(std::move(result[0]));
            },
            std::move(error_func));
    }


    void
    get_tags(const TagParams& params,
             result_function_t<TagVec> result_func,
             error_function_t error_func)
    {
        if (state != State::connected) {
            when_connected(get_tags,
                           params,
                           std::move(result_func),
                           std::move(error_func));
            return;
        }

        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/tags"),
            params_json,
            [result_func=std::move(result_func)](const std::string& response)
                mutable
            {
                TagVec result;
                glz::ex::read<glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }


    void
    reconnect()
    {
        state = State::disconnected;
        connect();
    }


    void
    search_stations(const SearchStationParams& params,
                    result_function_t<StationVec> result_func,
                    error_function_t error_func)
    {
        if (searching) {
            error e{"RadioBrowserAPI is searching"};
            if (error_func)
                error_func(e);
            return;
        }

        if (state != State::connected) {
            when_connected(search_stations,
                           params,
                           std::move(result_func),
                           std::move(error_func));
            return;
        }

        std::string params_json;
        glz::ex::write_json(params, params_json);

        searching = true;
        rest::post_json_async(
            make_url("/json/stations/search"),
            params_json,
            [result_func=std::move(result_func)](const std::string& response)
                mutable
            {
                searching = false;
                StationVec result;
                glz::ex::read<glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            [error_func=std::move(error_func)](const std::exception& e)
                mutable
            {
                searching = false;
                if (error_func)
                    error_func(e);
            });
    }


    void
    send_click(const string& uuid,
               result_function_t<ClickResult> result_func,
               error_function_t error_func)
    {
        if (uuid.empty())
            return;

        if (state != State::connected) {
            when_connected(send_click,
                           uuid,
                           std::move(result_func),
                           std::move(error_func));
            return;
        }

        // Note: clicking does not support GET/POST parameters.
        rest::get_json_async(
            make_url("/json/url/" + uuid),
            {},
            [result_func=std::move(result_func)](const std::string& response)
                mutable
            {
                ClickResult result;
                glz::ex::read<glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }


    void
    send_vote(const string& uuid,
              result_function_t<VoteResult> result_func,
              error_function_t error_func)
    {
        if (uuid.empty())
            return;

        if (state != State::connected) {
            when_connected(send_vote,
                           uuid,
                           std::move(result_func),
                           std::move(error_func));
            return;
        }

        // NOTE: voting does not support GET/POST parameters.
        rest::get_json_async(
            make_url("/json/vote/" + uuid),
            {},
            [result_func=std::move(result_func)](const std::string& response)
                mutable
            {
                VoteResult result;
                glz::ex::read<glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }

} // namespace RadioBrowserAPI
