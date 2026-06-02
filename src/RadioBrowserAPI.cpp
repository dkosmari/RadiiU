#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <unordered_set>

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

    constexpr const glz::opts custom_glz_options{
        .error_on_unknown_keys = false,
        .prettify = true,
    };


    bool busy;

    thread_safe<string> server;
    thread_safe<MirrorsVec> mirrors;


    struct WorkerState {

        enum class Task {
            none,
            connect,
            fetch_mirrors,
            fetch_mirrors_and_connect,
        };

        Task task = Task::none;
        bool pending_complete = false;
        std::jthread thread = {};
        std::minstd_rand random_engine;
        result_function_t<> result_func = {};
        error_function_t error_func = {};
        std::string response = {};
        std::string error_msg = {};

        void
        stop();

        void
        prepare_task(Task t,
                     result_function_t<> rf,
                     error_function_t ef);

        void
        complete_task()
            noexcept;

        void
        call_result_func()
            noexcept;

        void
        call_error_func(const std::exception& e)
            noexcept;

        void
        ensure_no_task();

    }; // struct WorkerState

    thread_safe<WorkerState> worker_state;


    namespace {

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
            auto random_engine = make_random_engine();
            std::ranges::shuffle(result, random_engine);

            return result;
        }


        struct StatusResponse {
            bool result;
            std::string response;
        };

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


        void
        ensure_not_busy()
        {
            if (busy)
                throw error{"RadioBrowserAPI is busy"};
        }

    } // namespace


    string
    to_string(WorkerState::Task t)
    {
        switch (t) {
            using enum WorkerState::Task;
            case none:
                return "none";
            case connect:
                return "connect";
            case fetch_mirrors:
                return "fetch_mirrors";
            case fetch_mirrors_and_connect:
                return "fetch_mirrors_and_connect";
            default:
                return "<INVALID>";
        }
    }


    void
    WorkerState::stop()
    {
        thread = {};
        task = Task::none;
        pending_complete = false;
    }


    void
    WorkerState::prepare_task(Task t,
                              result_function_t<> rf,
                              error_function_t ef)
    {
        result_func = std::move(rf);
        error_func = std::move(ef);
        error_msg.clear();
        response.clear();
        pending_complete = false;
        task = t;
    }


    void
    WorkerState::complete_task()
        noexcept
    {
        if (!pending_complete)
            return;
        if (error_msg.empty())
            call_result_func();
        else
            call_error_func(std::runtime_error{error_msg});
        error_msg.clear();
        response.clear();
        // thread = {};
        task = Task::none;
        pending_complete = false;
    }


    void
    WorkerState::call_result_func()
        noexcept
    {
        try {
            if (result_func)
                result_func();
        }
        catch (std::exception& e) {
            call_error_func(e);
        }
    }


    void
    WorkerState::call_error_func(const std::exception& e)
        noexcept
    {
        if (error_func)
            error_func(e, response);
    }


    void
    WorkerState::ensure_no_task()
    {
        if (task != Task::none)
            throw error{"WorkerState::task should be none, but is "s + to_string(task)};
    }



    void
    initialize(const string& user_agent)
    {
        TRACE_FUNC;

        busy = false;

        rest::initialize(user_agent);
    }


    void
    finalize()
    {
        TRACE_FUNC;

        {
            auto ws = worker_state.lock();
            ws->stop();
        }

        busy = false;

        rest::finalize();
    }


    void
    process()
    {
        if (auto ws = worker_state.try_lock())
            ws->complete_task();

        rest::process();
    }


    bool
    is_busy()
    {
        return busy;
    }


    void
    set_server(const string& address)
    {
        server.store(address);
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

        using Task = WorkerState::Task;
        auto ws = worker_state.lock();
        try {
            ws->ensure_no_task();
        }
        catch (error& e) {
            cout << "ERROR: " << e.what() << endl;
            if (error_func)
                error_func(e, {});
            return;
        }

        ws->prepare_task(Task::fetch_mirrors,
                         std::move(result_func),
                         std::move(error_func));
        ws->thread = std::jthread{
            [](std::stop_token stopper)
            {
                try {
                    auto new_mirrors = get_mirrors_sync(stopper);
                    mirrors.store(std::move(new_mirrors));
                }
                catch (std::exception& e) {
                    auto ws = worker_state.lock();
                    ws->error_msg = e.what();
                }
                auto ws = worker_state.lock();
                ws->pending_complete = true;
            }
        };
    }


    MirrorsVec
    current_mirrors()
    {
        return mirrors.load();
    }


    void
    connect(result_function_t<> result_func,
            error_function_t error_func)
    {
        TRACE_FUNC;

        using Task = WorkerState::Task;
        auto ws = worker_state.lock();
        try {
            ws->ensure_no_task();
        }
        catch (error& e) {
            cout << "ERROR: " << e.what() << endl;
            if (error_func)
                error_func(e, {});
            return;
        }

        ws->prepare_task(Task::connect,
                         std::move(result_func),
                         std::move(error_func));

        ws->thread = std::jthread{
            [](std::stop_token stopper)
            {
                try {
                    auto srv = server.load();
                    if (srv.empty()) {
                        // No preferred server, use a random one.
                        auto mir = mirrors.load();
                        if (mir.empty()) {
                            // If no mirrors list yet, fetch it.
                            mir = get_mirrors_sync(stopper);
                            mirrors.store(mir);
                        }
                        bool success = false;
                        for (auto name : mir) {
                            if (stopper.stop_requested())
                                throw error{"stop requested"};
                            auto [test_result, test_response] = test_server(name);
                            worker_state.lock()->response = std::move(test_response);
                            if (test_result) {
                                server.store(name);
                                success = true;
                                break;
                            }
                        }
                        if (!success)
                            throw error{"no working mirror found"};
                    } else {
                        // We have a preferred server.
                        auto [test_result, test_response] = test_server(srv);
                        worker_state.lock()->response = std::move(test_response);
                        if (test_result)
                            server.store(srv);
                        else
                            throw error{"server "s + srv + " did not respond"s};
                    }
                }
                catch (std::exception& e) {
                    auto ws = worker_state.lock();
                    ws->error_msg = e.what();
                }
                auto ws = worker_state.lock();
                ws->pending_complete = true;
            }
        };
    }


    void
    get_codecs(const CodecParams& params,
               result_function_t<CodecVec> result_func,
               error_function_t error_func)
    {
        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/codecs"),
            params_json,
            [result_func = std::move(result_func)](const std::string& response)
                mutable
            {
                CodecVec result;
                glz::ex::read<custom_glz_options>(result, response);
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
        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/countries"),
            params_json,
            [result_func = std::move(result_func)](const std::string& response)
                mutable
            {
                CountryVec result;
                glz::ex::read<custom_glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }


    void
    get_server_stats(result_function_t<ServerStats> result_func,
                     error_function_t error_func)
    {
        rest::get_json_async(
            make_url("/json/stats"),
            {},
            [result_func = std::move(result_func)](const std::string& response)
                mutable
            {
                ServerStats result;
                glz::ex::read<custom_glz_options>(result, response);
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
                glz::ex::read<custom_glz_options>(result, response);
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
        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/tags"),
            params_json,
            [result_func=std::move(result_func)](const std::string& response)
                mutable
            {
                TagVec result;
                glz::ex::read<custom_glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }


    void
    search_stations(const SearchStationParams& params,
                    result_function_t<StationVec> result_func,
                    error_function_t error_func)
    {
        ensure_not_busy();

        std::string params_json;
        glz::ex::write_json(params, params_json);

        busy = true;
        rest::post_json_async(
            make_url("/json/stations/search"),
            params_json,
            [result_func=std::move(result_func)](const std::string& response)
                mutable
            {
                busy = false;
                StationVec result;
                glz::ex::read<custom_glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            [error_func=std::move(error_func)](const std::exception& e,
                                               const std::string& response)
                mutable
            {
                busy = false;
                if (error_func)
                    error_func(e, response);
            });
    }


    void
    send_click(const string& uuid,
               result_function_t<ClickResult> result_func,
               error_function_t error_func)
    {
        if (uuid.empty())
            return;

        ClickParams params{ .stationuuid = uuid };
        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/url"),
            params_json,
            [result_func=std::move(result_func)](const std::string& response)
                mutable
            {
                ClickResult result;
                glz::ex::read<custom_glz_options>(result, response);
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

        VoteParams params{ .stationuuid = uuid };
        std::string params_json;
        glz::ex::write_json(params, params_json);

        rest::post_json_async(
            make_url("/json/vote"),
            params_json,
            [result_func=std::move(result_func)](const std::string& response)
                mutable
            {
                VoteResult result;
                glz::ex::read<custom_glz_options>(result, response);
                if (result_func)
                    result_func(std::move(result));
            },
            std::move(error_func));
    }

} // namespace RadioBrowserAPI
