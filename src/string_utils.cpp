/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdarg>
#include <iterator>
#include <locale>
#include <ranges>
#include <tuple>

#include <iostream> // DEBUG

#include "string_utils.hpp"


namespace string_utils {

    std::string
    concat(const std::string& a,
           const std::string& b,
           const std::string& sep)
    {
        if (a.empty())
            return b;
        if (b.empty())
            return a;
        return a + sep + b;
    }


    bool
    equal_case(std::string_view a,
               std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        auto loc = std::locale::classic();
        for (std::size_t i = 0; i <a.size(); ++i) {
            if (std::toupper(a[i], loc) !=
                std::toupper(b[i], loc))
                return false;
        }
        return true;
    }


    std::vector<std::string>
    from_csv(const std::string& csv,
             bool compress)
    {
        std::vector<std::string> result;
        for (auto token : csv | std::views::split(','))
            if (!compress || !token.empty())
                result.emplace_back(token.cbegin(), token.cend());
        return result;
    }


    std::string
    join(const std::vector<std::string>& tokens,
         const std::string& separator,
         bool compress)
    {
        if (tokens.empty())
            return "";

        std::string result;
        std::string::size_type total = 0;
        if (compress) {
            for (const auto& tok : tokens) {
                if (tok.empty())
                    continue;
                if (total)
                    total += separator.size();
                total += tok.size();
            }
            result.reserve(total);
            for (const auto& tok : tokens) {
                if (tok.empty())
                    continue;
                if (!result.empty())
                    result += separator;
                result += tok;
            }
        } else {
            for (const auto& tok : tokens)
                total += tok.size();
            total += separator.size() * (tokens.size() - 1);
            result.reserve(total);
            result += tokens[0];
            for (std::size_t i = 1; i < tokens.size(); ++i) {
                result += separator;
                result += tokens[i];
            }
        }
        return result;
    }


    // Used by the split() functions.
    namespace {

        std::tuple<std::string::size_type,
                   std::vector<std::string>::size_type>
        find_first_of(const std::string& haystack,
                      const std::vector<std::string>& needles,
                      std::string::size_type start = 0)
        {
            std::string::size_type result_pos = std::string::npos;
            std::vector<std::string>::size_type result_index = 0;
            for (std::vector<std::string>::size_type i = 0; i < needles.size(); ++i) {
                auto pos = haystack.find(needles[i], start);
                if (pos < result_pos) {
                    result_pos = pos;
                    result_index = i;
                }
            }
            return { result_pos, result_index };
        }


        std::tuple<std::string_view::size_type,
                   std::vector<std::string_view>::size_type>
        find_first_of(const std::string_view& haystack,
                      const std::vector<std::string_view>& needles,
                      std::string_view::size_type start = 0)
        {
            std::string_view::size_type result_pos = std::string_view::npos;
            std::vector<std::string_view>::size_type result_index = 0;
            for (std::vector<std::string_view>::size_type i = 0; i < needles.size(); ++i) {
                auto pos = haystack.find(needles[i], start);
                if (pos < result_pos) {
                    result_pos = pos;
                    result_index = i;
                }
            }
            return { result_pos, result_index };
        }

    } // namespace


    std::vector<std::string>
    split(const std::string& input,
          const std::vector<std::string>& separators,
          bool compress,
          std::string::size_type max_tokens)
    {
        std::vector<std::string> result;

        using size_type = std::string::size_type;
        auto [sep_start, sep_index] = find_first_of(input, separators);
        size_type tok_start = 0;

        // Loop until no more separators are found.
        while (sep_start != std::string::npos) {
            if (!compress || sep_start > tok_start) {
                // If this token reaches the maximum allowed, stop the looop
                if (max_tokens && result.size() + 1 == max_tokens)
                    break;
                result.push_back(input.substr(tok_start, sep_start - tok_start));
            }
            tok_start = sep_start + separators[sep_index].size();
            if (tok_start >= input.size())
                break;
            std::tie(sep_start, sep_index) = find_first_of(input, separators, tok_start);
        }
        // The remainder of the string is the last token, unless (compress && empty)
        if (!compress || tok_start < input.size())
            result.push_back(input.substr(tok_start));
        return result;
    }


    std::vector<std::string>
    split(const std::string& input,
          const std::string& separator,
          bool compress,
          std::string::size_type max_tokens)
    {
        return split(input, std::vector{separator}, compress, max_tokens);
    }


    std::vector<std::string_view>
    split_view(const std::string_view& input,
               const std::vector<std::string_view>& separators,
               bool compress,
               std::string_view::size_type max_tokens)
    {
        std::vector<std::string_view> result;

        using size_type = std::string_view::size_type;
        auto [sep_start, sep_index] = find_first_of(input, separators);
        size_type tok_start = 0;

        // Loop until no more separators are found.
        while (sep_start != std::string_view::npos) {
            if (!compress || sep_start > tok_start) {
                // If this token reaches the maximum allowed, stop the looop
                if (max_tokens && result.size() + 1 == max_tokens)
                    break;
                result.push_back(input.substr(tok_start, sep_start - tok_start));
            }
            tok_start = sep_start + separators[sep_index].size();
            if (tok_start >= input.size())
                break;
            std::tie(sep_start, sep_index) = find_first_of(input, separators, tok_start);
        }
        // The remainder of the string is the last token, unless (compress && empty)
        if (!compress || tok_start < input.size())
            result.push_back(input.substr(tok_start));
        return result;
    }


    std::vector<std::string_view>
    split_view(const std::string_view& input,
               const std::string_view& separator,
               bool compress,
               std::string_view::size_type max_tokens)
    {
        return split_view(input, std::vector{separator}, compress, max_tokens);
    }


    std::string
    to_csv(const std::vector<std::string>& vec,
           bool compress)
    {
        return join(vec, ",", compress);
    }


    std::string
    to_upper(std::string input)
    {
        auto loc = std::locale::classic();
        for (auto& c : input)
            c = std::toupper(c, loc);
        return input;
    }


    std::string
    trimmed(const std::string& input)
    {
        return trimmed(input, std::isspace);
    }


    std::string
    trimmed(const std::string& input,
            char discard)
    {
        if (input.empty())
            return {};
        auto start = input.find_first_not_of(discard);
        if (start == std::string::npos)
            return {};
        auto finish = input.find_last_not_of(discard);
        return input.substr(start, finish - start + 1);
    }


    std::string
    trimmed(const std::string& input,
            const std::string& discard)
    {
        if (input.empty())
            return {};
        auto start = input.find_first_not_of(discard);
        if (start == std::string::npos)
            return {};
        auto finish = input.find_last_not_of(discard);
        return input.substr(start, finish - start + 1);
    }


    std::string
    trimmed(const std::string& input,
            const std::function<bool(std::string::value_type)>& predicate)
    {
        if (input.empty())
            return {};
        auto start = std::ranges::find_if_not(input, predicate);
        if (start == input.end())
            return {};
        auto finish = std::ranges::find_if_not(input | std::views::reverse, predicate).base();
        return std::string{start, finish};
    }


    std::string
    trimmed(const std::string& input,
            int (*predicate)(int))
    {
        return trimmed(input, [predicate](std::string::value_type c) -> bool
        {
            return predicate(static_cast<unsigned char>(c));
        });
    }

} // namespace string_utils
