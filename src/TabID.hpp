/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TAB_ID_HPP
#define TAB_ID_HPP

#include <string>


class TabID {

    enum class Name : unsigned {
        favorites,
        browser,
        recent,
        player,
        settings,
        about,

        last_active,

        num_tabs
    };

    Name value = Name::favorites;


    constexpr
    TabID(Name name)
        noexcept :
        value{name}
    {}

public:

    constexpr
    TabID()
        noexcept = default;


    explicit
    TabID(unsigned idx)
        noexcept;


    [[nodiscard]]
    static
    TabID
    from_string(const std::string& str);


    [[nodiscard]]
    static
    std::size_t
    count()
        noexcept;


    [[nodiscard]]
    constexpr
    bool
    operator ==(const TabID& other)
        const noexcept = default;


    friend std::string to_string(TabID tab);
    friend std::string to_ui_string(TabID tab);


    static const TabID favorites;
    static const TabID browser;
    static const TabID recent;
    static const TabID player;
    static const TabID settings;
    static const TabID about;
    static const TabID last_active;

}; // class TabID


inline const TabID TabID::favorites{TabID::Name::favorites};
inline const TabID TabID::browser{TabID::Name::browser};
inline const TabID TabID::recent{TabID::Name::recent};
inline const TabID TabID::player{TabID::Name::player};
inline const TabID TabID::settings{TabID::Name::settings};
inline const TabID TabID::about{TabID::Name::about};
inline const TabID TabID::last_active{TabID::Name::last_active};


[[nodiscard]]
std::string
to_string(TabID tab);


[[nodiscard]]
std::string
to_ui_string(TabID tab);

#endif
