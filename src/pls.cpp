/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdio>
#include <ostream>
#include <ranges>
#include <utility>              // move()

#include "pls.hpp"

#include "string_utils.hpp"


using namespace std::literals;


namespace pls {

    const std::string header = "[playlist]";


    error::error(const std::string& msg) :
        std::runtime_error{"PLS error: " + msg}
    {}


    void
    write(std::ostream& out,
          const track& trk,
          std::size_t num)
    {
        out << "File" << num << "=" << trk.url << '\n';
        if (trk.length)
            out << "Length" << num << "=" << trk.length->count() << '\n';
        if (trk.title)
            out << "Title" << num << "=" << *trk.title << '\n';
    }


    std::ostream&
    operator <<(std::ostream& out,
                const playlist& pl)
    {
        out << header << "\n\n";

        for (std::size_t i = 0; i < pl.size(); ++i) {
            write(out, pl[i], i + 1);
            out << '\n';
        }

        out << "NumberOfEntries=" << pl.size() << '\n';
        out << "Version=2\n";

        return out;
    }


    /*
     * Note: a PLS file can have all its keys out of order.
     */
    playlist
    parse(const std::string& input)
    {
        using string_utils::equal_case;
        using string_utils::split;

        playlist result;

        auto lines = split(input, {"\r"s, "\n"s}, true);
        if (lines.empty())
            return result;

        if (lines.front() != header)
            throw error{"wrong header: \""s + lines.front() + "\""};

        std::optional<std::size_t> num_of_entries;

        for (auto& line : lines | std::views::drop(1)) {

            auto tokens = split(line, "=", false, 2);
            if (tokens.size() != 2)
                throw error{"can't parse line: \""s + line + "\""s};

            if (equal_case(tokens[0], "NumberOfEntries")) {
                try {
                    num_of_entries = std::stoul(tokens[1]);
                }
                catch (std::exception&) {
                    throw error{"failed to parse number of entries: \""s + line + "\""s};
                }
                continue;
            }
            if (equal_case(tokens[0], "Version")) {
                if (tokens[1] != "2")
                    throw error{"unsupported PLS version"};
                continue;
            }

            // otherwise assume it's a track entry
            char key_name[64] = "";
            std::size_t key_num = 0;
            int r = std::sscanf(tokens[0].data(), "%63[A-Za-z]%zu", key_name, &key_num);
            if (r != 2)
                throw error{"can't parse key: \""s + tokens[0] + "\""s};

            if (key_num == 0)
                throw error{"track number cannot be zero"};
            if (result.size() < key_num)
                result.resize(key_num);
            track& trk = result[key_num - 1];

            if (equal_case(key_name, "File")) {
                trk.url = tokens[1];
            } else if (equal_case(key_name, "Length")) {
                long val = std::stol(tokens[1]);
                if (val != -1 && val != 0)
                    trk.length = std::chrono::seconds(val);
            } else if (equal_case(key_name, "Title")) {
                if (!tokens[1].empty())
                    trk.title = tokens[1];
            } else
                throw error{"invalid key: \""s + key_name + "\""s};
        } // for each line

        // sanity check, ensure the playlist wasn't truncated
        if (num_of_entries && *num_of_entries != result.size())
            throw error{"incomplete playlist: NumberOfEntries says "
                        + std::to_string(*num_of_entries) + " but found "
                        + std::to_string(result.size()) + " tracks"};

        return result;
    }

} // namespace pls


#ifdef UNIT_TEST

// compilation: g++ -std=c++23 -DUNIT_TEST pls.cpp

#include "string_utils.cpp"

#include "unit_test.hpp"

void
dump(const pls::playlist& pl)
{
    cout << "<pls>\n"
         << pl
         << "</pls>"
         << endl;
}

