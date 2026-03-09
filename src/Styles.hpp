/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STYLES_HPP
#define STYLES_HPP

#include <compare>
#include <string>
#include <vector>


namespace Styles {

    void
    initialize();

    void
    finalize();

    void
    process_ui();


    enum class Type {
        imgui,
        builtin,
        user,
    };


    std::string
    to_string(Type t);


    struct Info {

        Type type;
        std::string name;


        std::strong_ordering
        operator <=>(const Info& other)
            const noexcept = default;

    }; // struct Info


    const std::vector<Info>&
    get_styles()
        noexcept;


    void
    load();

} // namespace Styles

#endif
