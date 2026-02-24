/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <concepts>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>


namespace utils {

    [[nodiscard]]
    const std::string&
    get_user_agent();


    [[nodiscard]]
    const std::filesystem::path&
    get_content_path();


    namespace detail {

        template<typename T>
        extern const char* format_helper;

    } // detail


    template<typename T>
    [[nodiscard]]
    std::string
    format(T&)
    {
        return detail::format_helper<std::remove_cv_t<T>>;
    }


    [[nodiscard]]
    std::string
    concat(const std::string& a,
           const std::string& b,
           const std::string& sep = "");


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
          std::size_t max_tokens = 0);

    [[nodiscard]]
    std::vector<std::string>
    split(const std::string& input,
          const std::string& separator = ",",
          bool compress = true,
          std::size_t max_tokens = 0);

    [[nodiscard]]
    std::vector<std::string_view>
    split(const std::string_view& input,
          const std::vector<std::string_view>& separators = {","},
          bool compress = true,
          std::size_t max_tokens = 0);

    [[nodiscard]]
    std::vector<std::string_view>
    split(const std::string_view& input,
          const std::string_view& separator = ",",
          bool compress = true,
          std::size_t max_tokens = 0);


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


    // Overload to easily use predicates from <cctype>, such as std::isspace()
    [[nodiscard]]
    inline
    std::string
    trimmed(const std::string& input,
            int (*predicate)(int))
    {
        return trimmed(input, [predicate](std::string::value_type c) -> bool
        {
            return predicate(static_cast<unsigned char>(c));
        });
    }


    [[nodiscard]]
    std::string
    cpp_vsprintf(const char* fmt,
                 va_list args)
        __attribute__ (( __format__(__printf__, 1, 0) ));

    [[nodiscard]]
    std::string
    cpp_sprintf(const char* fmt,
                ...)
        __attribute__ (( __format__(__printf__, 1, 2) ));


    template<typename T,
             std::floating_point F>
    [[nodiscard]]
    T
    lerp(const T& a,
         const T& b,
         F t)
    {
        return a + t * (b - a);
    }


    bool
    equal_case(std::string_view a,
               std::string_view b);

} // namespace utils

#endif
