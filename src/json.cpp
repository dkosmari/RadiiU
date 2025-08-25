/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <fstream>
#include <utility>

#include <jansson.h>


#include "json.hpp"


namespace json {


    error::error(const char* msg) :
        std::runtime_error{msg}
    {}


    value::value(const value& other) :
        base_type{other}
    {}


    value::value(value&& other)
        noexcept :
        base_type{std::move(other)}
    {}


    value&
    value::operator= (const value& other)
    {
        base_type::operator=(other);
        return *this;
    }


    value&
    value::operator= (value&& other)
        noexcept
    {
        base_type::operator=(std::move(other));
        return *this;
    }


    template<typename T>
    bool
    value::is()
        const noexcept
    {
        return holds_alternative<T>(*this);
    }


    bool
    value::is_null() const noexcept
    {
        return is<nullptr_t>();
    }


    bool
    value::is_number()
        const noexcept
    {
        return is<integer>() || is<real>();
    }


    template<typename T>
    T&
    value::as()
    {
        if (!holds_alternative<T>(*this))
            *this = T{};
        return get<T>(*this);
    }


    template<typename T>
    const T&
    value::as() const
    {
        return get<T>(*this);
    }


    // explicit instantiations

    template bool value::is<array>() const noexcept;
    template bool value::is<bool>() const noexcept;
    template bool value::is<integer>() const noexcept;
    template bool value::is<nullptr_t>() const noexcept;
    template bool value::is<object>() const noexcept;
    template bool value::is<real>() const noexcept;
    template bool value::is<string>() const noexcept;

    template array&     value::as<array>();
    template bool&      value::as<bool>();
    template integer&   value::as<integer>();
    template nullptr_t& value::as<nullptr_t>();
    template object&    value::as<object>();
    template real&      value::as<real>();
    template string&    value::as<string>();

    template const array&     value::as<array>() const;
    template const bool&      value::as<bool>() const;
    template const integer&   value::as<integer>() const;
    template const nullptr_t& value::as<nullptr_t>() const;
    template const object&    value::as<object>() const;
    template const real&      value::as<real>() const;
    template const string&    value::as<string>() const;


    integer
    value::to_integer()
        const
    {
        if (is<integer>())
            return as<integer>();
        if (is<real>())
            return as<real>();
        throw error{"value is not a number"};
    }


    real
    value::to_real()
        const
    {
        if (is<real>())
            return as<real>();
        if (is<integer>())
            return as<integer>();
        throw error{"value is not a number"};
    }


    value
    convert(json_t* j)
    {
        if (!j)
            throw error{"cannot convert null pointer"};

        switch (json_typeof(j)) {

        case JSON_OBJECT:
            {
                object result;
                for (void* iter = json_object_iter(j);
                     iter;
                     iter = json_object_iter_next(j, iter)) {
                    const char* key = json_object_iter_key(iter);
                    json_t* value = json_object_iter_value(iter);
                    if (key && value)
                        result[key] = convert(value);
                }
                return result;
            }
            break;

        case JSON_ARRAY:
            {
                std::size_t n = json_array_size(j);
                array result(n);
                for (std::size_t i = 0; i < n; ++i)
                    result[i] = convert(json_array_get(j, i));
                return result;
            }
            break;

        case JSON_STRING:
            return json_string_value(j);

        case JSON_INTEGER:
            return json_integer_value(j);

        case JSON_REAL:
            return json_real_value(j);

        case JSON_TRUE:
            return true;

        case JSON_FALSE:
            return false;

        case JSON_NULL:
            return nullptr;

        default:
            throw error{"invalid json_t::type"};

        }
    }


    value
    parse(const std::string& s)
    {
        json_error_t e{};
        json_auto_t* root = ::json_loads(s.data(), 0, &e);
        if (!root)
            throw error{e.text};

        return convert(root);
    }


    namespace {

        const std::string base_indent = "    ";


        std::string
        escaped(const std::string& s)
        {
            std::string result;
            result.reserve(s.size());
            for (char c : s) {
                if (c == '"' || c == '\\')
                    result += '\\';
                result += c;
            }
            return result;
        }


        std::string
        repr(nullptr_t)
        {
            return "null";
        }

        std::string
        repr(bool b)
        {
            return b ? "true" : "false";
        }

        std::string
        repr(const string& s)
        {
            return "\"" + escaped(s) + "\"";
        }

        std::string
        repr(integer i)
        {
            return std::to_string(i);
        }

        std::string
        repr(real r)
        {
            return std::to_string(r);
        }


        struct printer {

            std::ostream& out;
            std::string prefix;
            std::string suffix = "\n";


            printer(std::ostream& out) :
                out(out)
            {}


            void
            operator ()(nullptr_t n)
            {
                out << prefix << repr(n) << suffix;
            }


            void
            operator ()(bool b)
            {
                out << prefix << repr(b) << suffix;
            }


            void
            operator ()(const string& s)
            {
                out << prefix << repr(s) << suffix;
            }


            void
            operator ()(integer i)
            {
                out << prefix << repr(i) << suffix;
            }


            void
            operator ()(real r)
            {
                out << prefix << repr(r) << suffix;
            }


            void
            operator ()(const array& a)
            {
                out << prefix << "[\n";

                printer nested{out};
                nested.prefix = prefix + base_indent;
                nested.suffix = ",\n";

                auto print_nested = [&nested](const auto& x) { nested(x); };

                for (std::size_t idx = 0; idx < a.size(); ++idx) {
                    if (idx == a.size() - 1)
                        nested.suffix = "\n";
                    visit(print_nested, a[idx]);
                }

                out << prefix << "]" << suffix;
            }


            void
            operator ()(const object& o)
            {
                out << prefix << "{\n";

                printer nested{out};
                nested.prefix = prefix + base_indent + base_indent;
                nested.suffix = ",\n";

                auto print_nested = [&nested](const auto& x) { nested(x); };

                std::size_t index = 0;
                for (auto& [key, val] : o) {
                    // print key
                    out << prefix << base_indent
                        << repr(key) << ":\n";
                    // print value
                    if (index++ == o.size() - 1)
                        nested.suffix = "\n";
                    std::visit(print_nested, val);
                }

                out << prefix << "}" << suffix;
            }


        }; // struct printer

    } // namespace


    void
    dump(const value& val)
    {
        printer p{std::cout};
        visit([&p](const auto& v) { p(v); }, val);
        std::cout << std::flush;
    }


    value
    load(const std::filesystem::path& filename)
    {
        json_error_t e{};
        json_auto_t* root = ::json_load_file(filename.c_str(), 0, &e);
        if (!root)
            throw error{e.text};
        return convert(root);
    }


    void
    save(const value& val,
         const std::filesystem::path& filename)
    {
        std::ofstream out{filename};
        if (!out)
            throw error{"could not open output file"};
        printer p{out};
        visit([&p](const auto& v) { p(v); }, val);
    }

} // namespace json
