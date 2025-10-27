/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef JSON_HPP
#define JSON_HPP

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
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
    dump(const value& va, std::ostream& out);


    value
    load(const std::filesystem::path& filename);


    void
    save(const value& val,
         const std::filesystem::path& filename,
         bool replace = true);


    template<typename T>
    [[nodiscard]]
    std::optional<T>
    try_get(const object& obj,
            const std::string& key)
    {
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is<T>())
            return it->second.as<T>();
        return {};
    }


#if 0
    // Explicit JSON type.
    template<typename JT,
             typename RT>
    [[nodiscard]]
    std::optional<RT>
    try_get2(const object& obj,
             const std::string& key)
    {
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is<JT>())
            return static_cast<RT>(it->second.as<JT>());
        return {};
    }
#endif


    // Constrained specialization for integral types.
    template<std::integral I>
    [[nodiscard]]
    std::optional<I>
    try_get(const object& obj,
            const std::string& key)
    {
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is<json::integer>())
            return static_cast<I>(it->second.as<json::integer>());
        return {};
    }


    // Full specialization for bool.
    template<>
    [[nodiscard]]
    std::optional<bool>
    try_get<bool>(const object& obj,
                  const std::string& key);


    template<typename T>
    bool
    try_get(const object& obj,
            const std::string& key,
            T& result)
    {
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is<T>()) {
            result = it->second.as<T>();
            return true;
        }
        return false;
    }


#if 0
    // Explicit JSON type.
    template<typename JT,
             typename RT>
    bool
    try_get2(const object& obj,
             const std::string& key,
             RT& result)
    {
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is<JT>()) {
            result = static_cast<RT>(it->second.as<JT>());
            return true;
        }
        return false;
    }
#endif


    // Constrained specialization for integral types.
    template<std::integral I>
    bool
    try_get(const object& obj,
            const std::string& key,
            I& result)
    {
        auto it = obj.find(key);
        if (it != obj.end() && it->second.is<json::integer>()) {
            result = static_cast<I>(it->second.as<json::integer>());
            return true;
        }
        return false;
    }


    // Full specialization for bool.
    template<>
    bool
    try_get<bool>(const object& obj,
                  const std::string& key,
                  bool& result);

} // namespace json

#endif
