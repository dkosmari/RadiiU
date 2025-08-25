/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NET_RESOLVER_HPP
#define NET_RESOLVER_HPP

#include <optional>
#include <string>
#include <vector>
#include <utility>

#include <netdb.h>

#include "address.hpp"
#include "socket.hpp"


namespace net::resolver {

    struct address_resolver {

        struct {
            std::optional<int> family;
            std::optional<socket::type> type;
            bool canon   = false; // store the canonical name
            bool numeric = false; // only parse numerical notation, no name resolution
            bool passive = false;
        } param;


        struct entry_t {
            address addr;
            socket::type type;
        };


        struct {
            std::vector<entry_t> entries;
            std::optional<std::string> canon_name;
        } result;


        struct {
            std::optional<std::string> message;
        } error;


        void
        process(const std::optional<std::string>& name,
                const std::optional<std::string>& service = {});

        bool
        try_process(const std::optional<std::string>& name,
                    const std::optional<std::string>& service = {})
            noexcept;


    };


    struct name_resolver {

        struct {
            bool name    = true;
            bool service = false;

            bool name_required   = false;
            bool datagram        = false;
            bool local           = false;
            bool numeric_host    = false;
            bool numeric_service = false;
        } param;


        struct {
            std::optional<std::string> name;
            std::optional<std::string> service;
        } result;


        struct {
            std::optional<std::string> message;
        } error;


        void
        process(const address& addr);

        bool
        try_process(const address& addr) noexcept;

    };

} // namespace net::resolver

#endif
