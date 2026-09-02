/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <print>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glaze/json/write.hpp>
#include <glaze/exceptions/core_exceptions.hpp>

#include "TraceManager.hpp"

#include "App.hpp"
#include "LogManager.hpp"
#include "thread_safe.hpp"
#include "tracer.hpp"


using namespace std::literals;


namespace TraceManager {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        using dbl_microseconds = std::chrono::duration<double, std::micro>;

        struct custom_glz_opts_t : glz::opts {
            // static constexpr std::string_view float_format = "{:.3f}";
        };


        struct Event {
            dbl_microseconds ts;
            std::string_view name;
            std::optional<std::string_view> cat{};
            std::size_t tid;
            int pid;
            char ph;
            std::optional<char> s{};
            std::optional<glz::generic_u64> args{};
        }; // struct Event


        struct EventBuffer {

            using DataArray = std::array<Event, 65536>;


            constexpr
            EventBuffer()
                noexcept = default;

            void
            clear();

            DataArray::size_type
            size()
                const noexcept;

            bool
            full()
                const noexcept;

            void
            add(const Event& e);

            DataArray::const_iterator
            begin()
                const noexcept;

            DataArray::const_iterator
            end()
                const noexcept;

            const Event*
            data()
                const noexcept;

        private:

            DataArray data_;
            DataArray::size_type size_{0};

        }; // struct EventBuffer


        using EventBufferPtr = std::unique_ptr<EventBuffer>;

        using EventBufferPtrList = std::deque<EventBufferPtr>;


        enum class ThreadError {
            none,
            full,
            out_of_memory,
            unknown_error,
        };


        struct ThreadContext {

            thread_safe<EventBufferPtrList> safe_empty_buffers;
            thread_safe<EventBufferPtrList> safe_full_buffers;
            EventBufferPtr current_buffer;
            ThreadError error = ThreadError::none;

        }; //struct ThreadContext

        using ThreadContextPtr = std::shared_ptr<ThreadContext>;

        using ThreadContextMap = std::unordered_map<std::size_t, ThreadContextPtr>;


        struct ThreadContextManager {

            std::atomic_bool active{false};

            thread_safe<ThreadContextMap> safe_producers;


            void
            shutdown();


            EventBufferPtr
            get_new_buffer_for(ThreadContextPtr ctx);

        }; // struct ThreadContextManager


        // Constants

        constexpr custom_glz_opts_t custom_glz_opts;

        std::size_t max_full_buffers_per_thread = 64;


        /*-----------*/
        /* Variables */
        /*-----------*/

        std::chrono::steady_clock::time_point start_time;

        ThreadContextManager context_manager;

        std::jthread collector_thread;

        thread_local std::shared_ptr<ThreadContext> thread_context;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        bool
        add_event(const Event& e)
            noexcept;

        void
        collector_thread_function(std::stop_token stopper);

        void
        dump_buffer(EventBufferPtr& buffer,
                    std::ostream& output,
                    std::string& json_buffer,
                    bool& started);

        void
        dump_buffers(EventBufferPtrList& buffers,
                     std::ostream& output,
                     std::string& json_buffer,
                     bool& started);

        bool
        dump_full_buffers(ThreadContextPtr& ctx,
                          EventBufferPtrList& work_buffers,
                          std::ostream& output,
                          std::string& json_buffer,
                          bool& started);

        template<typename T>
        std::size_t
        get_hash(const T& val);

        std::size_t
        get_tid();

        ThreadContextPtr&
        get_thread_context();

        dbl_microseconds
        get_timestamp_floor();

        dbl_microseconds
        get_timestamp_ceil();

        [[maybe_unused]]
        const std::filesystem::path
        make_log_filename();


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        EventBuffer::clear()
        {
            size_ = 0;
        }


        EventBuffer::DataArray::size_type
        EventBuffer::size()
            const noexcept
        {
            return size_;
        }


        bool
        EventBuffer::full()
            const noexcept
        {
            return size_ >= data_.size();
        }


        void
        EventBuffer::add(const Event& e)
        {
            if (size_ < data_.size())
                data_[size_++] = e;
        }


        EventBuffer::DataArray::const_iterator
        EventBuffer::begin()
            const noexcept
        {
            return data_.begin();
        }


        EventBuffer::DataArray::const_iterator
        EventBuffer::end()
            const noexcept
        {
            return begin() + size();
        }


        [[maybe_unused]]
        const Event*
        EventBuffer::data()
            const noexcept
        {
            return data_.data();
        }


        void
        ThreadContextManager::shutdown()
        {
            active = false;
            auto producers = safe_producers.lock();
            producers->clear();
        }


