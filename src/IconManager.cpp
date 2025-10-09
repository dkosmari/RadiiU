/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <queue>
#include <span>
#include <string>
#include <thread>
#include <vector>
#include <ranges>

#include <curlxx/curl.hpp>
#include <sdl2xx/img.hpp>

#include "IconManager.hpp"

#include "async_queue.hpp"
#include "crc32.hpp"
#include "thread_safe.hpp"
#include "utils.hpp"


using std::cout;
using std::endl;

using namespace std::literals;


namespace IconManager {

    std::string user_agent;

    std::filesystem::path content_prefix;

    const std::size_t max_cache_size = 256;

    std::uint64_t use_counter = 0;

    sdl::renderer* renderer = nullptr;
    sdl::texture error_icon;
    sdl::texture loading_icon;

    std::optional<curl::multi> multi;

    enum class LoadState {
        unloaded,
        requested,
        loading,
        loaded,
        error,
    };

    struct IconInfo {
        std::atomic<LoadState> state{LoadState::unloaded};
        std::uint64_t last_use = 0;
        sdl::surface img;
        sdl::texture tex;
        std::optional<curl::easy> easy;
        std::optional<std::vector<char>> raw_buf;
    };


    // TODO: use a good hash map here
    using cache_t = std::map<std::string, IconInfo>;
    thread_safe<cache_t> safe_cache;


    // std::queue<std::string> load_queue;
    async_queue<std::string> requests_queue;


    std::jthread worker_thread;


    void
    worker_func(std::stop_token token);


    void
    initialize(sdl::renderer& rend)
    {
        user_agent = utils::get_user_agent();
        content_prefix = utils::get_content_path();

        renderer = &rend;

        error_icon   = sdl::img::load_texture(*renderer,
                                              content_prefix / "ui/error-icon.png");
        loading_icon = sdl::img::load_texture(*renderer,
                                              content_prefix / "ui/loading-icon.png");

        requests_queue.reset();
        worker_thread = std::jthread{worker_func};

        assert((std::atomic<LoadState>{}.is_lock_free()));
    }


    void
    finalize()
    {
        requests_queue.stop();
        worker_thread = {};

        {
            auto cache = safe_cache.lock();
            cache->clear();
        }

        loading_icon.destroy();
        error_icon.destroy();
    }


    const sdl::texture*
    get(const std::string& location)
    {
        ++use_counter;
        auto cache = safe_cache.lock();
        try {
            auto it = cache->find(location);
            if (it != cache->end()) {
                auto& status = it->second;
                status.last_use = use_counter;

                switch (status.state) {

                    case LoadState::loaded:
                        if (!status.tex) {
                            status.tex.create(*renderer, status.img);
                            status.img.destroy();
                        }
                        return &status.tex;

                    case LoadState::error:
                        return &error_icon;

                    case LoadState::requested:
                    case LoadState::loading:
                        return &loading_icon;

                    case LoadState::unloaded:
                        status.state = LoadState::loading;
                        requests_queue.push(location);
                        return &loading_icon;

                }

                return &error_icon;
            } else {
                (*cache)[location].state = LoadState::requested;
                requests_queue.push(location);
                return &loading_icon;
            }
        }
        catch (std::exception& /*e*/) {
            // cout << "error getting icon texture: " << e.what() << "\n";
            return &error_icon;
        }
    }


    IconInfo*
    find(const curl::easy* ez,
         thread_safe<cache_t>::guard<cache_t>& cache)
    {
        // TODO: use an auxiliary data structure to speed this up
        for (auto& [location, entry] : *cache)
            if (entry.easy && &*entry.easy == ez)
                return &entry;
        return nullptr;
    }


