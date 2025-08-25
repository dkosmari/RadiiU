/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NET_ADDRESS_HPP
#define NET_ADDRESS_HPP

#include <compare>
#include <string>
#include <vector>
#include <ostream>

#include <netinet/in.h> // in_addr_t, in_port_t
#include <sys/socket.h> // sockaddr*


namespace net {

    using ipv4_t = in_addr_t;
    using port_t = in_port_t;


    struct address {

        ::sockaddr_storage storage;


        constexpr
        address()
            noexcept
        {
            storage.ss_family = AF_UNSPEC;
        }


        address(const ::sockaddr* ptr, socklen_t size);


        address(const ::sockaddr_in& src) noexcept;
        address(const ::sockaddr_in* src) noexcept;

#ifdef AF_INET6

        address(const ::sockaddr_in6& src) noexcept;
        address(const ::sockaddr_in6* src) noexcept;

#endif

        address(ipv4_t ip, port_t port) noexcept;



        sa_family_t
        family() const noexcept;


        const ::sockaddr*
        data() const noexcept;

        ::sockaddr*
        data() noexcept;


        const ::sockaddr_in*
        data4() const;


        ::sockaddr_in*
        data4();


#ifdef AF_INET6

        const ::sockaddr_in6*
        data6() const;


        ::sockaddr_in6*
        data6();

#endif

        socklen_t
        size() const noexcept;


        port_t
        port() const noexcept;


        bool
        operator ==(const address& other) const noexcept;

        std::strong_ordering
        operator <=>(const address& other) const noexcept;

    };


    std::string to_string(const address& addr);


    std::ostream& operator <<(std::ostream& out, const address& addr);


} // namespace net

#endif
