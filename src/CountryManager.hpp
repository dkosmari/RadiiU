/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef COUNTRY_MANAGER_HPP
#define COUNTRY_MANAGER_HPP

#include <functional>
#include <string>

namespace CountryManager {

    struct Country {
        std::string code;
        std::string name;
    };


    using CountryCallbackSignature = void(const Country&);
    using CountryFunction = std::function<CountryCallbackSignature>;


    void
    initialize();

    void
    finalize();


    char32_t
    get_codepoint(const std::string& code);


    std::string
    get_utf8(const std::string& code);


    std::string
    get_code(const std::string& name);

    std::string
    get_name(const std::string& code);


    void
    for_each_country(const CountryFunction& func);

} // namespace CountryManager

#endif
