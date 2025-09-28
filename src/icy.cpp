/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdexcept>

#include "icy.hpp"


using std::runtime_error;
using std::string;

using namespace std::literals;


namespace icy {

    dict_t
    parse(std::string_view input)
    {
        dict_t result;

        const auto npos = std::string_view::npos;
        using size_type = std::string_view::size_type;

        while (!input.empty()) {

            // Look for KeyName=
            size_type key_end_pos = input.find('=');
            if (key_end_pos == npos)
                break;

            std::string_view key = input.substr(0, key_end_pos);
            if (key.empty())
                // Input error: empty key
                break;

            // Discard all the way up to the '='
            input.remove_prefix(key_end_pos + 1);
            if (input.empty())
                // Input error: empty value
                break;

            char quote_char = input.front();
            if (quote_char != '\'' && quote_char != '"')
                break;

            input.remove_prefix(1);
            if (input.empty())
                // Input error: truncated inside quotes
                break;

            size_type remove_size = 2;
            size_type close_quote_pos = input.find(quote_char + ";"s);
            if (close_quote_pos == npos) {
                // When on the end of input, the last quote doesn't have a following ';'
                close_quote_pos = input.size() - 1;
                remove_size = 1;
                if (input[close_quote_pos] != quote_char)
                    // Input error: input does not end in the closing quote char
                    break;
            }

            if (close_quote_pos == npos)
                // Input error: unterminated quote
                break;

            std::string_view value = input.substr(0, close_quote_pos);

            result.emplace(key, value);

            input.remove_prefix(close_quote_pos + remove_size);

        }

        return result;
    }
} // icy


#ifdef UNIT_TEST

#include <iostream>

using std::cout;
using std::endl;

unsigned check_counter = 0;

template<typename A,
         typename B>
void
check(A&& a, B&& b)
{
    ++check_counter;
    if (a != b)
        throw runtime_error{"Check failed: \"" + a + "\" != \"" + b + "\""};
    cout << "check " << check_counter << " passed" << endl;
}


int main()
{
    {
        // trivial case
        auto d = icy::parse("StreamTitle='Testing something'");
        check(d.at("StreamTitle"), "Testing something");
    }

    {
        // again, with trailing ';'
        auto d = icy::parse("StreamTitle='Testing something';");
        check(d.at("StreamTitle"), "Testing something");
    }

    {
        // trivial case with two fields
        auto d = icy::parse("StreamTitle='Another test';StreamURL='http://example.com'");
        check(d.at("StreamTitle"), "Another test");
        check(d.at("StreamURL"), "http://example.com");
    }

    {
        // again, with trailing ';'
        auto d = icy::parse("StreamTitle='Another test';StreamURL='http://example.com';");
        check(d.at("StreamTitle"), "Another test");
        check(d.at("StreamURL"), "http://example.com");
    }

    {
        // obnoxious case: title with quote character
        auto d = icy::parse("StreamTitle='Icecast's problem'");
        check(d.at("StreamTitle"), "Icecast's problem");
    }

    {
        // again, with trailing ';'
        auto d = icy::parse("StreamTitle='Icecast's problem';");
        check(d.at("StreamTitle"), "Icecast's problem");
    }


    {
        // fairly ambiguous title
        auto d = icy::parse("StreamTitle='Why's=no quote escaping?'");
        check(d.at("StreamTitle"), "Why's=no quote escaping?");
    }

    {
        // again, trailing ';'
        auto d = icy::parse("StreamTitle='Why's=no quote escaping?';");
        check(d.at("StreamTitle"), "Why's=no quote escaping?");
    }

}

#endif
