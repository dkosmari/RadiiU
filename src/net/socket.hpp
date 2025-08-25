/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NET_SOCKET_HPP
#define NET_SOCKET_HPP

#include <chrono>
#include <cstdint>
#include <expected>
#include <utility>              // pair<>

#include <netinet/in.h>         // IP_*
#include <netinet/tcp.h>        // TCP_*
#include <poll.h>
#include <sys/socket.h>         // SO_*, MSG_*

#include "address.hpp"
#include "error.hpp"


#ifdef __WIIU__

#ifndef SO_KEEPCNT
#define SO_KEEPCNT 0x101B
#endif

#ifndef SO_KEEPIDLE
#define SO_KEEPIDLE 0x1019
#endif

#ifndef SO_KEEPINTVL
#define SO_KEEPINTVL 0x101A
#endif

#ifndef TCP_ACKFREQUENCY
#define TCP_ACKFREQUENCY 0x2005
#endif

#endif // __WIIU__


namespace net {

    class socket {

        int fd = -1;

    public:

        enum class ip_option : int {
            tos = IP_TOS,
            ttl = IP_TTL,
        };


        enum class msg_flags : int {
            dontroute = MSG_DONTROUTE,
            dontwait  = MSG_DONTWAIT,
            none      = 0,
            oob       = MSG_OOB,
            peek      = MSG_PEEK,
        };


        enum class poll_flags : short {
            err  = POLLERR,
            hup  = POLLHUP,
            in   = POLLIN,
            none = 0,
            nval = POLLNVAL,
            out  = POLLOUT,
            pri  = POLLPRI,
        };


        enum class socket_option : int {
#ifdef SO_BIO
            bio         = SO_BIO,
#endif
            broadcast   = SO_BROADCAST,
            dontroute   = SO_DONTROUTE,
            error       = SO_ERROR,
#ifdef SO_HOPCNT
            hopcnt      = SO_HOPCNT,
#endif
            keepalive   = SO_KEEPALIVE,
#ifdef SO_KEEPCNT
            keepcnt     = SO_KEEPCNT,
#endif
#ifdef SO_KEEPIDLE
            keepidle    = SO_KEEPIDLE,
#endif
#ifdef SO_KEEPINTVL
            keepintvl   = SO_KEEPINTVL,
#endif
            linger      = SO_LINGER,
#ifdef SO_MAXMSG
            maxmsg      = SO_MAXMSG,
#endif
#ifdef SO_MYADDR
            myaddr      = SO_MYADDR,
#endif
#ifdef SO_NBIO
            nbio        = SO_NBIO,
#endif
#ifdef SO_NONBLOCK
            nonblock    = SO_NONBLOCK,
#endif
#ifdef SO_NOSLOWSTART
            noslowstart = SO_NOSLOWSTART,
#endif
            oobinline   = SO_OOBINLINE,
            rcvbuf      = SO_RCVBUF,
            rcvlowat    = SO_RCVLOWAT,
            reuseaddr   = SO_REUSEADDR,
#ifdef SO_RUSRBUF
            rusrbuf     = SO_RUSRBUF,
#endif
#ifdef SO_RXDATA
            rxdata      = SO_RXDATA,
#endif
            sndbuf      = SO_SNDBUF,
            sndlowat    = SO_SNDLOWAT,
#ifdef SO_TCPSACK
            tcpsack     = SO_TCPSACK,
#endif
#ifdef SO_TXDATA
            txdata      = SO_TXDATA,
#endif
            type        = SO_TYPE,
#ifdef SO_WINSCALE
            winscale    = SO_WINSCALE,
#endif
        };


        enum class tcp_option : int {
#ifdef TCP_ACKDELAYTIME
            ackdelaytime = TCP_ACKDELAYTIME,
#endif
#ifdef TCP_ACKFREQUENCY
            ackfrequency = TCP_ACKFREQUENCY,
#endif
            maxseg       = TCP_MAXSEG,
#ifdef TCP_NOACKDELAY
            noackdelay   = TCP_NOACKDELAY,
#endif
            nodelay      = TCP_NODELAY,
        };


        enum class type {
            tcp,
            udp,
        };


        constexpr
        socket() noexcept = default;

        explicit
        socket(int fd);

        socket(int family, type t);

        // move constructor
        socket(socket&& other) noexcept;

        // move assignment
        socket& operator =(socket&& other) noexcept;

        ~socket();


