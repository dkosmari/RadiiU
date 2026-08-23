/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STYLES_HPP
#define STYLES_HPP

#include <compare>
#include <functional>
#include <string>


namespace Styles {

    enum class Group {
        imgui,
        builtin,
        user,
    };


    struct Info {

        Group group;
        std::string name;


        bool
        operator ==(const Info& other)
            const noexcept = default;

        std::strong_ordering
        operator <=>(const Info& other)
            const noexcept;

    }; // struct Info


    using StyleInfoCallbackSignature = void(const Info&);
    using StyleInfoFunction = std::function<StyleInfoCallbackSignature>;


    void
    initialize();

    void
    finalize();


    void
    for_each_style(const StyleInfoFunction& func);


    void
    load();


    std::string
    to_label(Group g);


    std::string
    to_label(const Info& info);

} // namespace Styles

#endif
