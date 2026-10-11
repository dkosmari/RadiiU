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

#include "icy_stream.hpp"

#include "icy.hpp"
#include "LogManager.hpp"
#include "string_utils.hpp"
#include "tracer.hpp"


using namespace std::literals;


namespace icy {

    namespace {

#if 0
        bool
        is_image_url(const std::string& url)
        {
            try {
                curl::url parser{url};
                auto path = parser.get_path();
                auto ext_pos = path.find_last_of('.');
                if (ext_pos == std::string::npos)
                    return false;
                auto ext = std::string_view(path.begin() + ext_pos, path.end());
                using string_utils::equal_case;
                if (equal_case(ext, ".png"sv) ||
                    equal_case(ext, ".jpg"sv) ||
                    equal_case(ext, ".jpeg"sv) ||
                    equal_case(ext, ".webp"sv) ||
                    equal_case(ext, ".gif"sv))
                    return true;
                return false;
            }
            catch (...) {
                return false;
            }
        }
#endif


#if 0
        std::optional<std::string>
        if_not_empty(std::string_view input)
        {
            if (input.empty())
                return {};
            return std::string{input};
        }
#endif


        bool
        is_just_a_number(std::string_view input)
        {
            auto loc = std::locale::classic();
            for (char c : input)
                if (!std::isdigit(c, loc))
                    return false;
            return true;
        }


        void
        remove_prefix(std::span<const std::byte>& input,
                      std::size_t n)
        {
            input = input.subspan(n);
        }

    } // namespace


    stream::stream()
    {
        TRACE_FUNC;
    }


    void
    stream::reset(std::string base_url_,
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
    stream::get_metadata()
        noexcept
    {
        return std::move(current_meta);
    }


    std::span<std::byte>
    stream::get_data(std::span<std::byte> dst)
    {
        return data_stream.read(dst);
    }


    void
    stream::feed(std::span<const std::byte> input)
    {
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
    stream::parse_metadata()
    {
        using string_utils::trimmed;

        //current_meta = {};

        // Note: icy metadata is padded with null bytes.
        std::string raw_meta_str(meta_stream.size(), '\0');
        std::ignore = meta_stream.read(std::span{raw_meta_str});
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
    stream::resolve_url(std::string_view url)
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