        // convenience named constructors

        static
        socket make_tcp(int family = AF_INET);

        static
        socket make_udp(int family = AF_INET);


        // check if socket is valid
        explicit
        operator bool() const noexcept;

        bool is_socket() const noexcept;


        // The regular BSD sockets API, throw net::error

        std::pair<socket, address> accept();

        void bind(const address& addr);

        void close();

        void connect(ipv4_t ip, port_t port);
        void connect(const address& addr);


        std::expected<std::uint8_t, error>
        getsockopt(ip_option opt) const noexcept;

        template<typename T>
        std::expected<T, error>
        getsockopt(socket_option opt) const noexcept;

        std::expected<unsigned, error>
        getsockopt(tcp_option opt) const noexcept;

        // convenience getters

        // getters for ip_option
        std::expected<std::uint8_t, error> get_tos() const noexcept;
        std::expected<std::uint8_t, error> get_ttl() const noexcept;

        // getters for socket_option
        std::expected<bool,     error> get_broadcast() const noexcept;
        std::expected<bool,     error> get_dontroute() const noexcept;
        std::expected<error,    error> get_error()     const noexcept;
#ifdef SO_HOPCNT
        std::expected<unsigned, error> get_hopcnt()    const noexcept;
#endif
        std::expected<bool,     error> get_keepalive() const noexcept;
#ifdef SO_KEEPCNT
        std::expected<unsigned, error> get_keepcnt()   const noexcept;
#endif
#ifdef SO_KEEPIDLE
        std::expected<unsigned, error> get_keepidle()  const noexcept;
#endif
#ifdef SO_KEEPINTVL
        std::expected<unsigned, error> get_keepintvl() const noexcept;
#endif
        std::expected<::linger, error> get_linger()    const noexcept;
#ifdef SO_MAXMSG
        std::expected<unsigned, error> get_maxmsg()    const noexcept;
#endif
#ifdef SO_MYADDR
        std::expected<address,  error> get_myaddr()    const noexcept;
#endif
        std::expected<bool,     error> get_nonblock()  const;
        std::expected<bool,     error> get_oobinline() const noexcept;
        std::expected<unsigned, error> get_rcvbuf()    const noexcept;
        std::expected<unsigned, error> get_rcvlowat()  const noexcept;
        std::expected<bool,     error> get_reuseaddr() const noexcept;
#ifdef SO_RUSRBUF
        std::expected<bool,     error> get_rusrbuf()   const noexcept;
#endif
#ifdef SO_RXDATA
        std::expected<unsigned, error> get_rxdata()    const noexcept;
#endif
        std::expected<unsigned, error> get_sndbuf()    const noexcept;
        std::expected<unsigned, error> get_sndlowat()  const noexcept;
#ifdef SO_TCPSACK
        std::expected<bool,     error> get_tcpsack()   const noexcept;
#endif
#ifdef SO_TXDATA
        std::expected<unsigned, error> get_txdata()    const noexcept;
#endif
        std::expected<type,     error> get_type()      const noexcept;
#ifdef SO_WINSCALE
        std::expected<bool,     error> get_winscale()  const noexcept;
#endif

        // getters for tcp_option
#ifdef TCP_ACKDELAYTIME
        std::expected<std::chrono::milliseconds, error> get_ackdelaytime() const noexcept;
#endif
#ifdef TCP_ACKFREQUENCY
        std::expected<unsigned, error> get_ackfrequency() const noexcept;
#endif
        std::expected<unsigned, error> get_maxseg() const noexcept;
#ifdef TCP_NOACKDELAY
        std::expected<bool, error> get_noackdelay() const noexcept;
#endif
        std::expected<bool, error> get_nodelay() const noexcept;


        address getpeername() const;
        address get_remote_address() const;

        address getsockname() const;
        address get_local_address() const;

        void listen(int backlog);


        poll_flags poll(poll_flags flags, std::chrono::milliseconds timeout = {}) const;
        // convenience wrappers for poll()
        bool is_readable(std::chrono::milliseconds timeout = {}) const;
        bool is_writable(std::chrono::milliseconds timeout = {}) const;


        std::size_t
        recv(void* buf, std::size_t len,
             msg_flags flags = msg_flags::none);

        std::size_t
        recv_all(void* buf, std::size_t total,
                 msg_flags flags = msg_flags::none);

