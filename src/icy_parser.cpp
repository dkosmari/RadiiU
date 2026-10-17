/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cstdint>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <SDL_stdinc.h>

#include <curlxx/url.hpp>

#include "icy_parser.hpp"

#include "LogManager.hpp"
#include "string_utils.hpp"
#include "utils.hpp"


using namespace std::literals;


namespace icy {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        using dict_t = std::unordered_map<std::string_view, std::string_view>;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        bool
        is_just_a_number(std::string_view input);

        dict_t
        parse(std::string_view input);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        bool
        is_just_a_number(std::string_view input)
        {
            auto loc = std::locale::classic();
            for (char c : input)
                if (!std::isdigit(c, loc))
                    return false;
            return true;
        }


        dict_t
        parse(std::string_view input)
        {
            dict_t result;

            const auto npos = std::string_view::npos;
            using size_type = std::string_view::size_type;

            while (!input.empty()) {

                // Look for KeyName=
                size_type key_end_pos = input.find('=');
                if (key_end_pos == npos)
                    break;

                std::string_view key = input.substr(0, key_end_pos);
                if (key.empty())
                    // Input error: empty key
                    break;

                // Discard all the way up to the '='
                input.remove_prefix(key_end_pos + 1);
                if (input.empty())
                    // Input error: empty value
                    break;

                char quote_char = input.front();
                if (quote_char != '\'' && quote_char != '"')
                    break;

                input.remove_prefix(1);
                if (input.empty())
                    // Input error: truncated inside quotes
                    break;

                size_type remove_size = 2;
                size_type close_quote_pos = input.find(quote_char + ";"s);
                if (close_quote_pos == npos) {
                    // When on the end of input, the last quote doesn't have a following ';'
                    close_quote_pos = input.size() - 1;
                    remove_size = 1;
                    if (input[close_quote_pos] != quote_char)
                        // Input error: input does not end in the closing quote char
                        break;
                }

                if (close_quote_pos == npos)
                    // Input error: unterminated quote
                    break;

                std::string_view value = input.substr(0, close_quote_pos);

                result.emplace(key, value);

                input.remove_prefix(close_quote_pos + remove_size);

            }

            return result;
        }

    } // namespace


    parser::parser()
    {}


    void
    parser::reset(std::string base_url_,
                  std::size_t new_interval)
    {
        base_url = std::move(base_url_);

        meta_stream.clear();
        data_stream.clear();

        interval = new_interval;

        data_left = interval;
        meta_left = 0;

        current_meta = {};
    }


    stream_metadata&&
    parser::get_metadata()
        noexcept
    {
        return std::move(current_meta);
    }


    std::span<std::byte>
    parser::get_data(std::span<std::byte> dst)
    {
        return data_stream.read(dst);
    }


    void
    parser::feed(std::span<const std::byte> input)
    {
        using utils::remove_prefix;

        if (!interval) {
            data_stream.write(input);
            return;
        }

        while (!input.empty()) {

            if (data_left) {
                std::size_t n = std::min(data_left, input.size());
                auto written = data_stream.write(input.first(n));
                remove_prefix(input, written.size());
                data_left -= written.size();
            } else {
                if (meta_left == 0) {
                    // When both data_left and meta_left are zero, we're waiting for the meta
                    // size prefix.
                    auto hdr = static_cast<std::uint8_t>(input.front());
                    remove_prefix(input, 1);
                    meta_left = hdr * 16u;
                    // LOG_DEBUG("metadata size: {}", meta_left);
                    if (meta_left == 0) { // no metadata
                        data_left = interval; // go back to reading data
                        continue;
                    }
                }

                std::size_t n = std::min(meta_left, input.size());
                auto written = meta_stream.write(input.first(n));
                remove_prefix(input, written.size());
                meta_left -= written.size();
                if (meta_left == 0) {
                    // Finished reading this chunk of metadata.
                    data_left = interval; // go back to reading data
                    try {
                        parse_metadata();
                    }
                    catch (std::exception& e) {
                        LOG_ERROR("Metadata error: {}", e.what());
                        throw;
                    }
                }
            }

        }
    }


    void
    parser::parse_metadata()
    {
        using string_utils::trimmed;

        //current_meta = {};

        // Note: icy metadata is padded with null bytes.
        std::string raw_meta_str(meta_stream.size(), '\0');
        std::ignore = meta_stream.read(as_writable_bytes(std::span{raw_meta_str}));
        std::string meta_str = trimmed(raw_meta_str, '\0');

        auto meta_dict = icy::parse(meta_str);

        auto& meta = current_meta;

        for (const auto& [key, raw_val] : meta_dict) {
            auto val = trimmed(raw_val);
            LOG_DEBUG("icy meta key: {} = {:?}", key, val);
            if (key == "StreamTitle"sv) {
                meta.title = std::move(val);
            } else if (key == "StreamUrl"sv) {
                // NOTE: Some servers report a numeric song ID as the URL, let's ignore them.
                if (!is_just_a_number(val)) {
                    auto url = resolve_url(val);
                    if (!meta_dict.contains("StreamArtwork"sv))
                        meta.cover_art = std::move(url);
                    else
                        meta.station.url = std::move(url);
                }
            } else if (key == "StreamArtwork"sv) {
                meta.cover_art = resolve_url(val);
            } else if (key == "StreamArtist"sv) {
                meta.artist = val;
            } else if (key == "StreamAlbum"sv) {
                meta.album = val;
            } else if (key == "StreamGenre"sv) {
                meta.genre = val;
            } else if (key == "StreamYear"sv) {
                meta.date = val;
            } else if (key == "StreamDate"sv) {
                meta.date = val;
            } else {
                /*
                  Not handled:
                  StreamComposer
                  StreamPublisher
                  StreamLabel
                  StreamCover
                  StreamCoverArt
                  CoverArt
                  CoverArtUrl
                  Image
                  ImageUrl
                  AlbumArt
                */
                current_meta.extra[std::string{key}] = std::string{val};
            }
        }

    }


    std::string
    parser::resolve_url(std::string_view url)
        noexcept
    {
        try {
            if (url.empty())
                return {};
            curl::url resolver{base_url};
            resolver.set_url(std::string{url});
            std::string result = resolver.get_url();
            if (url != result)
                LOG_DEBUG("Resolved URL: {} -> {}", url, result);
            return result;
        }
        catch (std::exception& e) {
            LOG_DEBUG("Failed to resolve {:?}: {}", url, e.what());
            return {};
        }
    }

} // namespace icy



