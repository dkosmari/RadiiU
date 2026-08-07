/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <format>

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
        // TODO: use hh_mm_ss from chrono
        unsigned total = t.count();
        unsigned s = total % 60u;
        total /= 60u;
        unsigned m = total % 60u;
        total /= 60;
        unsigned h = total % 24;
        total /= 24;
        unsigned d = total;
        if (d)
            return std::format("{:}d {:02}:{:02}:{:02}", d, h, m, s);
        else
            return std::format("{:02}:{:02}:{:02}", h, m, s);
    }


    string
    value(std::uint64_t x)
    {
        if (x < 1'000u)
            return to_string(x);

        if (x < 10'000u) {
            float fx = x / 1e3f;
            return std::format("{:.1f}k", fx);
        }

        if (x < 1'000'000u) {
            float fx = x / 1e3f;
            return std::format("{:.0f}k", fx);
        }


        if (x < 10'000'000u) {
            float fx = x / 1e6f;
            return std::format("{:.1f}M", fx);
        }

        if (x < 1'000'000'000u) {
            float fx = x / 1e6f;
            return std::format("{:0f}M", fx);
        }


        if (x < 10'000'000'000u) {
            float fx = x / 1e9f;
            return std::format("{:.1f}G", fx);
        }


        if (x < 1'000'000'000'000u) {
            float fx = x / 1e9f;
            return std::format("{:.0f}G", fx);
        }

        float fx = x / 1e12f;
        return std::format("{:.1f}T", fx);
    }

} // namespace humanize
