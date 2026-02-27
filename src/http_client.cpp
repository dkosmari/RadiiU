/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <functional>
#include <iostream>
#include <vector>

#include "http_client.hpp"

#include "utils.hpp"


using std::cout;
using std::endl;

using namespace std::literals;
using namespace std::placeholders;


http_client::http_client(const std::string& url)
{
    easy.set_verbose(false);
    easy.set_user_agent(utils::get_user_agent());
    easy.set_url(url);
    easy.set_forbid_reuse(true);
    easy.set_follow_location(true);
    easy.set_ssl_verify_peer(false);

    // easy.set_http_headers({
    //         "Accept: "s + utils::join(mimes, ",")
    //     });

    easy.set_write_function(std::bind(&http_client::curl_write_callback, this, _1));

    multi.set_max_total_connections(1);
    multi.add(easy);
}


http_client::~http_client()
    noexcept
{
    multi.remove(easy);
}


// std::optional<stream_metadata>
// http_client::get_metadata()
//     const
// {
//     std::optional<stream_metadata> result;
//     if (metadata)
//         result = metadata;
//     if (dec) {
//         if (auto decoder_metadata = dec->get_metadata()) {
//             if (result)
//                 result->merge(*decoder_metadata);
//             else
//                 result = *decoder_metadata;
//         }
//     }
//     return result;
// }


void
http_client::add_header(const std::string& hdr)
{
    headers.push_back(hdr);
}


void
http_client::add_accept(const std::string& mime)
{
    accepts.push_back(mime);
}


void
http_client::process()
{
    if (!requested) {
        headers.push_back("Accept: " + utils::join(accepts, ","));
        easy.set_http_headers(headers);
        requested = true;
    }

    multi.perform();
    auto done = multi.get_done();
    for (const auto& msg : done)
        if (msg.handle == &easy) {
            if (msg.result != CURLE_OK)
                throw std::runtime_error{"curl::multi::perform(): " + std::to_string(msg.result)};
            finished = true;
        }
}


std::optional<std::string>
http_client::get_header(const std::string& name)
{
    if (!responded)
        return {};

    auto result = easy.try_get_header(name);
    if (result)
        return result->value;
    return {};
}


std::size_t
http_client::curl_write_callback(std::span<const char> buf)
{
    if (buf.empty()) {
        cout << "End of connection detected." << endl;
        return 0;
    }

    responded = true;

    data_stream.write(buf);

#if 0
    if (!dec) {
        if (data_stream.size() >= 256) {
            std::vector<char> initial_data;
            try {
                // try to create a decoder
                auto hdr_content_type = easy.try_get_header("content-type");
                auto content_type = hdr_content_type ? hdr_content_type->value : ""s;
                initial_data = data_stream.read_as<char>();
                dec = decoder::create(content_type, initial_data);
                // total_bytes_fed = initial_data.size();
            }
            catch (std::exception& e) {
                cout << "Failed to create decoder: " << e.what() << endl;
                data_stream.write(std::span{initial_data});
            }
        }
    } else {
        // decoder already created
        if (!data_stream.empty()) {
            // cout << "Feeding " << data_stream.size() << " bytes to decoder" << endl;
            total_bytes_fed += dec->feed(data_stream.read_as<char>());
        }
    }
#endif

    return buf.size();
}
