/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cerrno>
#include <cstddef>              // byte
#include <stdexcept>
#include <thread>

#include <arpa/inet.h>          // ntohl()
#include <sys/socket.h>         // socket()
#include <unistd.h>             // close()

#ifndef __WIIU__
#include <fcntl.h>
#endif

#include "socket.hpp"


// Note: WUT doesn't have SOL_IP, but IPPROTO_IP seems to work.
#ifndef SOL_IP
#define SOL_IP IPPROTO_IP
#endif


namespace net {

    // bitwise operations for socket::msg_flags

    socket::msg_flags
    operator &(socket::msg_flags a, socket::msg_flags b)
        noexcept
    { return socket::msg_flags{static_cast<int>(a) & static_cast<int>(b)}; }

    socket::msg_flags
    operator ^(socket::msg_flags a, socket::msg_flags b)
        noexcept
    { return socket::msg_flags{static_cast<int>(a) ^ static_cast<int>(b)}; }

    socket::msg_flags
    operator |(socket::msg_flags a, socket::msg_flags b)
        noexcept
    { return socket::msg_flags{static_cast<int>(a) | static_cast<int>(b)}; }

    socket::msg_flags
    operator ~(socket::msg_flags a)
        noexcept
    { return socket::msg_flags{~static_cast<int>(a)}; }


    // bitwise operations for socket::poll_flags

    socket::poll_flags
    operator &(socket::poll_flags a, socket::poll_flags b)
        noexcept
    { return socket::poll_flags(static_cast<short>(a) & static_cast<short>(b)); }

    socket::poll_flags
    operator ^(socket::poll_flags a, socket::poll_flags b)
        noexcept
    { return socket::poll_flags(static_cast<short>(a) ^ static_cast<short>(b)); }

    socket::poll_flags
    operator |(socket::poll_flags a, socket::poll_flags b)
        noexcept
    { return socket::poll_flags(static_cast<short>(a) | static_cast<short>(b)); }

    socket::poll_flags
    operator ~(socket::poll_flags a)
        noexcept
    { return socket::poll_flags(~static_cast<short>(a)); }



    socket::socket(int fd) :
        fd{fd}
    {}


    socket::socket(int family, type t)
    {
        switch (t) {
        case type::tcp:
            fd = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
            break;
        case type::udp:
            fd = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
            break;
        }

        if (fd == -1)
            throw error{errno};
    }


    socket::socket(socket&& other)
        noexcept :
        fd{other.fd}
    {
        other.fd = -1;
    }


    socket&
    socket::operator =(socket&& other)
        noexcept
    {
        if (this != &other) {
            try {
                close();
                fd = other.fd;
                other.fd = -1;
            }
            catch (std::exception& e) {
                //WHBLogPrintf("socket::operator=() failed: %s", e.what());
            }
        }
        return *this;
    }


    socket::~socket()
    {
        try {
            close();
        }
        catch (std::exception& e) {
            //WHBLogPrintf("socket::~socket() failed: %s", e.what());
        }
    }


    socket
    socket::make_tcp(int family)
    {
        return socket{family, type::tcp};
    }


    socket
    socket::make_udp(int family)
    {
        return socket{family, type::udp};
    }


    socket::operator bool()
        const noexcept
    {
        return is_socket();
    }


    bool
    socket::is_socket()
        const noexcept
    {
        return fd != -1;
    }


    std::pair<socket, address>
    socket::accept()
    {
        address addr;
        socklen_t len = sizeof addr.storage;
        int new_fd = ::accept(fd, addr.data(), &len);
        if (new_fd == -1)
            throw error{errno};

        socket new_socket{new_fd};

        if (len != addr.size())
            throw std::logic_error{"unknown address size in accept(): " + std::to_string(len)};

        return { std::move(new_socket), std::move(address{addr}) };
    }


     void
     socket::bind(const address& addr)
     {
         int status = ::bind(fd, addr.data(), addr.size());
         if (status == -1)
             throw error{errno};
     }