        std::pair<std::size_t, address>
        recvfrom(void* buf, std::size_t len,
                 msg_flags flags = msg_flags::none);


        // Disassociate the handle from this socket.
        int release() noexcept;


        std::size_t
        send(const void* buf, std::size_t len,
             msg_flags flags = msg_flags::none);

        std::size_t
        send_all(const void* buf, std::size_t total,
                 msg_flags flags = msg_flags::none);

        std::size_t
        sendto(const void* buf, std::size_t len,
               address dst,
               msg_flags flags = msg_flags::none);


        void setsockopt(ip_option opt, std::uint8_t arg);

        void setsockopt(socket_option opt);
        void setsockopt(socket_option opt, unsigned arg);
        void setsockopt(socket_option opt, const struct ::linger& arg);

        void setsockopt(tcp_option opt, unsigned arg);


        // convenience setters

        // setters for ip_option
        void set_tos(std::uint8_t t);
        void set_ttl(std::uint8_t t);

        // setters for socket_option
#ifdef SO_BIO
        void set_bio();
#endif
        void set_broadcast(bool enable);
        void set_dontroute(bool enable);
        void set_keepalive(bool enable);
#ifdef SO_KEEPCNT
        void set_keepcnt(unsigned count);
#endif
#ifdef SO_KEEPIDLE
        void set_keepidle(unsigned period);
#endif
#ifdef SO_KEEPINTVL
        void set_keepintvl(unsigned interval);
#endif
        void set_linger(bool enable, int period = 0);
#ifdef SO_MAXMSG
        void set_maxmsg(unsigned size);
#endif
#ifdef SO_NBIO
        void set_nbio();
#endif
        void set_nonblock(bool enable);
#ifdef SO_NOSLOWSTART
        void set_noslowstart(bool enable);
#endif
        void set_oobinline(bool enable);
        void set_rcvbuf(unsigned size);
        void set_reuseaddr(bool enable);
#ifdef SO_RUSRBUF
        void set_rusrbuf(bool enable);
#endif
        void set_sndbuf(unsigned size);
#ifdef SO_TCPSACK
        void set_tcpsack(bool enable);
#endif
#ifdef SO_WINSCALE
        void set_winscale(bool enable);
#endif

        // setters for tcp_option
#ifdef TCP_ACKDELAYTIME
        void set_ackdelaytime(unsigned ms);
#endif
#ifdef TCP_ACKFREQUENCY
        void set_ackfrequency(unsigned pending);
#endif
        void set_maxseg(unsigned size);
#ifdef TCP_NOACKDELAY
        void set_noackdelay();
#endif
        void set_nodelay(bool enable);


        std::expected<poll_flags, error>
        try_poll(poll_flags flags, std::chrono::milliseconds timeout = {})
            const noexcept;

        std::expected<bool, error>
        try_is_readable(std::chrono::milliseconds timeout = {})
            const noexcept;

        std::expected<bool, error>
        try_is_writable(std::chrono::milliseconds timeout = {})
            const noexcept;


        std::expected<std::size_t, error>
        try_recv(void* buf, std::size_t len,
                 msg_flags flags = msg_flags::none)
            noexcept;

        std::expected<std::pair<std::size_t, address>,
                      error>
        try_recvfrom(void* buf, std::size_t len,
                     msg_flags flags = msg_flags::none)
            noexcept;


        std::expected<std::size_t, error>
        try_send(const void* buf, std::size_t len,
                 msg_flags flags = msg_flags::none)
            noexcept;

        std::expected<std::size_t, error>
        try_sendto(const void* buf, std::size_t len,
                   const address& dst,
                   msg_flags flags = msg_flags::none)
            noexcept;
    };


    socket::msg_flags operator &(socket::msg_flags a, socket::msg_flags b) noexcept;
    socket::msg_flags operator ^(socket::msg_flags a, socket::msg_flags b) noexcept;
    socket::msg_flags operator |(socket::msg_flags a, socket::msg_flags b) noexcept;
    socket::msg_flags operator ~(socket::msg_flags a) noexcept;


    socket::poll_flags operator &(socket::poll_flags a, socket::poll_flags b) noexcept;
    socket::poll_flags operator ^(socket::poll_flags a, socket::poll_flags b) noexcept;
    socket::poll_flags operator |(socket::poll_flags a, socket::poll_flags b) noexcept;
    socket::poll_flags operator ~(socket::poll_flags a) noexcept;

} // namespace net


#endif
