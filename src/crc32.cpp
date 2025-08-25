/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <array>

#include "crc32.hpp"


using std::uint32_t;
using std::size_t;

using crc32_table_t = std::array<uint32_t, 256>;


namespace {

    crc32_table_t
    make_crc32_table()
    {
        crc32_table_t table;
        for (size_t idx = 0; idx < table.size(); ++idx) {
            uint32_t c = idx;
            for (unsigned k = 0; k < 8; ++k)
                if (c & 1)
                    c = 0xedb88320L ^ (c >> 1);
                else
                    c >>= 1;
            table[idx] = c;
        }
        return table;
    }

} // namespace


uint32_t
calc_crc32(std::span<const std::byte> data,
           uint32_t crc32)
{
    static const crc32_table_t table = make_crc32_table();

    crc32 = ~crc32;

    for (auto b : data)
        crc32 = table[(crc32 ^ std::to_integer<std::uint8_t>(b)) & 0xff]
                ^ (crc32 >> 8);

    return ~crc32;
}