    void
    process_one_request(const std::string& location)
    {
        // cout << "loading location " << location << endl;
        std::map<std::string, IconInfo>::iterator it;

        {
            auto cache = safe_cache.lock();
            it = cache->find(location);
            if (it == cache->end())
                return;
            LoadState expected = LoadState::requested;
            if (!it->second.state.compare_exchange_strong(expected, LoadState::loading)) {
                cout << "unexpected state: got " << static_cast<int>(expected) << endl;
                return;
            }
        }

        auto& entry = it->second;

        try {

            if (location.starts_with("http://") || location.starts_with("https://")) {
                // URL
                auto& ez = entry.easy.emplace();
                ez.set_verbose(false);
                ez.set_ssl_verify_peer(false);
                if (!user_agent.empty())
                    ez.set_user_agent(user_agent);
                ez.set_url(location);
                ez.set_follow(true);
                ez.set_write_function([&entry](std::span<const char> buf) -> std::size_t
                {
                    if (!entry.raw_buf)
                        entry.raw_buf.emplace();
#ifdef __cpp_lib_containers_ranges
                    entry.raw_buf->append_range(buf);
#else
                    entry.raw_buf->insert(entry.raw_buf->end(), buf.begin(), buf.end());
#endif
                    return buf.size();
                });
                multi->add(ez);
            } else if (location.starts_with("ui/")) {
                // local path
                auto img = sdl::img::try_load(content_prefix / location);
                if (img) {
                    entry.img = std::move(*img);
                    assert(entry.img);
                    entry.state = LoadState::loaded;
                } else {
                    cout << "error processing icon load for \"" + location + "\": "
                         << img.error().what()
                         << endl;
                    entry.state = LoadState::error;
                    return;
                }
            } else {
                throw std::runtime_error{"invalid location: \"" + location + "\""};
            }

        }
        catch (std::exception& e) {
            cout << "Error processing request " << location << ": " << e.what() << endl;
            entry.state = LoadState::error;
        }
    }


    void
    trim_cache()
    {
        auto cache = safe_cache.lock();
        if (cache->size() > max_cache_size) {
            std::size_t excess = cache->size() - max_cache_size;
            std::map<std::uint64_t, std::string> to_remove;
            for (auto& [location, info] : *cache)
                to_remove.emplace(info.last_use, location);

            for (auto& [last_use, location] : to_remove | std::views::take(excess)) {
                auto& info = cache->at(location);
                if (info.easy) {
                    cout << "Pruning active request" << endl;
                    multi->remove(*info.easy);
                }
                cache->erase(location);
            }
        }
    }


    void
    handle_finished_downloads()
    {
        for (auto [ez, error_code] : multi->get_done()) {
            auto cache = safe_cache.lock();
            auto* entry = find(ez, cache);
            if (!entry) {
                cout << "ERROR: failed to find entry for " << ez << endl;
                continue;
            }

            try {
                if (!error_code) {
                    if (entry->raw_buf) {
                        sdl::rwops rw{std::span(*entry->raw_buf)};
                        entry->img = sdl::img::load(rw);
                        entry->state = LoadState::loaded;
                        rw.destroy();
                        entry->raw_buf.reset();
                    } else {
                        entry->state = LoadState::error;
                    }
                } else {
                    curl::error e{error_code};
                    cout << "error: " << e.what() << endl;
                    entry->state = LoadState::error;
                }
            }
            catch (std::exception& e) {
                cout << "error: " << e.what() << endl;
                entry->state = LoadState::error;
            }

            multi->remove(*entry->easy);
            entry->easy.reset();
            entry->raw_buf.reset();
        }
    }


    void
    worker_func(std::stop_token token)
    {
        try {
            multi.emplace();
            multi->set_max_total_connections(10);
            multi->set_max_connections(20);

            while (!token.stop_requested()) {
                auto location = requests_queue.try_pop();
                if (location)
                    process_one_request(*location);
                else if (location.error() == async_queue_exception::stop)
                    break;
                multi->perform();
                handle_finished_downloads();
                trim_cache();
                std::this_thread::sleep_for(50ms);
            }
        }
        catch (async_queue_exception) {
        }
        catch (std::exception& e) {
            cout << "worker thread error: " << e.what() << endl;
        }
        multi.reset();
    }


} // namespace IconManager
