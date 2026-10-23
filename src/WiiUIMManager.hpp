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
#include <functional>

#include <coreinit/im.h>


namespace WiiUIMManager {

    // RAII type for IM handles
    struct IMDevice {

        const IOSHandle handle;

        IMDevice();

        ~IMDevice()
            noexcept;

    }; // struct IMDevice


    struct AsyncCall {

        enum class State : unsigned {
            idle,
            requesting,
            handling,
            shutdown_starting,
            shutdown_finishing,
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


    std::string
    to_string(AsyncCall::State st);


    struct EventWatcher : AsyncCall {

        using Function = std::move_only_function<void (IMEventMask)>;

        EventWatcher(IMDevice& dev,
                     IMEventMask ev,
                     Function f);

        ~EventWatcher()
            noexcept;

        void
        request_watch();

        void
        callback(IOSError e)
            override;


    protected:

        IMEventMask desired;
        IMEventMask event{};
        Function func;


        static
        void
        callback_helper(IOSError e,
                        void* ctx)
            noexcept;

    }; // struct EventWatcher


    struct DIMTimerReader : AsyncCall {

        DIMTimerReader(IMDevice& dev);

        void
        request_read();

        void
        callback(IOSError e)
            override;

        std::uint32_t
        get_value()
            const noexcept;


    private:

        std::uint32_t value = 0;

    }; // struct DIMTimerReader

} // namespace WiiUIMManager

#endif

#endif
