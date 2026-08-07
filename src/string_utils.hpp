/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STRING_UTILS_HPP
#define STRING_UTILS_HPP

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>


namespace string_utils {

    namespace detail {

        template<typename T>
        extern const char* format_helper;

    } // detail


    [[nodiscard]]
    std::string
    concat(const std::string& a,
           const std::string& b,
           const std::string& sep = "");


    [[nodiscard]]
    bool
    equal_case(std::string_view a,
               std::string_view b);


    [[nodiscard]]
    std::vector<std::string>
    from_csv(const std::string& csv,
             bool compress = true);


    [[nodiscard]]
    std::string
    join(const std::vector<std::string>& tokens,
         const std::string& sep = "",
         bool compress = true);


    [[nodiscard]]
    std::vector<std::string>
    split(const std::string& input,
          const std::vector<std::string>& separators = {","},
          bool compress = true,
          std::string::size_type max_tokens = 0);

    [[nodiscard]]
    std::vector<std::string>
    split(const std::string& input,
          const std::string& separator = ",",
          bool compress = true,
          std::string::size_type max_tokens = 0);

    [[nodiscard]]
    std::vector<std::string_view>
    split_view(const std::string_view& input,
               const std::vector<std::string_view>& separators = {","},
               bool compress = true,
               std::string_view::size_type max_tokens = 0);

    [[nodiscard]]
    std::vector<std::string_view>
    split_view(const std::string_view& input,
               const std::string_view& separator = ",",
               bool compress = true,
               std::string_view::size_type max_tokens = 0);


    [[nodiscard]]
    std::string
    to_csv(const std::vector<std::string>& vec,
           bool compress = true);


    // equivalent to trimmed(..., std::isspace)
    [[nodiscard]]
    std::string
    trimmed(const std::string& input);


    [[nodiscard]]
    std::string
    trimmed(const std::string& input,
            char discard);


    [[nodiscard]]
    std::string
    trimmed(const std::string& input,
            const std::string& discard);


    [[nodiscard]]
    std::string
    trimmed(const std::string& input,
            const std::function<bool(std::string::value_type)>& predicate);

    [[nodiscard]]
    std::string
    trimmed(const std::string& input,
            int (*predicate)(int));

} // namespace string_utils

#endif
