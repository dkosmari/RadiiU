/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CRC32_HPP
#define CRC32_HPP

#include <cstdint>
#include <cstddef>
#include <span>


std::uint32_t
calc_crc32(std::span<const std::byte> data,
           std::uint32_t crc32 = 0);

#endif