    void
    socket::close()
    {
        if (is_socket()) {
            if (::close(fd) == -1)
                throw error{errno};
        }
        fd = -1;
    }


    void
    socket::connect(ipv4_t ip,
                    port_t port)
    {
        connect({ip, port});
    }


    void
    socket::connect(const address& addr)
    {
        int status = ::connect(fd, addr.data(), addr.size());
        if (status == -1)
            throw error{errno};
    }


    std::expected<std::uint8_t, error>
    socket::getsockopt(ip_option opt)
        const noexcept
    {
        unsigned val = 0;
        socklen_t len = sizeof val;
        int status = ::getsockopt(fd, SOL_IP, static_cast<int>(opt),
                                  &val, &len);
        if (status == -1)
            return std::unexpected{error{errno}};

        return val;
    }


    template<typename T>
    std::expected<T, error>
    socket::getsockopt(socket_option opt)
        const noexcept
    {
        T val = {};
        socklen_t len = sizeof val;
        int status = ::getsockopt(fd, SOL_SOCKET, static_cast<int>(opt),
                                  &val, &len);
        if (status == -1)
            return std::unexpected{error{errno}};

        return val;
    }


    std::expected<unsigned, error>
    socket::getsockopt(tcp_option opt)
        const noexcept
    {
        unsigned val = 0;
        socklen_t len = sizeof val;
        int status = ::getsockopt(fd, SOL_TCP, static_cast<int>(opt),
                                  &val, &len);
        if (status == -1)
            return std::unexpected{error{errno}};

        return val;
    }


    namespace {

        // convenience function to convert between std::expected<> types.
        template<typename Dst,
                 typename Src>
        std::expected<Dst, error>
        convert(const std::expected<Src, error>& e)
        {
            if (!e)
                return std::unexpected{e.error()};
            return static_cast<Dst>(*e);
        }

    }

    // getters for ip_option

    std::expected<std::uint8_t, error>
    socket::get_tos()
        const noexcept
    { return getsockopt(ip_option::tos); }

    std::expected<std::uint8_t, error>
    socket::get_ttl()
        const noexcept
    { return getsockopt(ip_option::ttl); }


    // getters for socket_option

    std::expected<bool, error>
    socket::get_broadcast()
        const noexcept
    { return convert<bool>(getsockopt<unsigned>(socket_option::broadcast)); }

    std::expected<bool, error>
    socket::get_dontroute()
        const noexcept
    { return convert<bool>(getsockopt<unsigned>(socket_option::dontroute)); }

    std::expected<error, error>
    socket::get_error()
        const noexcept
    { return convert<error>(getsockopt<int>(socket_option::error)); }

#ifdef SO_HOPCNT
    std::expected<unsigned, error>
    socket::get_hopcnt()
        const noexcept
    { return getsockopt<unsigned>(socket_option::hopcnt); }
#endif

    std::expected<bool, error>
    socket::get_keepalive()
        const noexcept
    { return convert<bool>(getsockopt<unsigned>(socket_option::keepalive)); }

#ifdef SO_KEEPCNT
    std::expected<unsigned, error>
    socket::get_keepcnt()
        const noexcept
    { return getsockopt<unsigned>(socket_option::keepcnt); }
#endif

#ifdef SO_KEEPIDLE
    std::expected<unsigned, error>
    socket::get_keepidle()
        const noexcept
    { return getsockopt<unsigned>(socket_option::keepidle); }
#endif

#ifdef SO_KEEPINTVL
    std::expected<unsigned, error>
    socket::get_keepintvl()
        const noexcept
    { return getsockopt<unsigned>(socket_option::keepintvl); }
#endif

    std::expected<::linger, error>
    socket::get_linger()
        const noexcept
    { return getsockopt<::linger>(socket_option::linger); }


#ifdef SO_MAXMSG
    std::expected<unsigned, error>
    socket::get_maxmsg()
        const noexcept
    { return getsockopt<unsigned>(socket_option::maxmsg); }
#endif


#ifdef SO_MYADDR
    std::expected<address, error>
    socket::get_myaddr()
        const noexcept
    {
        auto r = getsockopt<unsigned>(socket_option::myaddr);
        if (!r)
            return std::unexpected{r.error()};
        ipv4_t ip = ntohl(*r);
        return address{ip, 0};
    }
#endif


