/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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


    struct Error : std::runtime_error {

        Error(const string& msg);

    }; // struct error


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


    struct StationParams {
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

        opt_order order = {};
        opt_bool reverse = {};
        opt_uint offset = {};
        opt_uint limit = {};
        opt_bool hidebroken = {};
    }; // struct StationParams


    struct StationUUIDParams {
        string uuids;
    }; // struct StationUUIDParams


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


    struct VoteResult {
        bool   ok;
        string message;
    }; // struct VoteResult


    template<typename... Args>
    using ResultCallbackSignature = void (Args...);

    template<typename... Args>
    using ResultFunction = std::move_only_function<ResultCallbackSignature<Args...>>;

    using ExceptionCallbackSignature = void (const std::exception&);
    using ExceptionFunction = std::move_only_function<ExceptionCallbackSignature>;

    using ErrorMsgCallbackSignature = void (const string&);
    using ErrorMsgFunction = std::move_only_function<ErrorMsgCallbackSignature>;


    void
    initialize(const string& user_agent,
               const string& server);

    void
    finalize();

    void
    process();


    bool
    is_busy();


    void
    set_server(const string& server);


    string
    get_server();


    using MirrorsVec = std::vector<string>;

    using FetchMirrorsResultFunction = ResultFunction<MirrorsVec>;
    using FetchMirrorsErrorCallbackSignature = void(const string&);

    void
    fetch_mirrors(FetchMirrorsResultFunction result_func = {},
                  ErrorMsgFunction error_func = {});


    void
    update_mirrors();


    void
    update_mirrors_and_select_random();


    using ForEachMirrorSignature = void (const string& server);
    using ForEachMirrorFunction = std::function<ForEachMirrorSignature>;

    void
    for_each_mirror(ForEachMirrorFunction func);


    using GetCodecsResultFunction = ResultFunction<CodecVec>;

    void
    get_codecs(const CodecParams& params,
               GetCodecsResultFunction result_func,
               ExceptionFunction except_func = {})
        noexcept;


    using GetCountriesResultFunction = ResultFunction<CountryVec>;

    void
    get_countries(const CountryParams& params,
                  GetCountriesResultFunction result_func,
                  ExceptionFunction except_func = {})
        noexcept;


    using GetServerStatsResultFunction = ResultFunction<ServerStats>;

    void
    get_server_stats(GetServerStatsResultFunction result_func,
                     ExceptionFunction except_func = {})
        noexcept;


    using GetStationResultFunction = ResultFunction<Station>;

    void
    get_station(const string& uuid,
                GetStationResultFunction result_func,
                ExceptionFunction except_func = {})
        noexcept;


    using GetTagsResultFunction = ResultFunction<TagVec>;

    void
    get_tags(const TagParams& params,
             GetTagsResultFunction result_func,
             ExceptionFunction except_func = {})
        noexcept;


    using SearchStationsResultFunction = ResultFunction<StationVec>;

    void
    search_stations(const SearchStationParams& params,
                    SearchStationsResultFunction result_func,
                    ExceptionFunction except_func = {})
        noexcept;


    using SendClickResultFunction = ResultFunction<ClickResult>;

    void
    send_click(const string& uuid,
               SendClickResultFunction result_func,
               ExceptionFunction except_func = {})
        noexcept;


    using SendVoteResultFunction = ResultFunction<VoteResult>;

    void
    send_vote(const string& uuid,
              SendVoteResultFunction result_func,
              ExceptionFunction except_func = {})
        noexcept;

} // namespace RadioBrowserAPI

#endif
