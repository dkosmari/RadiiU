/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BROWSER_SEARCH_POPUP_HPP
#define BROWSER_SEARCH_POPUP_HPP

#include <functional>
#include <optional>
#include <string>


namespace BrowserSearchPopup {

    // Convenience order enum, to combine both sorting and direction.
    enum class Order : unsigned {
        name_asc,
        name_desc,
        country_asc,
        country_desc,
        language_asc,
        language_desc,
        votes_asc,
        votes_desc,
        clicks_asc,
        clicks_desc,
        random,
        count
    };


    struct Filter {
        std::string name;
        std::string tag;
        std::string country;
        std::string codec;
    };


    struct SearchParams {
        Filter filter = {};
        Order order = Order::clicks_desc;
    };


    using ConfirmCallbackSignature = void(const SearchParams& params);

    using ConfirmFunction = std::move_only_function<ConfirmCallbackSignature>;


    void
    initialize();

    void
    finalize();


    void
    open(ConfirmFunction func);


    void
    process_ui();

} // namespace BrowserSearchPopup

#endif
