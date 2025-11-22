/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cinttypes>
#include <cstdarg>
#include <locale>
#include <tuple>

#include <SDL2/SDL_platform.h>

#include "utils.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using namespace std::literals;


namespace utils {

    const std::string&
    get_user_agent()
    {
        static const std::string user_agent = PACKAGE_NAME "/" PACKAGE_VERSION
                                              + " ("s + SDL_GetPlatform() + ")"s;
        return user_agent;
    }


    const std::filesystem::path&
    get_content_path()
    {
        static const std::filesystem::path content_path =
#ifdef __WIIU__
            "/vol/content"
#else
            "assets/content"
#endif
            ;
        return content_path;
    }


    namespace detail {

        template<> const char* format_helper<char> = "c";

        template<> const char* format_helper<std::int8_t>  = PRIi8;
        template<> const char* format_helper<std::uint8_t> = PRIu8;

        template<> const char* format_helper<std::int16_t>  = PRIi16;
        template<> const char* format_helper<std::uint16_t> = PRIu16;

        template<> const char* format_helper<std::int32_t>  = PRIi32;
        template<> const char* format_helper<std::uint32_t> = PRIu32;

        template<> const char* format_helper<std::int64_t>  = PRIi64;
        template<> const char* format_helper<std::uint64_t> = PRIu64;

        template<> const char* format_helper<char *>       = "s";
        template<> const char* format_helper<const char *> = "s";

    } // namespace detail


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


    std::string
    join(const std::vector<std::string>& tokens,
         const std::string& separator,
         bool compress)
    {
        if (tokens.empty())
            return "";

        std::string result;
        std::size_t total = 0;
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
          std::size_t max_tokens)
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
          std::size_t max_tokens)
    {
        return split(input, std::vector{separator}, compress, max_tokens);
    }


    std::vector<std::string_view>
    split(const std::string_view& input,
          const std::vector<std::string_view>& separators,
          bool compress,
          std::size_t max_tokens)
    {
        std::vector<std::string_view> result;

        using size_type = std::string::size_type;
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
    split(const std::string_view& input,
          const std::string_view& separator,
          bool compress,
          std::size_t max_tokens)
    {
        return split(input, std::vector{separator}, compress, max_tokens);
    }


    std::string
    trimmed(const std::string& input,
            char discard)
    {
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
        auto start = input.find_first_not_of(discard);
        if (start == std::string::npos)
            return {};
        auto finish = input.find_last_not_of(discard);
        return input.substr(start, finish - start + 1);
    }


    std::string
    cpp_vsprintf(const char* fmt,
                 va_list args)
    {
        std::va_list args2;
        va_copy(args2, args);
        int size = std::vsnprintf(nullptr, 0, fmt, args2);
        va_end(args2);
        if (size < 0)
            throw std::runtime_error{"vsnprintf() failed"};

        std::string text(size, '\0');
        std::vsnprintf(text.data(), text.size() + 1, fmt, args);
        return text;
    }


    std::string
    cpp_sprintf(const char* fmt,
                ...)
    {
        std::va_list args;
        va_start(args, fmt);
        try {
            auto result = cpp_vsprintf(fmt, args);
            va_end(args);
            return result;
        }
        catch (...) {
            va_end(args);
            throw;
        }
    }


    bool
    equal_case(std::string_view a,
               std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i <a.size(); ++i) {
            if (std::toupper(a[i], std::locale::classic()) !=
                std::toupper(b[i], std::locale::classic()))
                return false;
        }
        return true;
    }

} // namespace utils