#ifdef UNIT_TEST

#include <iostream>

using std::cout;
using std::endl;

unsigned check_counter = 0;

template<typename A,
         typename B>
void
check(A&& a, B&& b)
{
    ++check_counter;
    if (a != b)
        throw runtime_error{"Check failed: \"" + a + "\" != \"" + b + "\""};
    cout << "check " << check_counter << " passed" << endl;
}


int main()
{
    {
        // trivial case
        auto d = icy::parse("StreamTitle='Testing something'");
        check(d.at("StreamTitle"), "Testing something");
    }

    {
        // again, with trailing ';'
        auto d = icy::parse("StreamTitle='Testing something';");
        check(d.at("StreamTitle"), "Testing something");
    }

    {
        // trivial case with two fields
        auto d = icy::parse("StreamTitle='Another test';StreamURL='http://example.com'");
        check(d.at("StreamTitle"), "Another test");
        check(d.at("StreamURL"), "http://example.com");
    }

    {
        // again, with trailing ';'
        auto d = icy::parse("StreamTitle='Another test';StreamURL='http://example.com';");
        check(d.at("StreamTitle"), "Another test");
        check(d.at("StreamURL"), "http://example.com");
    }

    {
        // obnoxious case: title with quote character
        auto d = icy::parse("StreamTitle='Icecast's problem'");
        check(d.at("StreamTitle"), "Icecast's problem");
    }

    {
        // again, with trailing ';'
        auto d = icy::parse("StreamTitle='Icecast's problem';");
        check(d.at("StreamTitle"), "Icecast's problem");
    }


    {
        // fairly ambiguous title
        auto d = icy::parse("StreamTitle='Why's=no quote escaping?'");
        check(d.at("StreamTitle"), "Why's=no quote escaping?");
    }

    {
        // again, trailing ';'
        auto d = icy::parse("StreamTitle='Why's=no quote escaping?';");
        check(d.at("StreamTitle"), "Why's=no quote escaping?");
    }

}

#endif
