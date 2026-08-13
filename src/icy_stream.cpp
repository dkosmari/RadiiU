/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

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

        std::optional<std::string>
        if_not_empty(std::string input)
        {
            if (input.empty())
                return {};
            return {std::move(input)};
        }

    } // namespace


    stream::stream(http_client& hc) :
        http(hc),
        base_url{http.get_effective_url()}
    {
        TRACE_FUNC;

        using string_utils::trimmed;

        unsigned icy_num = 0;

        if (auto hdr = http.get_header("icy-metaint")) {
            LOG_INFO("Got icy-metaint={} in header.", *hdr);
            data_left = interval = std::stoull(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-name")) {
            initial_meta.station_name = trimmed(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-url")) {
            initial_meta.station_url = trimmed(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-genre")) {
            initial_meta.station_genre = trimmed(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-description")) {
            initial_meta.station_description = trimmed(*hdr);
            ++icy_num;
        }

        if (auto hdr = http.get_header("icy-br"))
            ++icy_num;

        if (auto hdr = http.get_header("ice-audio-info"))
            ++icy_num;

        if (auto hdr = http.get_header("icy-pub"))
            ++icy_num;

        // TODO: process icy-noticeN headers.

        if (!icy_num)
            throw std::runtime_error{"not an icecast stream"};

        current_meta = initial_meta;
    }


    const stream_metadata&
    stream::get_metadata()
        const noexcept
    {
        return current_meta;
    }


    void
    stream::process()
    {
        if (!interval) {
            data_stream.consume(http.data_stream);
            return;
        }

        // try to convert the whole raw_stream into either data_stream or meta_stream
        while (!http.data_stream.empty()) {

            if (data_left)
                data_left -= data_stream.consume(http.data_stream, data_left);
            else {
                // no more data, start reading metadata

                if (meta_left == 0) {
                    // when both data_left and meta_left are zero, we're waiting for the
                    // meta size prefix
                    auto c = http.data_stream.try_load_u8();
                    if (!c) // if not enough raw data to parse size of meta data, give up
                        return;
                    meta_left = *c * 16u;
                    if (meta_left == 0) { // no metadata for now
                        data_left = interval;
                        continue;
                    }
                }

                meta_left -= meta_stream.consume(http.data_stream, meta_left);
                if (meta_left == 0) {
                    // finished reading this chunk of metadata
                    data_left = interval;
                    try {
                        process_metadata();
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
    stream::process_metadata()
    {
        using string_utils::trimmed;

        current_meta = initial_meta;

        // Note: icy metadata is padded with null bytes.
        std::string meta_str = trimmed(meta_stream.read_str(), '\0');

        auto parsed_meta = icy::parse(meta_str);

        auto& meta = current_meta;

        // TODO: should probably do case-insensitive key comparisons

        for (const auto& [key, val_] : parsed_meta) {
            auto val = trimmed(val_);
            // LOG_DEBUG("icy meta key: {} = {:?}", key, val);
            if (key == "StreamTitle"s) {
                meta.title = if_not_empty(std::move(val));
            } else if (key == "StreamUrl"s) {
                auto url = resolve_url(val);
                if (!parsed_meta.contains("StreamArtwork"s))
                    meta.cover_art = if_not_empty(std::move(url));
                else
                    meta.station_url = if_not_empty(std::move(url));
            } else if (key == "StreamArtwork"s) {
                meta.cover_art = if_not_empty(resolve_url(val));
            } else if (key == "StreamArtist"s) {
                meta.artist = if_not_empty(std::move(val));
            } else if (key == "StreamAlbum"s) {
                meta.album = if_not_empty(std::move(val));
            } else if (key == "StreamGenre"s) {
                meta.genre = if_not_empty(std::move(val));
            } else if (key == "StreamYear"s) {
                meta.date = if_not_empty(std::move(val));
            } else if (key == "StreamDate"s) {
                meta.date = if_not_empty(std::move(val));
            } else
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
                current_meta.extra[key] = std::move(val);
        }

    }


    std::string
    stream::resolve_url(const std::string& url)
        noexcept
    {
        try {
            if (url.empty())
                return {};
            curl::url resolver{base_url};
            resolver.set_url(url);
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
