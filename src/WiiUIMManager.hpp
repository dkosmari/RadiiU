/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WII_U_IM_MANAGER_HPP
#define WII_U_IM_MANAGER_HPP

#ifdef __WIIU__

#include <atomic>

#include <coreinit/im.h>


namespace WiiUIMManager {

    // RAII type for IM handles
    struct IMDevice {

        struct AsyncCall {

            enum class State {
                idle,
                started,
                waiting,
                handling,
                shutdown_start,
                shutdown_finish,
            };

            alignas(0x40) IMRequest request{};
            IMDevice& device;
            std::atomic<State> state{State::idle};

            AsyncCall(IMDevice& dev);

            // Prevent moving.
            AsyncCall(AsyncCall&&) = delete;

            virtual
            ~AsyncCall()
                noexcept;

            virtual
            void
            callback(IOSError e);

        protected:

            static
            void
            callback_helper(IOSError e,
                            void* ctx)
                noexcept;

            bool
            finish_shutdown()
                noexcept;

        }; // struct AsyncCall


        struct EventWatcher : AsyncCall {

            IMEventMask desired;
            IMEventMask event{};

            EventWatcher(IMDevice& dev,
                         IMEventMask ev);

            ~EventWatcher()
                noexcept;

            void
            watch();

        protected:

            static
            void
            callback_helper(IOSError e,
                            void* ctx)
                noexcept;

        }; // struct EventWatcher


        const IOSHandle handle;

        IMDevice();

        ~IMDevice()
            noexcept;

    }; // struct IMDevice


    struct DIMTimerReader : IMDevice::AsyncCall {

        std::uint32_t value = 0;

        DIMTimerReader(IMDevice& dev);

        void
        read();

        void
        callback(IOSError e)
            override;

    }; // struct DIMTimerReader


    struct UNDIMEventWatcher : IMDevice::EventWatcher {

        UNDIMEventWatcher(IMDevice& dev);

        void
        callback(IOSError e)
            override;

    }; // struct UNDIMEventWatcher


    std::string
    to_string(IMDevice::AsyncCall::State st);

} // namespace WiiUIMManager

#endif

#endif
