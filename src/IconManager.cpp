/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <filesystem>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <optional>
#include <queue>
#include <ranges>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <curlxx/curl.hpp>
#include <sdl2xx/img.hpp>

#include "IconManager.hpp"

#include "async_queue.hpp"
#include "thread_safe.hpp"
#include "tracer.hpp"
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


    enum class LoadState : int {
        unloaded,
        requested,
        loading,
        loaded,
        error,
    };

    std::string
    to_string(LoadState st);


    struct CacheEntry {
        std::atomic<LoadState> state{LoadState::unloaded};
        std::uint64_t last_use = 0;
        sdl::surface img;
        sdl::texture tex;
        std::optional<curl::easy> easy;
        std::optional<std::vector<char>> raw_buf;
        std::string location;
    };


    // TODO: use a good hash map here
    using cache_t = std::unordered_map<std::string, CacheEntry>;
    thread_safe<cache_t> safe_cache;


    // std::queue<std::string> load_queue;
    async_queue<std::string> requests_queue;


    std::jthread worker_thread;


    void
    worker_func(std::stop_token token);


    void
    initialize(sdl::renderer& rend)
    {
        TRACE_FUNC;

        user_agent = utils::get_user_agent();
        content_prefix = utils::get_content_path();

        renderer = &rend;

        error_icon   = sdl::img::load_texture(*renderer,
                                              content_prefix / "ui/error-icon.png");
        error_icon.set_blend_mode(SDL_BLENDMODE_BLEND);

        loading_icon = sdl::img::load_texture(*renderer,
                                              content_prefix / "ui/loading-icon.png");
        loading_icon.set_blend_mode(SDL_BLENDMODE_BLEND);

        requests_queue.reset();
        cout << "IconManager: launching worker thread." << endl;
        worker_thread = std::jthread{worker_func};

        assert((std::atomic<LoadState>{}.is_lock_free()));
    }


    void
    finalize()
    {
        TRACE_FUNC;

        cout << "Stopping requests_queue" << endl;
        requests_queue.stop();

        cout << "Destroying thread." << endl;
        worker_thread = {};
        cout << "Thread destroyed." << endl;

        {
            cout << "Clearing safe_cache" << endl;
            auto cache = safe_cache.lock();
            cache->clear();
        }

        cout << "Destroying predefined icons" << endl;
        loading_icon.destroy();
        error_icon.destroy();
    }


    const sdl::texture*
    get(const std::string& location)
    {
        ++use_counter;
        auto cache = safe_cache.lock();
        auto it = cache->find(location);
        try {
            if (it != cache->end()) {
                auto& status = it->second;
                status.last_use = use_counter;

                try {
                    switch (status.state) {

                        case LoadState::loaded:
                            if (!status.tex) {
                                status.tex.create(*renderer, status.img);
                                status.tex.set_blend_mode(SDL_BLENDMODE_BLEND);
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

                        default:
                            throw std::logic_error{"invalid entry state: "
                                                   + to_string(status.state)};
                    }
                }
                catch (std::exception& e) {
                    status.state = LoadState::error;
                    throw;
                }
            } else {
                (*cache)[location].state = LoadState::requested;
                requests_queue.push(location);
                return &loading_icon;
            }
        }
        catch (std::exception& e) {
            cout << "ERROR: IconManager::get(): " << e.what() << endl;
            return &error_icon;
        }
    }


    // Find a CacheEntry that contains this specific curl::easy object.
    CacheEntry*
    find(thread_safe<cache_t>::guard<cache_t>& cache,
         const curl::easy* ez)
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
        // TRACE("IconManager::process_one_request(\"" + location + "\")");

        cache_t::iterator it;

        {
            auto cache = safe_cache.lock();
            it = cache->find(location);
            if (it == cache->end())
                return;
            LoadState expected = LoadState::requested;
            if (!it->second.state.compare_exchange_strong(expected, LoadState::loading)) {
                cout << "ERROR: IconManager::process_one_request() wrong cache entry state: "
                     << to_string(expected) << endl;
                return;
            }
        }

        auto& entry = it->second;
        entry.location = location;

        try {

            if (location.starts_with("http://") || location.starts_with("https://")) {
                // URL
                auto& ez = entry.easy.emplace();
                ez.set_verbose(false);
                ez.set_ssl_verify_peer(false);
                if (!user_agent.empty())
                    ez.set_user_agent(user_agent);
                ez.set_url(location);
                ez.set_follow_location(true);
                ez.set_http_headers({ "Accept: image/*" });
                ez.set_write_function([&entry](std::span<const char> buf) -> std::size_t
                {
                    auto content_type_header = entry.easy->try_get_header("Content-Type");
                    if (content_type_header) {
                        std::string ct = content_type_header->value;
                        if (!ct.starts_with("image/")) {
                            cout << "ERROR: Content-Type should be \"image/*\" but got \""
                                 << ct << "\"" << endl;
                            return CURL_READFUNC_ABORT;
                        }
                    }

                    if (!entry.raw_buf)
                        entry.raw_buf.emplace();
#ifdef __cpp_lib_containers_ranges
                    entry.raw_buf->append_range(buf);
#else
                    entry.raw_buf->insert(entry.raw_buf->end(),
                                          buf.begin(),
                                          buf.end());
#endif
                    return buf.size();
                });
                multi->add(ez);
            } else if (location.starts_with("ui/")) {
                // local path
                // cout << "Loading local image from " << location << endl;
                entry.img = sdl::img::load(content_prefix / location);
                entry.state = LoadState::loaded;
                // cout << "Created local image in format: "
                //      << entry.img.get_format_enum()
                //      << endl;
            } else
                throw std::runtime_error{"invalid location"};

        }
        catch (std::exception& e) {
            cout << "ERROR: IconManager::process_one_request():\n"
                 << "  location: \"" << location << "\"\n"
                 << "  exception: " << e.what() << endl;
            entry.state = LoadState::error;
        }
    }


    // Pushes the max element downwards to its correct heap location.
    template<std::ranges::random_access_range R,
             typename Comp = std::ranges::less,
             class Proj = std::identity>
    requires std::sortable<std::ranges::iterator_t<R>, Comp, Proj>
    void
    sift_down_heap(R&& r,
                   Comp comp = {},
                   Proj proj = {})
    {
        if (std::empty(r))
            return;
        const auto size = std::size(r);
        std::ranges::range_size_t<R> cur_idx = 0;
        for (;;) {
            auto left_idx = 2u * cur_idx + 1u;
            // If reached bottom of heap, we can stop.
            if (left_idx >= size)
                break;
            // Select the largest child to be the next.
            auto next_idx = left_idx;
            auto right_idx = 2u * cur_idx + 2u;
            if (right_idx < size
                && std::invoke(comp,
                               std::invoke(proj, r[left_idx]),
                               std::invoke(proj, r[right_idx])))
                next_idx = right_idx;
            // If max-heap property was restored, we can stop.
            if (!std::invoke(comp,
                             std::invoke(proj, r[cur_idx]),
                             std::invoke(proj, r[next_idx])))
                break;
            std::swap(r[cur_idx], r[next_idx]);
            cur_idx = next_idx;
        }
    }


    std::uint64_t&
    by_last_use(cache_t::iterator it)
        noexcept
    {
        return it->second.last_use;
    }


    void
    trim_cache()
    {
        auto cache = safe_cache.lock();
        if (cache->size() <= max_cache_size)
            return;

        std::size_t excess = cache->size() - max_cache_size;
        // cout << "IconManager: prunning " << excess << " icons" << endl;
        std::vector<cache_t::iterator> to_remove(excess);
        auto to_remove_end = to_remove.begin();
        for (auto it = cache->begin(); it != cache->end(); ++it) {
            if (to_remove_end != to_remove.end()) {
                *to_remove_end++ = it;
                std::ranges::push_heap(to_remove.begin(),
                                       to_remove_end,
                                       {},
                                       by_last_use);
            } else {
                /*
                 * Heap is already full:
                 * If new element is older than the max element on the heap,
                 * just replace the max element, and update the heap.
                 */
                if (by_last_use(it) < by_last_use(to_remove.front())) {
                    to_remove.front() = it;
                    sift_down_heap(to_remove, {}, by_last_use);
                }
            }
        }
        // Now to_remove contains the "excess" elements that must be purged.
        for (auto it : to_remove) {
            auto& info = it->second;
            if (info.easy) {
                // If removing an active request, make sure it's removed from the curl::multi.
                // cout << "IconManager: prunning an active request" << endl;
                multi->remove(*info.easy);
            }
            // unordered_map guarantees all other iterators remain valid
            cache->erase(it);
        }
    }


    void
    handle_finished_downloads()
    {
        for (auto [ez, error_code] : multi->get_done()) {
            auto cache = safe_cache.lock();
            auto* entry = find(cache, ez);
            if (!entry) {
                cout << "ERROR: IconManager::handle_finished_downloads(): failed to find entry for "
                     << ez << endl;
                continue;
            }

            try {
                if (error_code)
                    throw curl::error{error_code};

                if (!entry->raw_buf)
                    throw std::runtime_error{"empty download"};

                // cout << "Loading image from " << entry->location << endl;
                sdl::rwops rw{std::span(*entry->raw_buf)};
                auto img = sdl::img::load(rw);
                const int max_size = 256; // TODO: make it customizable per icon
                const sdl::vec2 old_size = img.get_size();
                if (old_size.x > max_size || old_size.y > max_size) {
                    sdl::vec2 new_size;
                    if (old_size.x > old_size.y) {
                        new_size.x = max_size;
                        new_size.y = std::max(1, max_size * old_size.y / old_size.x);
                    } else {
                        new_size.y = max_size;
                        new_size.x = std::max(1, max_size * old_size.x / old_size.y);
                    }
                    entry->img.create(new_size, 32, img.get_format_enum());
                    // cout << "Created shrunk image in format: "
                    //      << entry->img.get_format_enum()
                    //      << endl;
                    sdl::blit_scaled(img, nullptr, entry->img, nullptr);
                } else {
                    entry->img = std::move(img);
                    // cout << "Created image in format: "
                    //      << entry->img.get_format_enum()
                    //      << endl;
                }
                entry->state = LoadState::loaded;
                entry->raw_buf.reset();
            }
            catch (std::exception& e) {
                cout << "ERROR: IconManager::handle_finished_downloads(): " << e.what() << endl;
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
        TRACE_FUNC;

        try {
            multi.emplace();
            multi->set_max_total_connections(10);
            multi->set_max_connections(10);

            while (!token.stop_requested()) {
#if 1
                auto location = requests_queue.try_pop_for(50ms);
#else
                auto location = requests_queue.try_pop();
#endif
                if (location) {
                    process_one_request(*location);
                } else if (location.error() == async_queue_error::stop) {
                    break;
                } else if (location.error() == async_queue_error::locked) {
                    cout << "WARNING: requests_queue was locked" << endl;
                }
                multi->perform();
                handle_finished_downloads();
                trim_cache();
                std::this_thread::sleep_for(50ms);
            }
        }
        catch (std::exception& e) {
            cout << "ERROR: IconManager::worker_func(): " << e.what() << endl;
        }
        multi.reset();
    }


    std::string
    to_string(LoadState st)
    {
        switch (st) {
            case LoadState::unloaded:
                return "unloaded";
            case LoadState::requested:
                return "requested";
            case LoadState::loading:
                return "loading";
            case LoadState::loaded:
                return "loaded";
            case LoadState::error:
                return "loaded";
            default:
                return "unknown (" + std::to_string(static_cast<int>(st)) + ")";
        }
    }

} // namespace IconManager