int main()
{
    int total = 0;
    int successes = 0;

    {
        cout << "Test: empty" << endl;
        auto input = "";
        auto pl = pls::parse(input);
        CHECK_EQUAL(pl.empty(), true);
        dump(pl);
    }

    {
        cout << "Test: empty header" << endl;
        auto input =
            "[playlist]\n"
            "\n"
            "NumberOfEntries=0\n";
        auto pl = pls::parse(input);
        CHECK_EQUAL(pl.empty(), true);
        dump(pl);
    }

    {
        cout << "Test: empty header but wrong version" << endl;
        auto input =
            "[playlist]\n"
            "\n"
            "NumberOfEntries=0\n"
            "Version=1";
        CHECK_EXCEPT(auto pl = pls::parse(input), pls::error);
    }

    {
        cout << "Test: wrong empty" << endl;
        auto input =
            "[playlist]\n"
            "NumberOfEntries=1\n";
        CHECK_EXCEPT(auto pl = pls::parse(input), pls::error);
    }

    {
        cout << "Test: 1 track" << endl;
        auto url = "http://www.example.com/track.mp3"s;
        auto input =
            "[playlist]\r\n"s +
            "File1="s + url + "\r\n"s +
            "NumberOfEntries=1\r\n"s;
        auto pl = pls::parse(input);
        CHECK_EQUAL(pl.size(), 1);
        auto& trk = pl.at(0);
        CHECK_EQUAL(trk.url, url);
        CHECK_EQUAL(trk.length.has_value(), false);
        CHECK_EQUAL(trk.title.has_value(), false);
        dump(pl);
    }

    {
        cout << "Test: 1 track no numberofentries" << endl;
        auto url = "http://www.example.com/track.mp3"s;
        auto input =
            "[playlist]\n"s +
            "File1="s + url + "\n"s;
        auto pl = pls::parse(input);
        CHECK_EQUAL(pl.size(), 1);
        auto& trk = pl.at(0);
        CHECK_EQUAL(trk.url, url);
        CHECK_EQUAL(trk.title.has_value(), false);
        CHECK_EQUAL(trk.length.has_value(), false);
        dump(pl);
    }

    {
        cout << "Test: 1 track with title and length" << endl;
        auto url = "http://www.example.com/track.mp3"s;
        auto title = "Some Title"s;
        auto length = 45s;
        auto input =
            "[playlist]\n"s +
            "File1="s + url + "\n"s +
            "Title1="s + title + "\n"s +
            "Length1="s + std::to_string(length.count()) + "\n"s +
            "NumberOfEntries=1\n"s;
        auto pl = pls::parse(input);
        CHECK_EQUAL(pl.size(), 1);
        auto& trk = pl.at(0);
        CHECK_EQUAL(trk.url, url);
        CHECK_EQUAL(trk.title.has_value(), true);
        CHECK_EQUAL(*trk.title, title);
        CHECK_EQUAL(trk.length.has_value(), true);
        CHECK_EQUAL(*trk.length, length);
        dump(pl);
    }

    {
        cout << "Test: 2 tracks with title and length" << endl;

        auto url1 = "http://www.example.com/track1.mp3"s;
        auto title1 = "Some Title"s;
        auto length1 = 45s;

        auto url2 = "http://www.example.com/track2.mp3"s;
        auto title2 = "Another Title"s;
        auto length2 = 30s;

        auto input =
            "[playlist]\n"s +
            "File1="s + url1 + "\n"s +
            "Title1="s + title1 + "\n"s +
            "Title2="s + title2 + "\n"s +
            "Length1="s + std::to_string(length1.count()) + "\n"s +
            "Length2="s + std::to_string(length2.count()) + "\n"s +
            "File2="s + url2 + "\n"s +
            "NumberOfEntries=2\n"s;
        auto pl = pls::parse(input);
        CHECK_EQUAL(pl.size(), 2);
        auto& trk1 = pl.at(0);
        CHECK_EQUAL(trk1.url, url1);
        CHECK_EQUAL(trk1.title.has_value(), true);
        CHECK_EQUAL(*trk1.title, title1);
        CHECK_EQUAL(trk1.length.has_value(), true);
        CHECK_EQUAL(*trk1.length, length1);
        auto& trk2 = pl.at(1);
        CHECK_EQUAL(trk2.url, url2);
        CHECK_EQUAL(trk2.title.has_value(), true);
        CHECK_EQUAL(*trk2.title, title2);
        CHECK_EQUAL(trk2.length.has_value(), true);
        CHECK_EQUAL(*trk2.length, length2);
        dump(pl);
    }

    cout << "Successes: " << successes << " / " << total << endl;
}

#endif
