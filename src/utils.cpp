/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <array>
#include <stdexcept>

#include "utils.hpp"


using namespace std::literals;


namespace utils {

    std::optional<std::filesystem::path>
    find_custom_ca_info()
    {
#ifdef __WIIU__
        static const std::array candidates{
            "/vol/external01/wiiu/certs/cacert.pem"s,
            "/vol/external01/certs/cacert.pem"s,
            "/vol/content/cacert.pem"s,
        };

        for (auto p : candidates)
            if (std::filesystem::exists(p))
                return p;
#endif

        return {};
    }


    void
    remove_prefix(std::span<const std::byte>& input,
                  std::size_t n)
    {
        input = input.subspan(n);
    }


    bool
    starts_with(std::span<const std::byte> data,
                std::span<const std::byte> prefix)
    {
        return std::ranges::starts_with(data, prefix);
    }



    stop_requested::stop_requested() :
        std::runtime_error{"stop requested"}
    {}


    void
    throw_if_stopped(std::stop_token& stoken)
    {
        if (stoken.stop_requested())
            throw stop_requested{};
    }

} // namespace utils