    std::expected<bool, error>
    socket::get_nonblock()
        const
    {
#ifdef SO_NONBLOCK

        return convert<bool>(getsockopt<unsigned>(socket_option::nonblock));

#elif defined(_WIN32)

        // Winsock has no way to read the blocking property.
        throw error{ENOSYS};

#else
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
            throw error{errno};
        return flags & O_NONBLOCK;

#endif
    }


    std::expected<bool, error>
    socket::get_oobinline()
        const noexcept
    { return convert<bool>(getsockopt<unsigned>(socket_option::oobinline)); }

    std::expected<unsigned, error>
    socket::get_rcvbuf()
        const noexcept
    { return getsockopt<unsigned>(socket_option::rcvbuf); }

    std::expected<unsigned, error>
    socket::get_rcvlowat()
        const noexcept
    { return getsockopt<unsigned>(socket_option::rcvlowat); }

    std::expected<bool, error>
    socket::get_reuseaddr()
        const noexcept
    { return convert<bool>(getsockopt<unsigned>(socket_option::reuseaddr)); }

#ifdef SO_RUSRBUF
    std::expected<bool, error>
    socket::get_rusrbuf()
        const noexcept
    { return convert<bool>(getsockopt<unsigned>(socket_option::rusrbuf)); }
#endif

#ifdef SO_RXDATA
    std::expected<unsigned, error>
    socket::get_rxdata()
        const noexcept
    { return getsockopt<unsigned>(socket_option::rxdata); }
#endif

    std::expected<unsigned, error>
    socket::get_sndbuf()
        const noexcept
    { return getsockopt<unsigned>(socket_option::sndbuf); }

    std::expected<unsigned, error>
    socket::get_sndlowat()
        const noexcept
    { return getsockopt<unsigned>(socket_option::sndlowat); }

#ifdef SO_TCPSACK
    std::expected<bool, error>
    socket::get_tcpsack()
        const noexcept
    { return convert<bool>(getsockopt<unsigned>(socket_option::tcpsack)); }
#endif

#ifdef SO_TXDATA
    std::expected<unsigned, error>
    socket::get_txdata()
        const noexcept
    { return getsockopt<unsigned>(socket_option::txdata); }
#endif


    std::expected<socket::type, error>
    socket::get_type()
        const noexcept
    {
        auto r = getsockopt<unsigned>(socket_option::type);
        if (!r)
            return std::unexpected{r.error()};
        if (*r == SOCK_STREAM)
            return type::tcp;
        if (*r == SOCK_DGRAM)
            return type::udp;
        return std::unexpected{error{0}};
    }


#ifdef SO_WINSCALE
    std::expected<bool, error>
    socket::get_winscale()
        const noexcept
    { return convert<bool>(getsockopt<unsigned>(socket_option::winscale)); }
#endif


    // getters for tcp_option

#ifdef TCP_ACKDELAYTIME
    std::expected<std::chrono::milliseconds, error>
    socket::get_ackdelaytime()
        const noexcept
    { return convert<std::chrono::milliseconds>(getsockopt(tcp_option::ackdelaytime)); }
#endif

#ifdef TCP_ACKFREQUENCY
    std::expected<unsigned, error>
    socket::get_ackfrequency()
        const noexcept
    { return getsockopt(tcp_option::ackfrequency); }
#endif

    std::expected<unsigned, error>
    socket::get_maxseg()
        const noexcept
    { return getsockopt(tcp_option::maxseg); }

#ifdef TCP_NOACKDELAY
    std::expected<bool, error>
    socket::get_noackdelay()
        const noexcept
    { return convert<bool>(getsockopt(tcp_option::noackdelay)); }
#endif

    std::expected<bool, error>
    socket::get_nodelay()
        const noexcept
    { return convert<bool>(getsockopt(tcp_option::nodelay)); }


