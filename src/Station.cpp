/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <concepts>
#include <utility>

#include "Station.hpp"

#include "utils.hpp"


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
    if (a.country_code != b.country_code)
        return false;
    if (a.tags != b.tags)
        return false;
    if (a.languages != b.languages)
        return false;
    return true;
}


Station
Station::from_json(const json::object& obj)
{
    Station result;

    try_get(obj, "name",         result.name);
    try_get(obj, "url",          result.url);
    try_get(obj, "url_resolved", result.url_resolved);
    try_get(obj, "homepage",     result.homepage);
    try_get(obj, "favicon",      result.favicon);
    try_get(obj, "countrycode",  result.country_code);
    try_get(obj, "stationuuid",  result.uuid);
    try_get(obj, "votes",        result.votes);
    try_get(obj, "clickcount",   result.click_count);
    try_get(obj, "clicktrend",   result.click_trend);
    try_get(obj, "bitrate",      result.bitrate);
    try_get(obj, "codec",        result.codec);

    // Note: these fields we split into vectors.

    if (auto language = try_get<json::string>(obj, "language"))
        result.languages = utils::split(*language, ",");

    if (auto tags = try_get<json::string>(obj, "tags"))
        result.tags = utils::split(*tags, ",");

    return result;
}


json::object
Station::to_json()
    const
{
    json::object obj;

    obj["name"]         = name;
    obj["url"]          = url;
    obj["url_resolved"] = url_resolved;
    obj["homepage"]     = homepage;
    obj["favicon"]      = favicon;
    obj["countrycode"]  = country_code;
    obj["stationuuid"]  = uuid;

    // Note: these fields are volatile, no point in serializing them.
    // - votes
    // - click_count
    // - click_trend
    // - bitrate
    // - codec

    obj["language"] = utils::join(languages, ",");
    obj["tags"] = utils::join(tags, ",");

    return obj;
}


StationEx::StationEx()
        noexcept
{}


StationEx::StationEx(const Station& st) :
    Station{st},
    languages_str{utils::join(languages, ", ")},
    tags_str{utils::join(tags, ", ")}
{}


Station
StationEx::as_station()
    const
{
    // Note: slicing is okay here.
    Station result = *this;

    // copy languages_str into the languages vector
    result.languages = utils::split(languages_str, ",");
    for (auto& lang : result.languages)
        lang = utils::trimmed(lang, ' ');
    std::erase(result.languages, "");

    // copy tags_str into the tags vector
    result.tags = utils::split(tags_str, ",");
    for (auto& tag : result.tags)
        tag = utils::trimmed(tag, ' ');
    std::erase(result.tags, "");

    return result;
}
