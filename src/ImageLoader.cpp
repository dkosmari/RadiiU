/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <curlxx/curl.hpp>
#include <sdl2xx/img.hpp>

#include "ImageLoader.hpp"

#include "App.hpp"
#include "async_queue.hpp"
#include "LogManager.hpp"
#include "LogManagerCurl.hpp"
#include "mime_type.hpp"
#include "Settings.hpp"
#include "tracer.hpp"


using namespace std::literals;

using sdl::vec2;

using Settings::cfg;


namespace ImageLoader {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class LoadState : int {
            unloaded,
            loading,
            loaded,
            converted,
            error,
        };


        struct CacheKey {

            std::string location;
            vec2 size;


            constexpr
            bool
            operator ==(const CacheKey& other)
                const noexcept = default;

            constexpr
            auto
            operator <=>(const CacheKey& other)
                const noexcept = default;

        }; // struct CacheKey


        struct CacheKeyHasher {
            std::size_t
            operator ()(const CacheKey& key)
                const noexcept;
        }; // struct CacheKeyHasher


        struct CacheEntry : std::enable_shared_from_this<CacheEntry> {
            std::atomic<LoadState> state{LoadState::loading};
            std::uint64_t last_use = 0;
            sdl::texture tex;
            sdl::surface img;
            curl::easy easy{nullptr};
            std::optional<std::vector<char>> raw_buf; // TODO: use byte_stream, implement rwops
            std::string location;
            vec2 size_limit;
            bool checked_content_type = false;

            CacheEntry(std::uint64_t now,
                       const std::string& location,
                       const vec2& size_limit);

            ~CacheEntry()
                noexcept;

            void
            finish_download();

            bool
            is_remote()
                const noexcept;

            void
            load();

            void
            make_texture(sdl::renderer& renderer);

            curl::easy&
            start_download(const std::string& user_agent);

        private:

            std::size_t
            easy_write_callback(std::span<const char> buf);

            void
            enforce_size_limit();

            void
            load_from_buffer();

            void
            load_from_file();

        }; // struct CacheEntry

        using CacheEntryPtr = std::shared_ptr<CacheEntry>;


        // TODO: use a good hash map here
        using Cache = std::unordered_map<CacheKey, CacheEntryPtr, CacheKeyHasher>;


        struct Resources {

            std::uint64_t timestamp = 0;
            sdl::renderer& renderer;
            std::string user_agent;
            std::filesystem::path content_dir;
            sdl::texture error_icon;
            sdl::texture loading_icon;
            curl::multi multi;
            Cache cache;

            async_queue<CacheEntryPtr> load_queue;
            std::jthread loader_thread;

            async_queue<CacheEntryPtr> convert_queue;

            Resources(sdl::renderer& renderer,
                      const std::string& user_agent,
                      const std::filesystem::path& content_dir);

            ~Resources()
                noexcept;

            // Prevent moving
            Resources(Resources&&) = delete;

            const sdl::texture*
            get(const std::string& location,
                const sdl::vec2& size_limit);

            void
            loader_thread_func(std::stop_token stopper);

            void
            prepare_load(CacheEntryPtr entry);

            void
            process();

            void
            trim_cache();

        }; // struct Resources


        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string content_prefix = "content:/"s;
        const std::size_t max_cache_size = 256;

        // Variables

        std::optional<Resources> res;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        std::uint64_t&
        by_last_use(Cache::iterator it)
            noexcept;

        template<typename T>
        std::size_t
        calc_hash(const T& val);

        sdl::surface
        optimized(sdl::surface input);

        template<std::ranges::random_access_range R,
                 typename Comp = std::ranges::less,
                 class Proj = std::identity>
        requires std::sortable<std::ranges::iterator_t<R>, Comp, Proj>
        void
        sift_down_heap(R&& r,
                       Comp comp = {},
                       Proj proj = {});

        std::string
        to_string(LoadState st);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        std::uint64_t&
        by_last_use(Cache::iterator it)
            noexcept
        {
            return it->second->last_use;
        }


        std::size_t
        CacheKeyHasher::operator ()(const CacheKey& key)
            const noexcept
        {
            return
                std::rotl(calc_hash(key.location), 16)
                ^
                std::rotl(calc_hash(key.size.x), 8)
                ^
                calc_hash(key.size.y);
        }


