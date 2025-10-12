/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdio>

#include "humanize.hpp"


using std::chrono::days;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::minutes;
using std::chrono::seconds;
using std::string;
using std::to_string;

using namespace std::literals;


namespace humanize {

    string
    duration(seconds t)
    {
        if (t <= 90s)
            return std::to_string(t.count()) + " s";

        if (t < 60min) {
            auto t_min = duration_cast<minutes>(t);
            auto t_sec = duration_cast<seconds>(t - t_min);
            string result = to_string(t_min.count()) + " min";
            if (t_sec >= 1s)
                result += ", " + to_string(t_sec.count()) + " s";
            return result;
        }

        if (t < 24h) {
            auto t_hrs = duration_cast<hours>(t);
            auto t_min = duration_cast<minutes>(t - t_hrs);
            string result = to_string(t_hrs.count()) + " hr";
            if (t_min >= 1min)
                result += ", " + to_string(t_min.count()) + " min";
            return result;
        }

        auto t_days = duration_cast<days>(t);
        auto t_hrs = duration_cast<hours>(t - t_days);
        auto t_min = duration_cast<minutes>(t - t_days - t_hrs);
        string result = to_string(t_days.count());
        if (t_days >= 48h)
            result += " days";
        else
            result += " day";
        if (t_hrs >= 1h)
            result += ", " + to_string(t_hrs.count()) + " hr";
        if (t_min >= 1min)
            result += ", " + to_string(t_min.count()) + " min";
        return result;
    }


    std::string
    duration_brief(std::chrono::seconds t)
    {
        unsigned total = t.count();
        unsigned s = total % 60u;
        total /= 60u;
        unsigned m = total % 60u;
        total /= 60;
        unsigned h = total % 24;
        total /= 24;
        unsigned d = total;
        char buf[64];
        if (d)
            std::snprintf(buf, sizeof buf, "%ud %02u:%02u:%02u", d, h, m, s);
        else
            std::snprintf(buf, sizeof buf, "%02u:%02u:%02u", h, m, s);
        return buf;
    }


    string
    value(std::uint64_t x)
    {
        if (x < 1'000u)
            return to_string(x);

        char buf[32];

        if (x < 10'000u) {
            float fx = x / 1e3f;
            std::snprintf(buf, sizeof buf, "%.1fk", fx);
            return buf;
        }

        if (x < 1'000'000u) {
            float fx = x / 1e3f;
            std::snprintf(buf, sizeof buf, "%.0fk", fx);
            return buf;
        }


        if (x < 10'000'000u) {
            float fx = x / 1e6f;
            std::snprintf(buf, sizeof buf, "%.1fM", fx);
            return buf;
        }

        if (x < 1'000'000'000u) {
            float fx = x / 1e6f;
            std::snprintf(buf, sizeof buf, "%.0fM", fx);
            return buf;
        }


        if (x < 10'000'000'000u) {
            float fx = x / 1e9f;
            std::snprintf(buf, sizeof buf, "%.1fG", fx);
            return buf;
        }


        if (x < 1'000'000'000'000u) {
            float fx = x / 1e9f;
            std::snprintf(buf, sizeof buf, "%.0fG", fx);
            return buf;
        }

        float fx = x / 1e12f;
        std::snprintf(buf, sizeof buf, "%.1fT", fx);
        return buf;
    }

} // namespace humanize
