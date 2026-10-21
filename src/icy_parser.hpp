/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ICY_PARSER_HPP
#define ICY_PARSER_HPP

#include <cstddef>
#include <flat_map>
#include <span>
#include <string_view>

#include <curlxx/url.hpp>

#include "byte_stream.hpp"
#include "stream_metadata.hpp"


namespace icy {

    class parser {

    public:

        using dict_t = std::unordered_map<std::string_view, std::string_view>;


        parser();


        // NOTE: pass-by-value is intentional
        void
        reset(std::string base_url_,
              std::size_t new_interval = 0);


        void
        feed(std::span<const std::byte> input);


        [[nodiscard]]
        bool
        get_metadata(stream_metadata& dst)
            noexcept;


        std::span<std::byte>
        get_data(std::span<std::byte> dst);


    private:


        std::string base_url;

        byte_stream meta_stream;
        byte_stream data_stream;

        std::size_t interval = 0;
        std::size_t data_left = 0;
        std::size_t meta_left = 0;

        std::optional<stream_metadata> meta;


        void
        handle(const dict_t& meta_dict,
               std::string_view key,
               std::string_view val);

        void
        parse_metadata();

        std::string
        resolve_url(std::string_view url)
            noexcept;

    }; // class parser

} // namespace icy

#endif
