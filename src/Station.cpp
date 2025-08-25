/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <concepts>

#include "Station.hpp"


namespace {

    struct LoadHelper  {
        const json::object& obj;

        LoadHelper(const json::object& obj) :
            obj(obj)
        {}

        void
        load(const char* key,
             std::string& dst)
            const
        {
            if (obj.contains(key))
                dst = obj.at(key).as<json::string>();
        }

        template<std::integral I>
        void
        load(const char* key,
             I& dst)
            const
        {
            if (obj.contains(key))
                dst = static_cast<I>(obj.at(key).as<json::integer>());
        }

    };

} // namespace

Station
Station::from_json(const json::object& obj)
{
    Station result;
    LoadHelper h{obj};
    h.load("name",         result.name);
    h.load("url",          result.url);
    h.load("url_resolved", result.url_resolved);
    h.load("homepage",     result.homepage);
    h.load("favicon",      result.favicon);
    h.load("tags",         result.tags);
    h.load("countrycode",  result.country_code);
    h.load("language",     result.language);
    h.load("stationuuid",  result.uuid);
    h.load("votes",        result.votes);
    h.load("bitrate",      result.bitrate);
    return result;
}


namespace {

    struct StoreHelper {

        json::object& obj;

        StoreHelper(json::object& obj) :
            obj(obj)
        {}

        void
        store(const char* key,
              const std::string& val)
            const
        {
            if (!val.empty())
                obj[key] = val;
        }

        template<std::integral I>
        void
        store(const char* key,
              I val)
            const
        {
            if (val)
                obj[key] = static_cast<json::integer>(val);
        }

    };

} // namespace


json::object
Station::to_json()
    const
{
    json::object obj;
    StoreHelper h{obj};
    h.store("name", name);
    h.store("url", url);
    h.store("url_resolved", url_resolved);
    h.store("homepage", homepage);
    h.store("favicon", favicon);
    h.store("tags", tags);
    h.store("countrycode", country_code);
    h.store("language", language);
    h.store("stationuuid", uuid);
    h.store("votes", votes);
    h.store("bitrate", bitrate);
    return obj;
}
