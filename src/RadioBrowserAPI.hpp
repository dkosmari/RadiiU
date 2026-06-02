#ifndef RADIO_BROWSER_API_HPP
#define RADIO_BROWSER_API_HPP

#include <cstdint>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>


namespace RadioBrowserAPI {

    using string = std::string;

    using StringVec = std::vector<string>;

    using opt_bool    = std::optional<bool>;
    using opt_string  = std::optional<string>;
    using opt_double  = std::optional<double>;
    using opt_strings = std::optional<StringVec>;
    using opt_uint = std::optional<unsigned>;


    struct error : std::runtime_error {
        using std::runtime_error::runtime_error;
    }; // struct error



    struct ClickParams {
        string stationuuid;
    }; // stuct ClickParams

    struct ClickResult {
        bool ok;
        string message;
        string stationuuid;
        string name;
        string url;
    }; // struct Click



    struct CodecParams {
        enum class Order {
            name,
            stationcount,
        };

        using opt_order = std::optional<Order>;

        opt_order order      = {};
        opt_bool  reverse    = {};
        opt_bool  hidebroken = {};
        opt_uint  offset     = {};
        opt_uint  limit      = {};
    }; // struct CodecParams


    struct Codec {
        string   name;
        unsigned stationcount;
    }; // struct Codec


    using CodecVec = std::vector<Codec>;


    struct CountryParams {
        enum class Order {
            name,
            stationcount,
        };

        using opt_order = std::optional<Order>;

        opt_order order      = {};
        opt_bool  reverse    = {};
        opt_bool  hidebroken = {};
        opt_uint  offset     = {};
        opt_uint  limit      = {};
    }; // struct CountryParams


    struct Country {
        string   name;
        string   iso_3166_1;
        unsigned stationcount;
    }; // struct Country


    using CountryVec = std::vector<Country>;


    struct ServerStats {
        unsigned supported_version;
        string   software_version;
        string   status;
        unsigned stations;
        unsigned stations_broken;
        unsigned tags;
        unsigned clicks_last_hour;
        unsigned clicks_last_day;
        unsigned languages;
        unsigned countries;
    }; // struct ServerStats




    struct SearchStationParams {
        enum class Order {
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
            random,
        };

        using opt_order = std::optional<Order>;

        opt_string  name              = {};
        opt_bool    nameExact         = {};
        opt_string  country           = {};
        opt_bool    countryExact      = {};
        opt_string  countrycode       = {};
        opt_string  state             = {};
        opt_bool    stateExact        = {};
        opt_string  language          = {};
        opt_bool    languageExact     = {};
        opt_string  tag               = {};
        opt_bool    tagExact          = {};
        opt_strings tagList           = {};
        opt_string  codec             = {};
        opt_uint    bitrateMin        = {};
        opt_uint    bitrateMax        = {};
        opt_bool    has_geo_info      = {};
        opt_bool    has_extended_info = {};
        opt_bool    is_https          = {};
        opt_double  geo_lat           = {};
        opt_double  geo_long          = {};
        opt_double  geo_distance      = {};
        opt_order   order             = {};
        opt_bool    reverse           = {};
        opt_uint    offset            = {};
        opt_uint    limit             = {};
        opt_bool    hidebroken        = {};
    }; // struct SearchStationParams


    struct Station {
        string        changeuuid;
        string        stationuuid;
        string        name;
        string        url;
        string        url_resolved;
        string        homepage;
        string        favicon;
        string        tags;
        string        countrycode;
        opt_string    state;
        opt_string    iso_3166_2;
        string        language;
        string        languagecodes;
        std::uint64_t votes       = 0;
        opt_string    lastchangetime;
        opt_string    lastchangetime_iso8601;
        string        codec;
        unsigned      bitrate     = 0;
        int           hls         = 0; // TODO: maybe bool?
        int           lastcheckok = 0; // TODO: maybe bool?
        opt_string    lastchecktime;
        opt_string    lastchecktime_iso8601;
        opt_string    lastcheckoktime;
        opt_string    lastcheckoktime_iso8601;
        opt_string    lastlocalchecktime;
        opt_string    lastlocalchecktime_iso8601;
        opt_string    clicktimestamp;
        opt_string    clicktimestamp_iso8601;
        std::uint64_t clickcount  = 0;
        int           clicktrend  = 0;
        int           ssl_error   = 0; // TODO: maybe bool?
        opt_double    geo_lat;
        opt_double    geo_long;
        opt_double    geo_distance;
        opt_bool      has_extended_info;
    }; // struct Station


    using StationVec = std::vector<Station>;


    struct TagParams {
        enum class Order {
            name,
            stationcount,
        };

        using opt_order = std::optional<Order>;

        opt_order order      = {};
        opt_bool  reverse    = {};
        opt_bool  hidebroken = {};
        opt_uint  offset     = {};
        opt_uint  limit      = {};
    }; // struct TagParams


    struct Tag {
        string name;
        unsigned stationcount;
    }; // struct Tag


    using TagVec = std::vector<Tag>;


    struct VoteParams {
        string stationuuid;
    }; // struct VoteParams

    struct VoteResult {
        bool   ok;
        string message;
    }; // struct VoteResult


    template<typename... Args>
    using result_function_sig = void (Args...);

    template<typename... Args>
    using result_function_t = std::move_only_function<result_function_sig<Args...>>;

    using error_function_sig = void (const std::exception& error);
    using error_function_t = std::move_only_function<error_function_sig>;


    void
    initialize(const string& user_agent);

    void
    finalize();

    void
    process();


    bool
    is_busy();


    void
    set_server(const string& address);


    string
    get_server();


    void
    get_mirrors(result_function_t<> result_func = {},
                error_function_t error_func = {});


    using MirrorsVec = std::vector<string>;

    MirrorsVec
    current_mirrors();


    void
    connect(result_function_t<> result_func = {},
            error_function_t error_func = {});


    void
    get_codecs(const CodecParams& params,
               result_function_t<CodecVec> result_func,
               error_function_t error_func = {});


    void
    get_countries(const CountryParams& params,
                  result_function_t<CountryVec> result_func,
                  error_function_t error_func = {});


    void
    get_server_stats(result_function_t<ServerStats> result_func,
                     error_function_t error_func = {});


    void
    get_station(const string& uuid,
                result_function_t<Station> result_func,
                error_function_t error_func = {});


    void
    get_tags(const TagParams& params,
             result_function_t<TagVec> result_func,
             error_function_t error_func = {});


    void
    search_stations(const SearchStationParams& params,
                    result_function_t<StationVec> result_func,
                    error_function_t error_func = {});


    void
    send_click(const string& uuid,
               result_function_t<ClickResult> result_func,
               error_function_t error_func = {});


    void
    send_vote(const string& uuid,
              result_function_t<VoteResult> result_func,
              error_function_t error_func = {});

} // namespace RadioBrowserAPI

#endif
