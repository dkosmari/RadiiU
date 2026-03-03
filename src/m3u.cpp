/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <istream>
#include <utility>

#include "m3u.hpp"

#include "string_utils.hpp"


namespace m3u {

    const std::string ext_m3u = "#EXTM3U";
    const std::string ext_inf = "#EXTINF:";

    std::ostream&
    operator <<(std::ostream& out,
                const track& t)
    {
        if (t.duration || t.title) {
            out << ext_inf;
            if (t.duration)
                out << t.duration->count();
            else
                out << "-1";
            out << ',';
            if (t.title)
                out << *t.title;
            out << '\n';
        }
        return out << t.url;
    }


    std::ostream&
    operator <<(std::ostream& out,
                const playlist& pl)
    {
        bool is_ext = false;
        for (auto& t : pl)
            if (t.duration || t.title) {
                is_ext = true;
                break;
            }

        if (is_ext)
            out << ext_m3u << '\n';

        for (auto& t : pl) {
            out << t << '\n';
        }
        return out;
    }


    namespace {

        void
        parse_extended(std::istream& input,
                       playlist& result)
        {
            std::string line;
            track current_track;

            while (getline(input, line)) {
                if (line.empty())
                    continue;
                if (line[0] == '#') {
                    if (line.starts_with(ext_inf)) {
                        try {
                            auto tokens = string_utils::split(line.substr(ext_inf.size()),
                                                              ",", false, 2);
                            if (tokens.size() != 2)
                                continue;
                            long t = std::stol(tokens[0]);
                            if (t != 0 && t != -1)
                                current_track.duration = std::chrono::seconds(t);
                            current_track.title = std::move(tokens[1]);
                        }
                        catch (...) {
                            // TODO: report problem parsing M3U extended directive
                            continue;
                        }
                    } else
                        continue;

                } else {
                    current_track.url = line;
                    result.push_back(std::move(current_track));
                    current_track = {};
                }
            }
        }


        void
        parse_normal(std::istream& input,
                     playlist& result)
        {
            std::string line;
            while (getline(input, line)) {
                if (line.empty())
                    continue;
                result.emplace_back(line, std::nullopt, std::nullopt);
            }
        }

    } // namespace


    playlist
    parse(std::istream& input)
    {
        playlist result;

        std::string line;
        // check if first line is #EXTM3U
        if (!getline(input, line))
            return result;

        if (line == ext_m3u) {
            parse_extended(input, result);
        } else if (!line.empty() && line[0] != '#') {
            result.emplace_back(std::move(line), std::nullopt, std::nullopt);
            parse_normal(input, result);
        }

        return result;
    }


    playlist
    parse(const std::string& input)
    {
        std::istringstream stream{input};
        return parse(stream);
    }

} // namespace m3u


#ifdef UNIT_TEST

// g++ -std=c++23 -DUNIT_TEST m3u.cpp string_utils.cpp

#include <iostream>
#include <sstream>

using std::cout;
using std::endl;
using std::istringstream;
using std::stringstream;
using std::string;

using namespace std::literals;


#define CHECK_EQUAL(x, y)                       \
    do {                                        \
        const auto x_val = x;                   \
        const auto y_val = y;                   \
        ++total;                                \
        if (x_val == y_val) {                   \
            ++successes;                        \
            cout << "    PASSED" << endl;       \
        } else {                                \
            cout << "    FAILED: "              \
                 << #x << "(" << x_val << ")"  \
                 << " != "                      \
                 << #y << "(" << y_val << ")"  \
                 << endl;                       \
        }                                       \
    } while (false)

void
dump(const m3u::playlist& pl)
{
    cout << "---- PL BEGIN ----\n"
         << pl
         << "---- PL END ----"
         << endl;
}

int main()
{
    using namespace m3u;

    int total = 0;
    int successes = 0;

    {
        cout << "Test: simple empty" << endl;
        istringstream input{"\n"};
        auto pl = parse(input);
        CHECK_EQUAL(pl.size(), 0);
        dump(pl);
    }

    {
        cout << "Test: simple with one track" << endl;
        auto track_url = "http://www.example.com/stream"s;
        stringstream input;
        input << track_url << endl;
        auto pl = parse(input);
        CHECK_EQUAL(pl.size(), 1);
        CHECK_EQUAL(pl.at(0).url, track_url);
        dump(pl);
    }

    {
        cout << "Test: extended empty" << endl;
        stringstream input;
        input << "#EXTM3U\n";
        auto pl = parse(input);
        CHECK_EQUAL(pl.size(), 0);
        dump(pl);
    }

    {
        cout << "Test: extended with one track" << endl;
        auto track_url = "http://www.example.com/stream"s;
        auto track_duration = 69s;
        auto track_title = "abc, def"s;
        stringstream input;
        input << "#EXTM3U\n\n"
              << "#EXTINF:" << track_duration.count() << "," << track_title << "\n"
              << track_url << "\n";
        auto pl = parse(input);
        CHECK_EQUAL(pl.size(), 1);
        auto& t = pl.at(0);
        CHECK_EQUAL(t.duration.has_value(), true);
        CHECK_EQUAL(*t.duration, track_duration);
        CHECK_EQUAL(t.title.has_value(), true);
        CHECK_EQUAL(*t.title, track_title);
        dump(pl);
    }

    cout << "Successes: " << successes << " / " << total << endl;
}

#endif // UNIT_TEST
