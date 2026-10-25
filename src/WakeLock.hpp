/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WAKE_LOCK_HPP
#define WAKE_LOCK_HPP

#include <string>

#include <SDL_platform.h>

#ifdef __LINUX__
#include <systemd/sd-bus.h>
#endif


class WakeLock {

public:

    WakeLock(const std::string& who,
             const std::string& why);

    ~WakeLock()
        noexcept;

    // Prevent moving

    WakeLock(WakeLock&&) = delete;

private:

#ifdef __LINUX__
    sd_bus* bus = nullptr;
    int fd = -1;
#endif

}; // class WakeLock

#endif