        EventBufferPtr
        ThreadContextManager::get_new_buffer_for(ThreadContextPtr ctx)
        {
            if (active) {
                auto tid = get_tid();
                LOG_DEBUG("creating new event buffer for {}", tid);
                auto producers = safe_producers.lock();
                if (!producers->contains(tid))
                    producers->emplace(tid, std::move(ctx));
            }
            try {
                return std::make_unique<EventBuffer>();
            }
            catch (...) {
                return {};
            }
        }


        bool
        add_event(const Event& e)
            noexcept
        {
            if (!context_manager.active)
                return false;

            auto& ctx = get_thread_context();
            try {
                if (ctx->error != ThreadError::none)
                    return false;

                auto& cur = ctx->current_buffer;
                if (cur && cur->full()) {
                    auto full_buffers = ctx->safe_full_buffers.lock();
                    if (full_buffers->size() >= max_full_buffers_per_thread) {
                        LOG_ERROR("Thread {} has too many full buffers!", e.tid);
                        ctx->error = ThreadError::full;
                        return false;
                    }
                    full_buffers->push_back(std::move(cur));
                }
                if (!cur) {
                    auto empty_buffers = ctx->safe_empty_buffers.lock();
                    if (!empty_buffers->empty()) {
                        cur = std::move(empty_buffers->front());
                        empty_buffers->pop_front();
                    } else
                        cur = context_manager.get_new_buffer_for(ctx);
                }

                if (!cur) {
                    ctx->error = ThreadError::out_of_memory;
                    return false;
                }

                cur->add(e);

                return true;
            }
            catch (std::bad_alloc&) {
                ctx->error = ThreadError::out_of_memory;
            }
            catch (...) {
                // Silently ignore
                ctx->error = ThreadError::unknown_error;
            }
            return false;
        }


        void
        collector_thread_function(std::stop_token stopper)
        {
            try {
                LOG_DEBUG("Started collector thread");

                context_manager.active = true;

                auto filename = make_log_filename();
                if (stopper.stop_requested())
                    return;

                LOG_DEBUG("Collector thread: writing to {}", filename.string());

                std::ofstream output{filename};
                if (!output)
                    throw std::runtime_error{"Failed to open: " + filename.string()};

                output << "[\n";

                // Note: these containers are held outside the loop so their memory can be
                // reused, reducing alloations.
                std::string json_buffer;
                std::vector<ThreadContextPtr> producers;
                EventBufferPtrList work_buffers;
                bool started = false;

                while (!stopper.stop_requested()) {

                    bool worked = false;

                    // With producers locked: collect all contexts into a vector.
                    producers.clear();
                    {
                        auto all_producers = context_manager.safe_producers.lock();
                        for (auto& [tid, ctx] : *all_producers)
                            producers.push_back(ctx);
                    }

                    if (stopper.stop_requested())
                        break;

                    for (auto& ctx : producers)
                        if (dump_full_buffers(ctx, work_buffers, output, json_buffer, started))
                            worked = true;

                    if (!worked)
                        std::this_thread::sleep_for(10ms);

                }

                context_manager.active = false;

                std::println("Finishing event collection.");
                // Assume all threads stopped generating events, so take their buffers.
                auto all_producers = context_manager.safe_producers.lock();
                for (auto& [tid, ctx] : *all_producers) {
                    std::println("Dumping full buffers from {}", tid);
                    dump_full_buffers(ctx, work_buffers, output, json_buffer, started);
                    // Also consume the current_buffer
                    if (ctx->current_buffer) {
                        std::println("Dumping current buffer from {}: {} events",
                                     tid,
                                     ctx->current_buffer->size());
                        dump_buffer(ctx->current_buffer, output, json_buffer, started);
                    }
                }

                output << "\n]\n";
            }
            catch (std::exception& e) {
                LOG_ERROR("dumping traces: {}", e.what());
            }
        }


        void
        dump_buffer(EventBufferPtr& buffer,
                    std::ostream& output,
                    std::string& json_buffer,
                    bool& started)
        {
            for (const auto& event : *buffer) {
                json_buffer.clear();
                glz::ex::write<custom_glz_opts>(event, json_buffer);
                if (started)
                    output << ",\n";
                output << json_buffer;
                started = true;
            }
            buffer->clear();
        }


        void
        dump_buffers(EventBufferPtrList& buffers,
                     std::ostream& output,
                     std::string& json_buffer,
                     bool& started)
        {
            for (auto& buffer : buffers)
                dump_buffer(buffer, output, json_buffer, started);
        }


        bool
        dump_full_buffers(ThreadContextPtr& ctx,
                          EventBufferPtrList& work_buffers,
                          std::ostream& output,
                          std::string& json_buffer,
                          bool& started)
        {
            work_buffers.clear();

            // Lock ctx->safe_full_buffers, swap it with buffers (empty)
            {
                auto full_buffers = ctx->safe_full_buffers.lock();
                full_buffers->swap(work_buffers);
            }

            if (work_buffers.empty())
                return false;

            // With no locks held, write out the full buffers, then clear them.
            dump_buffers(work_buffers, output, json_buffer, started);

            // Lock ctx->safe_empty_buffers, swap in the buffers.
            {
                auto empty_buffers = ctx->safe_empty_buffers.lock();
                empty_buffers->append_range(work_buffers | std::views::as_rvalue);
            }

            return true;
        }


