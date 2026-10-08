/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <regex>
#include <stdexcept>
#include <string>

#include "mime_type.hpp"

#include "string_utils.hpp"


using namespace std::literals;


namespace mime_type {

    namespace {
        bool
        glob_match(std::string_view input,
                   std::string_view glob_rule)
        {
            // quick check for trivial match
            if (glob_rule == "*")
                return true;

            std::string regex_rule;
            for (char c : glob_rule) {
                switch (c) {
                    // convert glob wildcard into regex wildcard
                    case '*':
                        regex_rule += ".*";
                        break;

                    // Escape everything else that's special in ECMAScript regex
                    case '$':
                    case '\\':
                    case '.':
                    case '+':
                    case '?':
                    case '(':
                    case ')':
                    case '[':
                    case ']':
                    case '{':
                    case '}':
                    case '|':
                        regex_rule += '\\';
                        [[fallthrough]];
                    default:
                        regex_rule += c;
                }
            }
            std::regex regex{
                regex_rule,
                std::regex_constants::ECMAScript |
                std::regex_constants::icase
            };
            return regex_match(std::string{input}, regex);
        }

    } // namespace


    bool
    match(const std::string& input,
          std::string_view candidate)
    {
        using string_utils::split_view;

        auto input_sv = std::string_view{input};
        // ignore the parameter, if present
        // TODO: should process parameters too
        std::string_view no_param_input;
        auto semicolon_pos = input_sv.find(';');
        if (semicolon_pos != std::string_view::npos)
            no_param_input = input_sv.substr(0, semicolon_pos);
        else
            no_param_input = input_sv;

        auto input_tokens = split_view(no_param_input, "/"sv, false, 2);
        if (input_tokens.size() != 2) {
            //throw std::runtime_error{"invalid mime-type in input"};
            return false;
        }
        auto& input_type = input_tokens[0];
        auto& input_subtype = input_tokens[1];

        auto candidate_tokens = split_view(candidate, "/"sv, false, 2);
        if (candidate_tokens.size() != 2)
            throw std::runtime_error{"invalid mime-type in candidate"};
        auto& candidate_type = candidate_tokens[0];
        auto& candidate_subtype = candidate_tokens[1];

        if (!candidate_type.contains('*')) {
            // if no wildcards, it has to be an exact match
            if (input_type != candidate_type)
                return false;
        } else {
            if (!glob_match(input_type, candidate_type))
                return false;
        }

        if (!candidate_subtype.contains('*')) {
            // if no wildcards, it has to be an exact match
            if (input_subtype != candidate_subtype)
                return false;
        } else {
            if (!glob_match(input_subtype, candidate_subtype))
                return false;
        }
        return true;
    }

} // namespace mime_type


#ifdef UNIT_TEST
// g++ -std=c++23 -DUNIT_TEST mime_type.cpp string_utils.cpp

#include <iostream>
#include <cstdlib>

using std::cout;
using std::endl;

#define TEST_MATCH(x, y)                                                        \
    do {                                                                        \
        ++total;                                                                \
        if (mime_type::match(x, y)) {                                           \
            ++successes;                                                        \
            cout << "Passed: \"" << x << "\" matches \"" << y << "\"" << endl;  \
        } else {                                                                \
            cout << "Failed: \"" << x << "\" does not match \"" << y << "\"" << endl; \
        }                                                                       \
    }                                                                           \
    while (false)

#define TEST_NO_MATCH(x, y)                                                     \
    do {                                                                        \
        ++total;                                                                \
        if (!mime_type::match(x, y)) {                                           \
            ++successes;                                                        \
            cout << "Passed: \"" << x << "\" does not match \"" << y << "\"" << endl;  \
        } else {                                                                \
            cout << "Failed: \"" << x << "\" matches \"" << y << "\"" << endl; \
        }                                                                       \
    }                                                                           \
    while (false)

int main()
{
    int successes = 0;
    int total = 0;

    TEST_MATCH("audio/mpegurl", "*/*");
    TEST_MATCH("audio/mpegurl", "*/mpegurl");
    TEST_MATCH("audio/mpegurl", "*/*mpegurl");
    TEST_MATCH("application/vnd.apple.mpegurl", "*/*mpegurl");

    cout << "Successes: " << successes << " / " << total << endl;
    return successes < total ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
