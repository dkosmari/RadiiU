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


bool
operator ==(const Station& a,
            const Station& b)
    noexcept
{
    // If both have uuid, skip all other comparisons.
    if (!a.uuid.empty() && !b.uuid.empty())
        return a.uuid == b.uuid;

    if (a.name != b.name)
        return false;
    if (a.url != b.url)
        return false;
    if (a.url_resolved != b.url_resolved)
        return false;
    if (a.homepage != b.homepage)
        return false;
    if (a.favicon != b.favicon)
        return false;
    if (a.tags != b.tags)
        return false;
    if (a.country_code != b.country_code)
        return false;
    if (a.language != b.language)
        return false;
    return true;
}


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
    h.load("clickcount",   result.click_count);
    h.load("clicktrend",   result.click_trend);
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
    // Note: these fields are volatile, no point in serializing them.
    // h.store("votes", votes);
    // h.store("clickcount", click_count);
    // h.store("clicktrend", click_trend);
    // h.store("bitrate", bitrate);
    return obj;
}
