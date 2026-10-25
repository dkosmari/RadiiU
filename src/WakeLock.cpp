/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstring>
#include <stdexcept>
#include <utility>

#include <SDL_platform.h>

#ifdef __LINUX__
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef __WIIU__
#include <coreinit/energysaver.h>
#endif

#include "WakeLock.hpp"

#include "LogManager.hpp"


WakeLock::WakeLock(const std::string& who,
                   const std::string& why)
{
#ifdef __LINUX__

    int e;

    e = sd_bus_open_system(&bus);
    if (e < 0) {
        LOG_ERROR("Failed to connect to system bus: {}", std::strerror(-e));
        throw std::runtime_error{"Failed to connect to system bus"};
    }

    sd_bus_message* reply = nullptr;
    e = sd_bus_call_method(
        bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        "Inhibit",
        nullptr,
        &reply,
        "ssss",
        "sleep",
        who.data(),
        why.data(),
        "block"
    );

    if (e < 0) {
        std::string msg = std::strerror(-e);
        if (bus) {
            sd_bus_unref(bus);
            bus = nullptr;
        }
        LOG_ERROR("Failed to acquire systemd sleep inhibitor lock: {}", msg);
        throw std::runtime_error{"Failed to acquire systemd sleep inhibitor lock: " + msg};
    }

    int raw_fd = -1;
    e = sd_bus_message_read(reply, "h", &raw_fd);
    if (e < 0) {
        std::string msg = std::strerror(-e);
        sd_bus_message_unref(reply);
        sd_bus_unref(bus);
        bus = nullptr;
        LOG_ERROR("Failed to parse inhibitor file descriptor from reply: {}", msg);
        throw std::runtime_error{"Failed to parse inhibitor file descriptor: " + msg};
    }

    fd = ::fcntl(raw_fd, F_DUPFD_CLOEXEC, 3);

    sd_bus_message_unref(reply);

    LOG_INFO("Acquired systemd sleep inhibitor lock (fd: {}).", fd);

#elifdef __WIIU__

    std::ignore = who;
    std::ignore = why;
    IMDisableAPD();

#endif
}


WakeLock::~WakeLock()
    noexcept
{
#ifdef __LINUX__
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
        LOG_DEBUG("Closed systemd inhibitor file.");
    }
    if (bus) {
        sd_bus_unref(bus);
        bus = nullptr;
        LOG_DEBUG("Dropped system bus connection.");
    }
#elifdef __WIIU__
    IMEnableAPD();
#endif
}
