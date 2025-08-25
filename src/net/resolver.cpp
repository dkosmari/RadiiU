/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstring> // memcpy(), memset()
#include <memory>
#include <stdexcept>

#include "resolver.hpp"


namespace std {

    template<>
    struct default_delete<struct ::addrinfo> {
        void operator ()(struct ::addrinfo* p)
            const noexcept
        {
            ::freeaddrinfo(p);
        }
    };

}


namespace net::resolver {

    namespace {

        socket::type
        to_type(int socktype, int protocol)
        {
            if (socktype == SOCK_STREAM && protocol == IPPROTO_TCP)
                return socket::type::tcp;
            if (socktype == SOCK_DGRAM && protocol == IPPROTO_UDP)
                return socket::type::udp;
            return {};
        }

    } // namespace



    void
    address_resolver::process(const std::optional<std::string>& name,
                              const std::optional<std::string>& service)
    {
        // clear previous result
        result.entries.clear();
        result.canon_name.reset();

        struct ::addrinfo hints;
        struct ::addrinfo* hints_ptr = nullptr;

        // if any non-default option is used, we need to pass hints to getaddrinfo()
        if (param.type || param.canon || param.numeric || param.passive) {
            std::memset(&hints, 0, sizeof hints);

            hints.ai_family = param.family.value_or(0);

            hints.ai_flags = 0;
            if (param.canon)
                hints.ai_flags |= AI_CANONNAME;
            if (param.numeric)
                hints.ai_flags |= AI_NUMERICHOST;
            if (param.passive)
                hints.ai_flags |= AI_PASSIVE;

            if (param.type) {
                switch (*param.type) {
                case socket::type::tcp:
                    hints.ai_socktype = SOCK_STREAM;
                    hints.ai_protocol = IPPROTO_TCP;
                    break;
                case socket::type::udp:
                    hints.ai_socktype = SOCK_DGRAM;
                    hints.ai_protocol = IPPROTO_UDP;
                    break;
                }
            }
            hints_ptr = &hints;
        }

        struct ::addrinfo* raw_result_ptr = nullptr;
        int status = ::getaddrinfo(name ? name->data() : nullptr,
                                   service ? service->data() : nullptr,
                                   hints_ptr,
                                   &raw_result_ptr);
        if (status)
            throw std::runtime_error{::gai_strerror(status)};

        // take ownership of the raw pointer
        std::unique_ptr<struct ::addrinfo> info{raw_result_ptr};

        // if a canonical name was requested, it's on the first node
        if (param.canon && info && info->ai_canonname)
            result.canon_name = info->ai_canonname;

        // walk through the linked list
        for (auto node = info.get(); node; node = node->ai_next) {
#ifdef __WIIU__
            // sanity check: Wii U only supports IPv4
            if (node->ai_addrlen != sizeof(sockaddr_in))
                throw std::logic_error{"getaddrinfo() returned invalid result!"};
#endif

            entry_t entry;
            entry.addr = address{node->ai_addr, node->ai_addrlen};
            entry.type = to_type(node->ai_socktype, node->ai_protocol);

            result.entries.push_back(std::move(entry));
        }
    }


    bool
    address_resolver::try_process(const std::optional<std::string>& name,
                const std::optional<std::string>& service)
        noexcept
    {
        error.message.reset();
        try {
            process(name, service);
            return true;
        }
        catch (std::exception& e) {
            error.message = e.what();
            return false;
        }
        catch (...) {
            return false;
        }
    }



    void
    name_resolver::process(const address& addr)
    {
        // clear previous result
        result.name.reset();
        result.service.reset();

        std::vector<char> name(param.name ? NI_MAXHOST : 0);
        std::vector<char> service(param.service ? NI_MAXSERV : 0);

        int flags = 0;
        if (param.name_required)
            flags |= NI_NAMEREQD;
        if (param.datagram)
            flags |= NI_DGRAM;
        if (param.local)
            flags |= NI_NOFQDN;
        if (param.numeric_host)
            flags |= NI_NUMERICHOST;
        if (param.numeric_service)
            flags |= NI_NUMERICSERV;

        int status = ::getnameinfo(addr.data(),    addr.size(),
                                   name.data(),    name.size(),
                                   service.data(), service.size(),
                                   flags);
        if (status)
            throw std::runtime_error{::gai_strerror(status)};

        if (param.name)
            result.name = name.data();
        if (param.service)
            result.service = service.data();
    }


    bool
    name_resolver::try_process(const address& addr)
        noexcept
    {
        error.message.reset();
        try {
            process(addr);
            return true;
        }
        catch (std::exception& e) {
            error.message = e.what();
            return false;
        }
        catch (...) {
            return false;
        }
    }

} // namespace net::addrinfo
