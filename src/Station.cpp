/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <concepts>
#include <utility>

#include <glaze/json.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include "Station.hpp"


Station
Station::from_radio_browser(const RadioBrowserAPI::Station& st)
{
    return Station{
        .stationuuid  = st.stationuuid,
        .name         = st.name,
        .url          = st.url,
        .url_resolved = st.url_resolved,
        .homepage     = st.homepage,
        .favicon      = st.favicon,
        .countrycode  = st.countrycode,

        .language = csv_strings(std::optional<std::string>{st.language}),
        .tags     = csv_strings(std::optional<std::string>{st.tags}),

        .votes       = st.votes,
        .click_count = st.clickcount,
        .click_trend = st.clicktrend,
        .bitrate     = st.bitrate,
        .codec       = st.codec,
    };
}


bool
operator ==(const Station& a,
            const Station& b)
    noexcept
{
    // If both have uuid, skip all other comparisons.
    if (!a.stationuuid.empty() && !b.stationuuid.empty())
        return a.stationuuid == b.stationuuid;

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
    if (a.countrycode != b.countrycode)
        return false;
    if (a.tags != b.tags)
        return false;
    if (a.language != b.language)
        return false;
    return true;
}