    address
    socket::getpeername() const
    {
        sockaddr_in raw_addr = {};
        socklen_t len = sizeof raw_addr;
        int status = ::getpeername(fd,
                                   reinterpret_cast<sockaddr*>(&raw_addr),
                                   &len);
        if (status == -1)
            throw error{errno};

        return address{raw_addr};
    }


    address
    socket::get_remote_address()
        const
    {
        return getpeername();
    }


    address
    socket::getsockname()
        const
    {
        sockaddr_in raw_addr = {};
        socklen_t len = sizeof raw_addr;
        int status = ::getsockname(fd,
                                   reinterpret_cast<sockaddr*>(&raw_addr),
                                   &len);
        if (status == -1)
            throw error{errno};

        return address{raw_addr};
    }


    address
    socket::get_local_address()
        const
    {
        return getsockname();
    }


    void
    socket::listen(int backlog)
    {
        int status = ::listen(fd, backlog);
        if (status == -1)
            throw error{errno};
    }


    socket::poll_flags
    socket::poll(poll_flags flags,
                 std::chrono::milliseconds timeout)
        const
    {
        auto status = try_poll(flags, timeout);
        if (!status)
            throw status.error();
        return *status;
    }


    bool
    socket::is_readable(std::chrono::milliseconds timeout)
        const
    {
        auto status = try_is_readable(timeout);
        if (!status)
            throw status.error();
        return *status;
    }


    bool
    socket::is_writable(std::chrono::milliseconds timeout)
        const
    {
        auto status = try_is_writable(timeout);
        if (!status)
            throw status.error();
        return *status;
    }


    std::size_t
    socket::recv(void* buf, std::size_t len,
                 msg_flags flags)
    {
        auto status = try_recv(buf, len, flags);
        if (!status)
            throw status.error();
        return *status;
    }


    std::size_t
    socket::recv_all(void* vbuf, std::size_t total,
                     msg_flags flags)
    {
        auto buf = static_cast<std::byte*>(vbuf);
        std::size_t received = 0;
        while (received < total) {
            auto status = try_recv(buf + received, total - received, flags);
            if (!status) {
                auto& e = status.error();
                if (e.code() == std::errc::operation_would_block) {
                    // harmless error, just try again
                    std::this_thread::yield();
                    continue;
                }
                throw e;
            }
            if (!*status) // connection was closed gracefully
                break;
            received += *status;
        }
        return received;
    }


    std::pair<std::size_t, address>
    socket::recvfrom(void* buf, std::size_t len,
                     msg_flags flags)
    {
        sockaddr_in src;
        socklen_t src_size = sizeof src;
        auto status = ::recvfrom(fd,
                                 buf, len,
                                 static_cast<int>(flags),
                                 reinterpret_cast<sockaddr*>(&src),
                                 &src_size);
        if (status == -1)
            throw error{errno};
        return {status, src};
    }


    int
    socket::release()
        noexcept
    {
        int result = fd;
        fd = -1;
        return result;
    }


    std::size_t
    socket::send(const void* buf, std::size_t len,
                 msg_flags flags)
    {
        auto status = ::send(fd, buf, len, static_cast<int>(flags));
        if (status == -1)
            throw error{errno};
        return status;
    }


    std::size_t
    socket::send_all(const void* vbuf,
                     std::size_t total,
                     msg_flags flags)
    {
        auto buf = static_cast<const std::byte*>(vbuf);
        std::size_t sent = 0;
        while (sent < total) {
            auto status = try_send(buf + sent, total - sent, flags);
            if (!status) {
                auto& e = status.error();
                if (e.code() == std::errc::operation_would_block) {
                    // harmless error, just try again
                    std::this_thread::yield();
                    continue;
                }
                throw e;
            }
            if (!*status) // connection was closed gracefully
                break;
            sent += *status;
        }
        return sent;
    }


    std::size_t
    socket::sendto(const void* buf, std::size_t len,
                   address dst,
                   msg_flags flags)
    {
        auto raw_dst = dst.data();
        auto status = ::sendto(fd,
                               buf, len,
                               static_cast<int>(flags),
                               reinterpret_cast<const sockaddr*>(&raw_dst),
                               sizeof raw_dst);
        if (status == -1)
            throw error{errno};
        return status;
    }