        CacheEntry::CacheEntry(std::uint64_t now,
                               const std::string& location_,
                               const vec2& size_limit_) :
            last_use{now},
            location{location_},
            size_limit{size_limit_}
        {
            if (size_limit.x < 0)
                size_limit.x = 0;
            if (size_limit.y < 0)
                size_limit.y = 0;
        }


        CacheEntry::~CacheEntry()
            noexcept
        {}

        void
        CacheEntry::finish_download()
        {
            easy.destroy();
        }


        bool
        CacheEntry::is_remote()
            const noexcept
        {
            return location.starts_with("http://")
                || location.starts_with("https://");
        }


        void
        CacheEntry::load()
        {
            if (state != LoadState::loading) {
                LOG_ERROR("wrong cache entry state: {}", to_string(state.load()));
                return;
            }

            try {
                if (is_remote())
                    load_from_buffer();
                else
                    load_from_file();

                img = optimized(std::move(img));

                enforce_size_limit();
                state = LoadState::loaded;
            }
            catch (std::exception& e) {
                LOG_ERROR("loading image:\n"
                          "  location: {:?}\n"
                          "  what: {}",
                          location,
                          e.what());
                state = LoadState::error;
                throw;
            }
        }


        void
        CacheEntry::make_texture(sdl::renderer& renderer)
        {
            assert(img);
            assert(!tex);
            assert(state == LoadState::loaded);
            tex.create(renderer, img);
            tex.set_blend_mode(SDL_BLENDMODE_BLEND);
            img.destroy();
            state = LoadState::converted;
        }


        curl::easy&
        CacheEntry::start_download(const std::string& user_agent)
        {
            easy.create();
            if (!user_agent.empty())
                easy.set_user_agent(user_agent);
            easy.set_accept_encoding("");
            easy.set_auto_referer(true);
            easy.set_buffer_size(65536);
            easy.set_follow_location(true);
            easy.set_http_headers({ "Accept: image/*" });
            easy.set_http_version(curl::easy::http_version::none);
            easy.set_private(shared_from_this());
            easy.set_ssl_verify_peer(false);
            easy.set_tcp_no_delay(false);
            easy.set_transfer_encoding(true);
            easy.set_url(location);
            easy.set_verbose(cfg.verbose_image_logs);
            easy.set_write_function(std::bind_front(&CacheEntry::easy_write_callback, this));
            LogManagerCurl::capture_curl_debug(easy);
            checked_content_type = false;
            return easy;
        }


        std::size_t
        CacheEntry::easy_write_callback(std::span<const char> buf)
        {
            if (!checked_content_type) {
                checked_content_type = true;
                if (auto content_type = easy.try_get_header("Content-Type")) {
                    std::string desired = "image/*"s;
                    std::string received = content_type->value;
                    if (!mime_type::match(received, desired)) {
                        LOG_ERROR("Content-Type should be {:?} but received {:?}",
                                  desired,
                                  received);
                        return CURL_READFUNC_ABORT;
                    }
                }
            }

            if (!raw_buf)
                raw_buf.emplace();
            raw_buf->append_range(buf);
            return buf.size();
        }


        void
        CacheEntry::enforce_size_limit()
        {
            if (size_limit.x <= 0 && size_limit.y <= 0)
                return;

            const auto size = img.get_size();

            const double x_scale = double(size_limit.x) / size.x;
            const double y_scale = double(size_limit.y) / size.y;

            vec2 scaled_size;
            if (size_limit.x > 0 && size_limit.y > 0) {
                // Select the smaller of the two scales.
                if (x_scale < y_scale) {
                    // Only shrink.
                    if (x_scale >= 1)
                        return;
                    // Scale based on X limit.
                    int new_height = size.y * x_scale;
                    if (new_height < 1)
                        new_height = 1;
                    scaled_size = {size_limit.x, new_height};
                } else {
                    // Only shrink.
                    if (y_scale >= 1)
                        return;
                    // Scale based on Y limit.
                    int new_width = size.x * y_scale;
                    if (new_width < 1)
                        new_width = 1;
                    scaled_size = {new_width, size_limit.y};
                }
            } else {
                // Only one limit exists.
                if (size_limit.x > 0) {
                    // Only shrink.
                    if (x_scale >= 1)
                        return;
                    // Scale based on X limit.
                    int new_height = size.y * x_scale;
                    if (new_height < 1)
                        new_height = 1;
                    scaled_size = {size_limit.x, new_height};
                } else {
                    // Only shrink.
                    if (y_scale >= 1)
                        return;
                    // Scale based on Y limit.
                    int new_width = size.x * y_scale;
                    if (new_width < 1)
                        new_width = 1;
                    scaled_size = {new_width, size_limit.y};
                }
            }
            sdl::surface scaled_img{scaled_size, 32, img.get_format_enum()};
            sdl::blit_scaled(img, nullptr, scaled_img, nullptr);
            img = std::move(scaled_img);
        }


