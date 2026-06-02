/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CSV_STRINGS_HPP
#define CSV_STRINGS_HPP

#include <optional>
#include <string>
#include <vector>

#include <glaze/json/read.hpp>
#include <glaze/json/write.hpp>


struct csv_strings : std::vector<std::string> {

    using base = std::vector<std::string>;

    // Inherit constructors.
    using base::base;

    // Inherit assignment operators.
    using base::operator=;

    explicit
    csv_strings(const std::optional<std::string>& joined);

    explicit
    operator std::optional<std::string>()
        const;


    // Serialization helper.
    csv_strings(glz::make_reflectable)
    {
    }


    // Make it act like an optional.
    constexpr
    explicit
    operator bool()
        const noexcept
    {
        return !empty();
    }


    constexpr
    const csv_strings*
    operator *()
        const noexcept
    {
        return this;
    }

    constexpr
    csv_strings*
    operator *()
        noexcept
    {
        return this;
    }

}; // struct csv_strings


namespace glz {

    template<>
    struct from<JSON, csv_strings> {
        template<auto Opts>
        static
        void
        op(csv_strings& value,
           is_context auto&& ctx,
           auto&& it,
           auto&& end)
        {
            std::optional<std::string> joined;
            parse<JSON>::op<Opts>(joined, ctx, it, end);
            value = static_cast<csv_strings>(joined);
        }
    };

    template<>
    struct to<JSON, csv_strings> {
        template<auto Opts>
        static
        void
        op(csv_strings& value,
           is_context auto&& ctx,
           auto&& b,
           auto&& ix)
            noexcept
        {
            auto joined = static_cast<std::optional<std::string>>(value);
            serialize<JSON>::op<Opts>(joined, ctx, b, ix);
        }
    };

} // namespace glz

#endif
