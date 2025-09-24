/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef JSON_HPP
#define JSON_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>


namespace json {


    using std::string;
    using integer = std::int64_t;
    using real = double;


    struct error : std::runtime_error {

        error(const char* msg);
        error(const std::string& msg);

    };


    struct value;

    using value_ptr = std::unique_ptr<value>;


    using array = std::vector<value>;
    using object = std::map<string, value>;


    struct value : std::variant<nullptr_t, bool, string, integer,
                                real, array, object> {

        using base_type = std::variant<nullptr_t, bool, string, integer,
                                       real, array, object>;

        // Inherit constructors.
        using base_type::base_type;

        value(const value& other);

        value(value&& other)
            noexcept;


        value&
        operator= (const value& other);

        value&
        operator= (value&& other)
            noexcept;


        // same as `holds_alternative<T>(v)`
        template<typename T>
        bool
        is()
            const noexcept;

        bool
        is_null()
            const noexcept;
        bool
        is_number()
            const noexcept;


        // same as `get<T>(v)`.
        template<typename T>
        T&
        as();

        template<typename T>
        const T&
        as()
            const;


        // allow conversion real -> integer
        integer
        to_integer()
            const;

        // allow conversion integer -> real
        real
        to_real()
            const;

    }; // struct value


    value
    parse(const std::string& str);


    void
    dump(const value& va, std::ostream& outl);


    value
    load(const std::filesystem::path& filename);


    void
    save(const value& val,
         const std::filesystem::path& filename,
         bool replace = true);

} // namespace json

#endif
