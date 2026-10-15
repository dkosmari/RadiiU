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
    duration(seconds t,
             bool separate)
    {
        const std::string separator = separate ? " "s : ""s;

        if (t <= 90s)
            return to_string(t.count()) + separator + "s"s;

        if (t < 60min) {
            auto t_min = duration_cast<minutes>(t);
            auto t_sec = duration_cast<seconds>(t - t_min);
            string result = to_string(t_min.count()) + separator + "min"s;
            if (t_sec >= 1s)
                result += ", "s + to_string(t_sec.count()) + separator + "s"s;
            return result;
        }

        if (t < 24h) {
            auto t_hrs = duration_cast<hours>(t);
            auto t_min = duration_cast<minutes>(t - t_hrs);
            string result = to_string(t_hrs.count()) + separator + "hr"s;
            if (t_min >= 1min)
                result += ", "s + to_string(t_min.count()) + separator + "min"s;
            return result;
        }

        auto t_days = duration_cast<days>(t);
        auto t_hrs = duration_cast<hours>(t - t_days);
        auto t_min = duration_cast<minutes>(t - t_days - t_hrs);
        string result = to_string(t_days.count());
        if (t_days >= 48h)
            result += separator + "days"s;
        else
            result += separator + "day"s;
        if (t_hrs >= 1h)
            result += ", "s + to_string(t_hrs.count()) + separator + "hr"s;
        if (t_min >= 1min)
            result += ", "s + to_string(t_min.count()) + separator + "min"s;
        return result;
    }


    std::string
    duration_brief(std::chrono::seconds t)
    {
        auto days = std::chrono::floor<std::chrono::days>(t);
        std::chrono::hh_mm_ss time_of_day{t - days};
        if (days > 24h)
            return std::format("{:%Q%q} {:%T}", days, time_of_day);
        else
            return std::format("{:%T}", time_of_day);
    }


    string
    value(std::uint64_t x,
          bool separate)
    {
        const std::string separator = separate ? " "s : ""s;
        if (x < 1'000u)
            return to_string(x);

        if (x < 10'000u) {
            float fx = x / 1e3f;
            return std::format("{:.1f}{}k", fx, separator);
        }

        if (x < 1'000'000u) {
            float fx = x / 1e3f;
            return std::format("{:.0f}{}k", fx, separator);
        }


        if (x < 10'000'000u) {
            float fx = x / 1e6f;
            return std::format("{:.1f}{}M", fx, separator);
        }

        if (x < 1'000'000'000u) {
            float fx = x / 1e6f;
            return std::format("{:0f}{}M", fx, separator);
        }


        if (x < 10'000'000'000u) {
            float fx = x / 1e9f;
            return std::format("{:.1f}{}G", fx, separator);
        }


        if (x < 1'000'000'000'000u) {
            float fx = x / 1e9f;
            return std::format("{:.0f}{}G", fx, separator);
        }

        float fx = x / 1e12f;
        return std::format("{:.1f}{}T", fx, separator);
    }

} // namespace humanize
