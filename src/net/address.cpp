/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstddef>
#include <cstdint>
#include <cstring>              // memset()
#include <span>
#include <stdexcept>

#include <arpa/inet.h>          // htons(), inet_ntop()
#include <sys/socket.h>         // AF_INET

#include "address.hpp"


using namespace std::literals;


namespace net {

    address::address(const ::sockaddr* src, socklen_t size)
    {
#ifdef __WIIU__
        if (size != sizeof(sockaddr_in))
            throw std::logic_error{"address size mismatch"};
#endif
        std::memcpy(&storage, src, size);
    }


    address::address(const ::sockaddr_in& src)
        noexcept
    {
        std::memcpy(&storage, &src, sizeof src);
    }


    address::address(const ::sockaddr_in* src)
        noexcept
    {
        std::memcpy(&storage, src, sizeof *src);
    }


#ifdef AF_INET6
    address::address(const ::sockaddr_in6& src)
        noexcept
    {
        std::memcpy(&storage, &src, sizeof src);
    }


    address::address(const ::sockaddr_in6* src)
        noexcept
    {
        std::memcpy(&storage, src, sizeof *src);
    }
#endif


    address::address(ipv4_t ip, port_t port)
        noexcept
    {
        ::sockaddr_in raw{};
        raw.sin_family = AF_INET;
        raw.sin_port = ::htons(port);
        raw.sin_addr.s_addr = ::htonl(ip);
        std::memcpy(&storage, &raw, sizeof raw);
    }



    sa_family_t
    address::family()
        const noexcept
    {
        return storage.ss_family;
    }



    const ::sockaddr*
    address::data()
        const noexcept
    {
        return reinterpret_cast<const ::sockaddr*>(&storage);
    }


    ::sockaddr*
    address::data()
        noexcept
    {
        return reinterpret_cast<::sockaddr*>(&storage);
    }


    const ::sockaddr_in*
    address::data4()
        const
    {
        if (family() != AF_INET)
            throw std::logic_error{"invalid address family"};
        return reinterpret_cast<const ::sockaddr_in*>(&storage);
    }


    ::sockaddr_in*
    address::data4()
    {
        if (family() != AF_INET)
            throw std::logic_error{"invalid address family"};
        return reinterpret_cast<::sockaddr_in*>(&storage);
    }


#ifdef AF_INET6

    const ::sockaddr_in6*
    address::data6()
        const
    {
        if (family() != AF_INET6)
            throw std::logic_error{"invalid address family"};
        return reinterpret_cast<const ::sockaddr_in6*>(&storage);
    }


    ::sockaddr_in6*
    address::data6()
    {
        if (family() != AF_INET6)
            throw std::logic_error{"invalid address family"};
        return reinterpret_cast<::sockaddr_in6*>(&storage);
    }

#endif


    socklen_t
    address::size()
        const noexcept
    {
        switch (family()) {
            case AF_INET:
                return sizeof(::sockaddr_in);
#ifdef AF_INET6
            case AF_INET6:
                return sizeof(::sockaddr_in6);
#endif
            default:
                return 0;
        }
    }


    port_t
    address::port()
        const noexcept
    {
        switch (family()) {

            case AF_INET:
                return ::ntohs(data4()->sin_port);

#ifdef AF_INET6
            case AF_INET6:
                return ::ntohs(data6()->sin6_port);
#endif

            default:
                return 0;

        }
    }


    bool
    address::operator ==(const address& other)
        const noexcept
    {
        if (family() != other.family())
            return false;
        return !std::memcmp(data(), other.data(), size());
    }


    std::strong_ordering
    address::operator <=>(const address& other)
        const noexcept
    {
        auto family_order = family() <=> other.family();
        if (family_order != 0)
            return family_order;

        switch (family()) {

            case AF_INET: {
                // first order by IP, network (big endian) order
                int ip_order = std::memcmp(&data4()->sin_addr,
                                           &other.data4()->sin_addr,
                                           4);
                if (ip_order < 0)
                    return std::strong_ordering::less;
                if (ip_order > 0)
                    return std::strong_ordering::greater;

                // when same IP, compare ports
                return port() <=> other.port();
            }

#ifdef AF_INET6
            case AF_INET6: {
                // first compare by address
                int ip_order = std::memcmp(&data6()->sin6_addr,
                                           &other.data6()->sin6_addr,
                                           sizeof(in6_addr));
                if (ip_order < 0)
                    return std::strong_ordering::less;
                if (ip_order > 0)
                    return std::strong_ordering::greater;

                // when same IP, compare ports
                return port() <=> other.port();
                // NOTE: we ignore the flowinfo and scope_id fields
            }
#endif

            default: {
                // generic ordering
                int order = std::memcmp(data(), other.data(), size());
                if (order < 0)
                    return std::strong_ordering::less;
                if (order > 0)
                    return std::strong_ordering::greater;
                return std::strong_ordering::equal;
            }

        }
    }


    std::string
    to_string(const address& addr)
    {
        switch (addr.family()) {

            case AF_INET: {
                std::vector<char> buffer(INET_ADDRSTRLEN);
                std::string result = inet_ntop(AF_INET,
                                               &addr.data4()->sin_addr,
                                               buffer.data(),
                                               buffer.size());
                if (auto p = addr.port())
                    result += ":" + std::to_string(p);
                return result;
            }

#ifdef AF_INET6
            case AF_INET6: {
                std::vector<char> buffer(INET6_ADDRSTRLEN);
                std::string result = inet_ntop(AF_INET6,
                                               &addr.data6()->sin6_addr,
                                               buffer.data(),
                                               buffer.size());
                if (auto p = addr.port())
                    result = "[" + result + "]:" + std::to_string(p);
                return result;
            }
#endif

            default:
                return "<ERROR: "s + std::to_string(addr.family()) + ">"s;

        }
    }


    std::ostream&
    operator <<(std::ostream& out,
                const address& addr)
    {
        return out << to_string(addr);
    }

} // namespace net


namespace {

    // See https://stackoverflow.com/questions/35985960/c-why-is-boosthash-combine-the-best-way-to-combine-hash-values

    std::size_t
    hash_mix(std::uint64_t x)
    {
        const uint64_t m = 0xe9846afb1a615dull;
        x ^= x >> 32u;
        x *= m;
        x ^= x >> 32u;
        x *= m;
        x ^= x >> 28u;
        if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t))
            return (x >> 32u) ^ x;
        else
            return x;
    }


    template<typename T>
    void
    hash_combine(std::size_t& seed,
                 const T& val)
    {
        seed = hash_mix(seed + 0x9e3779b9 + std::hash<T>{}(val));
    }


    std::uint32_t
    load_u32_be(const std::byte* data)
    {
        return
            (static_cast<std::uint32_t>(data[0]) << 24u) |
            (static_cast<std::uint32_t>(data[1]) << 16u) |
            (static_cast<std::uint32_t>(data[2]) <<  8u) |
            (static_cast<std::uint32_t>(data[3]) <<  0u);
    }


    void
    hash_combine(std::size_t& seed,
                 const void* ptr,
                 std::size_t size)
    {
        auto data = reinterpret_cast<const std::byte*>(ptr);
        std::size_t i = 0;
        for (; i < size; i += 4)
            hash_combine(seed, load_u32_be(data + i));
        for (; i < size; ++i)
            hash_combine(seed, static_cast<std::uint8_t>(data[i]));
    }

} // namespace


std::size_t
std::hash<net::address>::operator ()(const net::address& a)
    const noexcept
{
    std::size_t result = 0;
    hash_combine(result, a.data(), a.size());
    return result;
}
