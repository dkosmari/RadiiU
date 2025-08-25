/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <regex>
#include <stdexcept>

#include "icy_meta.hpp"


using std::regex;
using std::smatch;
using std::runtime_error;
using std::string;


std::map<string, string>
icy_meta::parse(string input)
{
    std::map<string, string> result;

    regex field_start_re{R"(([[:alpha:]]+)=(['"]))"};

    while (!input.empty()) {

        // cout << "Input is [" << input << "]" << endl;

        smatch m;
        if (!regex_search(input, m, field_start_re,
                          std::regex_constants::match_continuous))
            throw runtime_error{"could not find field start: " + input};

        string key = m.str(1);
        string quote = m.str(2);

        // cout << "key: [" << key << "]" << endl;
        // cout << "quote: [" << quote << "]" << endl;

        input.erase(0, m.length());

        /* Ending quote is either:
         *   - the end of the input
         *   - right before a ';' that either
         *     - ends the input
         *     - starts the next field (test using assertion, to avoid consuming input)
         */
        regex field_end_re{"(" + quote + "$|" + quote + ";($|(?=[[:alpha:]+=])))"};

        if (!regex_search(input, m, field_end_re))
            throw runtime_error{"could not find end of field in " + input};

        string value = m.prefix();

        // cout << "value: [" << value << "]" << endl;
        // cout << "end pos: " << m.position() << endl;
        // cout << "end len: " << m.length() << endl;

        result[key] = value;

        input.erase(0, m.position() + m.length());
        // cout << "input is now: [" << input << "]" << endl;
    }

    return result;
}



#ifdef UNIT_TEST

#include <iostream>

using std::cout;
using std::endl;


template<typename A,
         typename B>
void
check(A&& a, B&& b)
{
    if (a != b)
        throw runtime_error{"Check failed: \"" + a + "\" != \"" + b + "\""};
}


int main()
{
    {
        // trivial case
        auto d = icy_meta::parse("StreamTitle='Testing something'");
        check(d.at("StreamTitle"), "Testing something");
    }

    {
        // again, with trailing ';'
        auto d = icy_meta::parse("StreamTitle='Testing something';");
        check(d.at("StreamTitle"), "Testing something");
    }

    {
        // trivial case with two fields
        auto d = icy_meta::parse("StreamTitle='Another test';StreamURL='http://example.com'");
        check(d.at("StreamTitle"), "Another test");
        check(d.at("StreamURL"), "http://example.com");
    }

    {
        // again, with trailing ';'
        auto d = icy_meta::parse("StreamTitle='Another test';StreamURL='http://example.com';");
        check(d.at("StreamTitle"), "Another test");
        check(d.at("StreamURL"), "http://example.com");
    }

    {
        // obnoxious case: title with quote character
        auto d = icy_meta::parse("StreamTitle='Icecast's problem'");
        check(d.at("StreamTitle"), "Icecast's problem");
    }

    {
        // again, with trailing ';'
        auto d = icy_meta::parse("StreamTitle='Icecast's problem';");
        check(d.at("StreamTitle"), "Icecast's problem");
    }


    {
        // fairly ambiguous title
        auto d = icy_meta::parse("StreamTitle='Why's=no quote escaping?'");
        check(d.at("StreamTitle"), "Why's=no quote escaping?");
    }

    {
        // again, trailing ';'
        auto d = icy_meta::parse("StreamTitle='Why's=no quote escaping?';");
        check(d.at("StreamTitle"), "Why's=no quote escaping?");
    }

}

#endif
