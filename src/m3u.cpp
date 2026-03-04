/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <ostream>
#include <ranges>
#include <utility>              // move()

#include "m3u.hpp"

#include "string_utils.hpp"


using namespace std::literals;


namespace m3u {

    const std::string ext_m3u = "#EXTM3U";
    const std::string ext_inf = "#EXTINF:";

    void
    write(std::ostream& out,
          const track& trk,
          bool is_ext)
    {
        if (is_ext) {
            out << ext_inf;
            if (trk.duration)
                out << trk.duration->count();
            else
                out << "-1";
            out << ',';
            if (trk.title)
                out << *trk.title;
            out << '\n';
        }
        out << trk.url << '\n';
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

        for (auto& trk : pl)
            write(out, trk, is_ext);

        return out;
    }


    playlist
    parse(const std::string& input)
    {
        playlist result;

        auto lines = string_utils::split(input, {"\r"s, "\n"s}, true);
        if (lines.empty())
            return result;

        // check if first line is #EXTM3U
        if (lines.front() == ext_m3u) {
            // handle extended playlist
            std::optional<std::chrono::seconds> duration;
            std::optional<std::string> title;
            for (auto& line : lines | std::views::drop(1)) {
                if (line.empty()) {
                    duration.reset();
                    title.reset();
                    continue;
                }
                if (line[0] == '#') {
                    // maybe ext directive, maybe comment
                    if (line.starts_with(ext_inf)) {
                        // #EXTINF:N,TITLE
                        auto tokens = string_utils::split(line.substr(ext_inf.size()),
                                                          ",", false, 2);
                        if (tokens.size() != 2) {
                            duration.reset();
                            title.reset();
                            continue;
                        }
                        long d = std::stol(tokens[0]);
                        if (d != 0 && d != -1)
                            duration = std::chrono::seconds(d);

                        if (!tokens[1].empty())
                            title = std::move(tokens[1]);
                    } else {
                        // non-standard directive, or comment, just skip it
                        duration.reset();
                        title.reset();
                        continue;
                    }
                } else {
                    result.emplace_back(std::move(line), duration, title);
                    duration.reset();
                    title.reset();
                }
            } // for (line : lines)
        } else {
            // handle simple playlist, no comments allowed
            for (auto& line : lines) {
                if (line.empty())
                    continue;
                result.emplace_back(std::move(line), std::nullopt, std::nullopt);
            }
        }

        return result;
    }

} // namespace m3u


#ifdef UNIT_TEST

// compilation: g++ -std=c++23 -DUNIT_TEST m3u.cpp

#include "string_utils.cpp"

#include "unit_test.hpp"

void
dump(const m3u::playlist& pl)
{
    cout << "<m3u>\n"
         << pl
         << "</m3u>"
         << endl;
}

int main()
{
    int total = 0;
    int successes = 0;

    const auto crlf = "\r\n"s;

    {
        cout << "Test: simple empty" << endl;
        auto input = crlf;
        auto pl = m3u::parse(input);
        CHECK_EQUAL(pl.size(), 0);
        dump(pl);
    }

    {
        cout << "Test: simple with one track" << endl;
        auto track_url = "http://www.example.com/stream"s;
        auto input = track_url;
        auto pl = m3u::parse(input);
        CHECK_EQUAL(pl.size(), 1);
        CHECK_EQUAL(pl.at(0).url, track_url);
        dump(pl);
    }

    {
        cout << "Test: extended empty" << endl;
        auto input = "#EXTM3U"s + crlf;
        auto pl = m3u::parse(input);
        CHECK_EQUAL(pl.size(), 0);
        dump(pl);
    }

    {
        cout << "Test: extended with one track" << endl;
        auto track_url = "http://www.example.com/stream"s;
        auto track_duration = 69s;
        auto track_title = "abc, def"s;
        auto input =
            "#EXTM3U"s + crlf
            + "#EXTINF:"s + std::to_string(track_duration.count()) + ","s + track_title + crlf
            + track_url + crlf;
        auto pl = m3u::parse(input);
        CHECK_EQUAL(pl.size(), 1);
        auto& t = pl.at(0);
        CHECK_EQUAL(t.duration.has_value(), true);
        CHECK_EQUAL(*t.duration, track_duration);
        CHECK_EQUAL(t.title.has_value(), true);
        CHECK_EQUAL(*t.title, track_title);
        dump(pl);
    }

    {
        cout << "Test: 101 Smooth Jazz" << endl;
        auto input = "#EXTM3U"s + crlf
            + "#EXTINF:0, 101 Smooth Jazz"s + crlf
            + "http://jking.cdnstream1.com/b22139_128mp3"s;
        auto pl = m3u::parse(input);
        CHECK_EQUAL(pl.size(), 1);
        auto& t = pl.at(0);
        CHECK_EQUAL(t.url, "http://jking.cdnstream1.com/b22139_128mp3"s);
        CHECK_EQUAL(t.title.has_value(), true);
        CHECK_EQUAL(*t.title, " 101 Smooth Jazz"s);
        CHECK_EQUAL(t.duration.has_value(), false);
        dump(pl);
    }

    cout << "Successes: " << successes << " / " << total << endl;
}

#endif // UNIT_TEST