    void
    socket::setsockopt(ip_option opt,
                       std::uint8_t arg)
    {
        unsigned uarg = arg;
        int status = ::setsockopt(fd, SOL_IP, static_cast<int>(opt), &uarg, sizeof uarg);
        if (status == -1)
            throw error{errno};
    }


    void
    socket::setsockopt(socket_option opt)
    {
        int status = ::setsockopt(fd, SOL_SOCKET, static_cast<int>(opt), nullptr, 0);
        if (status == -1)
            throw error{errno};
    }


    void
    socket::setsockopt(socket_option opt,
                       unsigned arg)
    {
        int status = ::setsockopt(fd, SOL_SOCKET, static_cast<int>(opt), &arg, sizeof arg);
        if (status == -1)
            throw error{errno};
    }


    void
    socket::setsockopt(socket_option opt,
                       const struct ::linger& arg)
    {
        int status = ::setsockopt(fd, SOL_SOCKET, static_cast<int>(opt), &arg, sizeof arg);
        if (status == -1)
            throw error{errno};
    }


    void
    socket::setsockopt(tcp_option opt,
                       unsigned arg)
    {
        int status = ::setsockopt(fd, SOL_TCP, static_cast<int>(opt), &arg, sizeof arg);
        if (status == -1)
            throw error{errno};
    }


    // IP

    void
    socket::set_tos(std::uint8_t t)
    { setsockopt(ip_option::tos, t); }

    void
    socket::set_ttl(std::uint8_t t)
    { setsockopt(ip_option::ttl, t); }


    // socket

#ifdef SO_BIO
    void
    socket::set_bio()
    { setsockopt(socket_option::bio); }
#endif

    void
    socket::set_broadcast(bool enable)
    { setsockopt(socket_option::broadcast, enable); }

    void
    socket::set_dontroute(bool enable)
    { setsockopt(socket_option::dontroute, enable); }

    void
    socket::set_keepalive(bool enable)
    { setsockopt(socket_option::keepalive, enable); }

#ifdef SO_KEEPCNT
    void
    socket::set_keepcnt(unsigned count)
    { setsockopt(socket_option::keepcnt, count); }
#endif

#ifdef SO_KEEPIDLE
    void
    socket::set_keepidle(unsigned period)
    { setsockopt(socket_option::keepidle, period); }
#endif

#ifdef SO_KEEPINTVL
    void
    socket::set_keepintvl(unsigned interval)
    { setsockopt(socket_option::keepintvl, interval); }
#endif

    void
    socket::set_linger(bool enable, int period)
    { setsockopt(socket_option::linger, ::linger{enable, period}); }

#ifdef SO_MAXMSG
    void
    socket::set_maxmsg(unsigned size)
    { setsockopt(socket_option::maxmsg, size); }
#endif


#ifdef SO_NBIO
    void
    socket::set_nbio()
    { setsockopt(socket_option::nbio); }
#endif

    void
    socket::set_nonblock(bool enable)
    {
#ifdef SO_NONBLOCK

        setsockopt(socket_option::nonblock, enable);

#elif defined(_WIN32)

        unsigned long value = enable;
        ioctlsocket(fd, FIONBIO, &value);

#else

        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
            throw error{errno};
        if (enable)
            flags |= O_NONBLOCK;
        else
            flags &= ~O_NONBLOCK;
        int e = fcntl(fd, F_SETFL, flags);
        if (e == -1)
            throw error{errno};

#endif
    }


#ifdef SO_NOSLOWSTART
    void
    socket::set_noslowstart(bool enable)
    { setsockopt(socket_option::noslowstart, enable); }
#endif

    void
    socket::set_oobinline(bool enable)
    { setsockopt(socket_option::oobinline, enable); }

    void
    socket::set_rcvbuf(unsigned size)
    { setsockopt(socket_option::rcvbuf, size); }