        void
        CacheEntry::load_from_buffer()
        {
            assert(raw_buf);
            sdl::rwops rw{std::span(*raw_buf)};
            img = sdl::img::load(rw);
            raw_buf.reset();
            LOG_DEBUG("Loaded URL {:?}", location);
        }


        void
        CacheEntry::load_from_file()
        {
            img = sdl::img::load(location);
            LOG_DEBUG("Loaded file {:?}", location);
        }


        template<typename T>
        std::size_t
        calc_hash(const T& val)
        {
            return std::hash<T>{}(val);
        }


        /*
         * NOTE: GX2 only supports a few texture formats, so this allows converting the image to
         * a supported format early.
         */
        sdl::surface
        optimized(sdl::surface input)
        {
            if (!input)
                return {};
            switch (input.get_format().get_enum()) {
                // Supported formats, see SDL_render_wiiu.h from the SDL2 port.
                using enum sdl::pixels::format_enum;

                case argb_4444:
                case rgba_4444:
                case abgr_4444:
                case bgra_4444:
                case argb_1555:
                case abgr_1555:
                case rgba_5551:
                case bgra_5551:
                case rgb_565:
                case bgr_565:
                case rgbx_8888:
                case rgba_8888:
                case argb_8888:
                case bgrx_8888:
                case bgra_8888:
                case abgr_8888:
                case argb_2101010:
                    return input;

                default:
                    return sdl::surface(input, sdl::pixels::format_enum::rgba_32);
            }
        }


        Resources::Resources(sdl::renderer& renderer_,
                             const std::string& user_agent_,
                             const std::filesystem::path& content_dir_) :
            renderer(renderer_),
            user_agent{user_agent_},
            content_dir{content_dir_},
            error_icon{sdl::img::load_texture(renderer,
                                              content_dir / "ui/error-icon.png")},
            loading_icon{sdl::img::load_texture(renderer,
                                                content_dir / "ui/loading-icon.png")}
        {
            TRACE_FUNC;

            error_icon.set_blend_mode(SDL_BLENDMODE_BLEND);
            loading_icon.set_blend_mode(SDL_BLENDMODE_BLEND);

            multi.set_max_total_connections(4);
            multi.set_max_connections(2);

            loader_thread = std::jthread{
                std::bind_front(&Resources::loader_thread_func, this)
            };
        }


        Resources::~Resources()
            noexcept
        {
            TRACE_FUNC;

            load_queue.stop();
            convert_queue.stop();

            for (auto& [location, entry] : cache)
                if (entry->easy)
                    multi.remove(entry->easy);
        }


        const sdl::texture*
        Resources::get(const std::string& location,
                       const sdl::vec2& size_limit)
        {
            const CacheKey key = {location, size_limit};
            CacheEntryPtr entry;
            auto it = cache.find(key);
            if (it != cache.end())
                entry = it->second;

            try {
                if (entry) {
                    entry->last_use = timestamp;

                    switch (entry->state) {

                        case LoadState::converted:
                            return &entry->tex;

                        case LoadState::error:
                            return &error_icon;

                        case LoadState::loading:
                        case LoadState::loaded:
                            return &loading_icon;

                        case LoadState::unloaded:
                            prepare_load(std::move(entry));
                            return &loading_icon;

                        default:
                            throw std::logic_error{"invalid entry state: "
                                                   + to_string(entry->state)};

                    }
                } else {
                    // entry not found, queue it up to load
                    std::string real_location;
                    LOG_DEBUG("Requested: {:?}", location);
                    if (location.starts_with(content_prefix)) {
                        real_location =
                            content_dir / location.substr(content_prefix.size());
                        LOG_DEBUG("Content: {:?}", real_location);
                    } else
                        real_location = location;

                    entry = std::make_shared<CacheEntry>(timestamp, real_location, size_limit);
                    cache[key] = entry;
                    prepare_load(std::move(entry));
                    return &loading_icon;
                }
            }
            catch (std::exception& e) {
                LOG_ERROR("{}", e.what());
                return &error_icon;
            }
        }