        template<typename T>
        std::size_t
        get_hash(const T& val)
        {
            return std::hash<T>{}(val);
        }


        std::size_t
        get_tid()
        {
            return get_hash(std::this_thread::get_id());
        }


        ThreadContextPtr&
        get_thread_context()
        {
            if (!thread_context)
                thread_context = std::make_shared<ThreadContext>();
            return thread_context;
        }


        dbl_microseconds
        get_timestamp_floor()
        {
            auto now = std::chrono::steady_clock::now();
            auto dt = now - start_time;
            return std::chrono::floor<dbl_microseconds>(dt);
        }


        dbl_microseconds
        get_timestamp_ceil()
        {
            auto now = std::chrono::steady_clock::now();
            auto dt = now - start_time;
            return std::chrono::ceil<dbl_microseconds>(dt);
        }


        const std::filesystem::path
        make_log_filename()
        {
            auto now = std::chrono::system_clock::now();
            auto now_seconds = std::chrono::floor<std::chrono::seconds>(now);
            auto today = std::chrono::floor<std::chrono::days>(now_seconds);
            std::chrono::year_month_day date = today;
            std::chrono::hh_mm_ss time{now_seconds - today};
            std::string filename = std::format("trace-{}-{:%H-%M-%S}.json", date, time);
            std::ranges::replace(filename, ':', '-');
            return App::get_config_path() / filename;
        }


        /*
         * NOTE: On Wii U, thread_local is not supported, so we need to manually call the thread
         * specific API from wut. We cannot rely on the thread to destroy the context, so the
         * context manager will have sole ownership. The context manager will create the thread
         * context manually as needed, and return a null pointer when shutting down, just in
         * case there's a lingering thread that has not finished. Gotta track this "shutting
         * down" flag in the manager, for this situation.
         */


    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize(std::string_view proc_name)
    {
        TRACE_FUNC;

        start_time = std::chrono::steady_clock::now();

        collector_thread = std::jthread{collector_thread_function};

        for (unsigned i = 0; i < 100; ++i) {
            if (context_manager.active)
                break;
            std::this_thread::sleep_for(1ms);
        }

        process_name(proc_name);
        thread_name("main thread");
    }


    void
    finalize()
    {
        TRACE_FUNC;

        collector_thread = {};
        context_manager.shutdown();
    }


    bool
    duration_begin(std::string_view name,
                   std::optional<std::string_view> cat,
                   std::optional<glz::generic_u64> args)
    {
        return add_event(
            {
                .ts = get_timestamp_floor(),
                .name = std::move(name),
                .cat = std::move(cat),
                .tid = get_tid(),
                .pid = 1,
                .ph = 'B',
                .args = std::move(args),
            }
        );
    }


    bool
    duration_end(std::string_view name,
                 std::optional<std::string_view> cat,
                 std::optional<glz::generic_u64> args)
    {
        return add_event(
            {
                .ts = get_timestamp_ceil(),
                .name = std::move(name),
                .cat = std::move(cat),
                .tid = get_tid(),
                .pid = 1,
                .ph = 'E',
                .args = std::move(args),
            }
        );
    }


    void
    instant(std::string_view name,
            std::optional<std::string_view> cat,
            std::optional<char> scope,
            std::optional<glz::generic_u64> args)
    {
        add_event(
            {
                .ts = get_timestamp_floor(),
                .name = std::move(name),
                .cat = std::move(cat),
                .tid = get_tid(),
                .pid = 1,
                .ph = 'i',
                .s = std::move(scope),
                .args = std::move(args),
            }
        );
    }


    void
    process_name(std::string_view proc_name)
    {
        glz::generic_u64 args;
        args["name"] = proc_name;
        add_event(
            {
                .ts = get_timestamp_floor(),
                .name = "process_name"sv,
                .tid = get_tid(),
                .pid = 1,
                .ph = 'M',
                .args = std::move(args),
            }
        );
    }


    void
    thread_name(std::string_view thread_name)
    {
        glz::generic_u64 args;
        args["name"] = thread_name;
        add_event(
            {
                .ts = get_timestamp_floor(),
                .name = "thread_name"sv,
                .tid = get_tid(),
                .pid = 1,
                .ph = 'M',
                .args = std::move(args),
            }
        );
    }


    void
    vsync()
    {
        add_event(
            {
                .ts = get_timestamp_floor(),
                .name = "VSync"sv,
                .cat = "gpu"sv,
                .tid = get_tid(),
                .pid = 1,
                .ph = 'i',
                .s = 'p',
            }
        );
    }

} // namespace TraceManager