    void
    socket::set_reuseaddr(bool enable)
    { setsockopt(socket_option::reuseaddr, enable); }

    void
    socket::set_sndbuf(unsigned size)
    { setsockopt(socket_option::sndbuf, size); }

#ifdef SO_RUSRBUF
    void
    socket::set_rusrbuf(bool enable)
    { setsockopt(socket_option::rusrbuf, enable); }
#endif

#ifdef SO_TCPSACK
    void
    socket::set_tcpsack(bool enable)
    { setsockopt(socket_option::tcpsack, enable); }
#endif

#ifdef SO_WINSCALE
    void
    socket::set_winscale(bool enable)
    { setsockopt(socket_option::winscale, enable); }
#endif

    // TCP

#ifdef TCP_ACKDELAYTIME
    void
    socket::set_ackdelaytime(unsigned ms)
    { setsockopt(tcp_option::ackdelaytime, ms); }
#endif

#ifdef TCP_ACKFREQUENCY
    void
    socket::set_ackfrequency(unsigned pending)
    { setsockopt(tcp_option::ackfrequency, pending); }
#endif

    void
    socket::set_maxseg(unsigned size)
    { setsockopt(tcp_option::maxseg, size); }

#ifdef TCP_NOACKDELAY
    void
    socket::set_noackdelay()
    { setsockopt(tcp_option::noackdelay, 0); }
#endif

    void
    socket::set_nodelay(bool enable)
    { setsockopt(tcp_option::nodelay, enable); }


    std::expected<socket::poll_flags, error>
    socket::try_poll(poll_flags flags,
                     std::chrono::milliseconds timeout)
        const noexcept
    {
        pollfd pf {
            .fd = fd,
            .events = static_cast<short>(flags),
            .revents = 0
        };
        int status = ::poll(&pf, 1, timeout.count());
        if (status == -1)
            return std::unexpected{error{errno}};
        return static_cast<poll_flags>(pf.revents);
    }


    std::expected<bool, error>
    socket::try_is_readable(std::chrono::milliseconds timeout)
        const noexcept
    {
        auto status = try_poll(poll_flags::in, timeout);
        if (!status)
            return std::unexpected{status.error()};
        return (*status & poll_flags::in) != poll_flags::none;
    }


    std::expected<bool, error>
    socket::try_is_writable(std::chrono::milliseconds timeout)
        const noexcept
    {
        auto status = try_poll(poll_flags::out, timeout);
        if (!status)
            return std::unexpected{status.error()};
        return (*status & poll_flags::out) != poll_flags::none;
    }


    std::expected<std::size_t, error>
    socket::try_recv(void* buf, std::size_t len,
                     msg_flags flags)
        noexcept
    {
        auto status = ::recv(fd, buf, len, static_cast<int>(flags));
        if (status == -1)
            return std::unexpected{error{errno}};
        return status;
    }


    std::expected<std::pair<std::size_t, address>,
                  error>
    socket::try_recvfrom(void* buf, std::size_t len,
                         msg_flags flags)
        noexcept
    {
        address src;
        socklen_t src_size = sizeof src.storage;
        auto status = ::recvfrom(fd,
                                 buf, len,
                                 static_cast<int>(flags),
                                 src.data(),
                                 &src_size);
        if (status == -1)
            return std::unexpected{error{errno}};
        return std::pair(status, std::move(src));
    }


    std::expected<std::size_t, error>
    socket::try_send(const void* buf, std::size_t len,
                     msg_flags flags)
        noexcept
    {
        auto status = ::send(fd, buf, len, static_cast<int>(flags));
        if (status == -1)
            return std::unexpected{error{errno}};
        return status;
    }


    std::expected<std::size_t, error>
    socket::try_sendto(const void* buf, std::size_t len,
                       const address& dst,
                       msg_flags flags)
        noexcept
    {
        auto status = ::sendto(fd,
                               buf, len,
                               static_cast<int>(flags),
                               dst.data(), dst.size());
        if (status == -1)
            return std::unexpected{error{errno}};
        return status;
    }

} // namespace net