        void
        Resources::loader_thread_func(std::stop_token stopper)
        try {
            while (!stopper.stop_requested()) {
                auto entry = load_queue.try_pop_block(stopper);
                if (entry) {
                    try {
                        (*entry)->load();
                        convert_queue.push(std::move(*entry));
                    }
                    catch (std::exception& e) {
                        LOG_ERROR("Loading: {}", e.what());
                    }
                } else if (stopper.stop_requested() ||
                           entry.error() == async_queue_error::stop) {
                    break;
                } else if (entry.error() == async_queue_error::locked) {
                    LOG_ERROR("load_queue was locked");
                }
                // std::this_thread::sleep_for(50ms);
            }
        }
        catch (std::exception& e) {
            LOG_ERROR("{}", e.what());
        }


        void
        Resources::prepare_load(CacheEntryPtr entry)
        {
            entry->state = LoadState::loading;
            if (entry->is_remote())
                multi.add(entry->start_download(user_agent));
            else
                load_queue.push(std::move(entry));
        }


        void
        Resources::process()
        {
            ++timestamp;

            multi.perform();
            for (auto [easy, error_code] : multi.get_done()) {
                auto entry = std::any_cast<CacheEntryPtr>(easy->get_private());
                if (!entry) {
                    LOG_ERROR("invalid download handle: {:p}",
                              reinterpret_cast<void*>(easy));
                    continue;
                }

                multi.remove(*easy);
                entry->finish_download();

                try {
                    if (error_code)
                        throw curl::error{error_code};

                    load_queue.push(entry);
                }
                catch (std::exception& e) {
                    LOG_ERROR("Processing finished download: {}", e.what());
                    entry->state = LoadState::error;
                }
            }

            // Convert at most one image into a texture.
            if (auto value = convert_queue.try_pop()) {
                auto& entry = *value;
                entry->make_texture(renderer);
            }

            trim_cache();
        }


        void
        Resources::trim_cache()
        {
            if (cache.size() <= max_cache_size)
                return;

            std::size_t excess = cache.size() - max_cache_size;
            LOG_DEBUG("Prunning {} icons.", excess);
            std::vector<Cache::iterator> to_remove(excess);
            auto to_remove_end = to_remove.begin();
            for (auto it = cache.begin(); it != cache.end(); ++it) {
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
            // Now "to_remove" contains the "excess" elements that must be purged.
            for (auto it : to_remove) {
                auto& entry = it->second;
                if (entry->easy) {
                    LOG_DEBUG("prunning an active request");
                    // If removing an active request, make sure it's removed from the curl::multi.
                    multi.remove(entry->easy);
                }
                // unordered_map guarantees all other iterators remain valid
                cache.erase(it);
            }
        }


        // Heap operation: After lowering the max element, push it downwards to its correct
        // location.
        template<std::ranges::random_access_range R,
                 typename Comp,
                 class Proj>
        requires std::sortable<std::ranges::iterator_t<R>, Comp, Proj>
        void
        sift_down_heap(R&& r,
                       Comp comp,
                       Proj proj)
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


        std::string
        to_string(LoadState st)
        {
            switch (st) {
                using enum LoadState;
                case unloaded:
                    return "unloaded";
                case loading:
                    return "loading";
                case loaded:
                    return "loaded";
                case converted:
                    return "converted";
                case error:
                    return "error";
                default:
                    return "unknown (" + std::to_string(static_cast<int>(st)) + ")";
            }
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize(sdl::renderer& rend)
    {
        TRACE_FUNC;

        res.emplace(rend,
                    App::get_user_agent(),
                    App::get_content_path());
    }


    void
    finalize()
    {
        TRACE_FUNC;

        res.reset();
    }


    void
    process()
    {
        res->process();
    }


    const sdl::texture*
    get(const std::string& location,
        const sdl::vec2& size_limit)
    {
        return res->get(location, size_limit);
    }

} // namespace ImageLoader
