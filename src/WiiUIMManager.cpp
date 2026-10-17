/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef __WIIU__

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "WiiUIMManager.hpp"

#include "App.hpp"
#include "LogManager.hpp"


using namespace std::literals;


namespace WiiUIMManager {

    namespace {

        std::string
        to_string(IOSError e)
        {
            switch (e) {
                case IOS_ERROR_OK:
                    return "OK"s;
                case IOS_ERROR_ACCESS:
                    return "ACCESS"s;
                case IOS_ERROR_EXISTS:
                    return "EXISTS"s;
                case IOS_ERROR_INTR:
                    return "INTR"s;
                case IOS_ERROR_INVALID:
                    return "INVALID"s;
                case IOS_ERROR_MAX:
                    return "MAX"s;
                case IOS_ERROR_NOEXISTS:
                    return "NOEXISTS"s;
                case IOS_ERROR_QEMPTY:
                    return "QEMPTY"s;
                case IOS_ERROR_QFULL:
                    return "QFULL"s;
                case IOS_ERROR_UNKNOWN:
                    return "UNKNOWN"s;
                case IOS_ERROR_NOTREADY:
                    return "NOTREADY"s;
                case IOS_ERROR_ECC:
                    return "ECC"s;
                case IOS_ERROR_ECCCRIT:
                    return "ECCCRIT"s;
                case IOS_ERROR_BADBLOCK:
                    return "BADBLOCK"s;
                case IOS_ERROR_INVALIDOBJTYPE:
                    return "INVALIDOBJTYPE"s;
                case IOS_ERROR_INVALIDRNG:
                    return "INVALIDRNG"s;
                case IOS_ERROR_INVALIDFLAG:
                    return "INVALIDFLAG"s;
                case IOS_ERROR_INVALIDFORMAT:
                    return "INVALIDFORMAT"s;
                case IOS_ERROR_INVALIDVERSION:
                    return "INVALIDVERSION"s;
                case IOS_ERROR_INVALIDSIGNER:
                    return "INVALIDSIGNER"s;
                case IOS_ERROR_FAILCHECKVALUE:
                    return "FAILCHECKVALUE"s;
                case IOS_ERROR_FAILINTERNAL:
                    return "FAILINTERNAL"s;
                case IOS_ERROR_FAILALLOC:
                    return "FAILALLOC"s;
                case IOS_ERROR_INVALIDSIZE:
                    return "INVALIDSIZE"s;
                case IOS_ERROR_NOLINK:
                    return "NOLINK"s;
                case IOS_ERROR_ANFAILED:
                    return "ANFAILED"s;
                case IOS_ERROR_MAXSEMCOUNT:
                    return "MAXSEMCOUNT"s;
                case IOS_ERROR_SEMUNAVAILABLE:
                    return "SEMUNAVAILABLE"s;
                case IOS_ERROR_INVALIDHANDLE:
                    return "INVALIDHANDLE"s;
                case IOS_ERROR_INVALIDARG:
                    return "INVALIDARG"s;
                case IOS_ERROR_NORESOURCE:
                    return "NORESOURCE"s;
                case IOS_ERROR_BUSY:
                    return "BUSY"s;
                case IOS_ERROR_TIMEOUT:
                    return "TIMEOUT"s;
                case IOS_ERROR_ALIGNMENT:
                    return "ALIGNMENT"s;
                case IOS_ERROR_BSP:
                    return "BSP"s;
                case IOS_ERROR_DATAPENDING:
                    return "DATAPENDING"s;
                case IOS_ERROR_EXPIRED:
                    return "EXPIRED"s;
                case IOS_ERROR_NOREADACCESS:
                    return "NOREADACCESS"s;
                case IOS_ERROR_NOWRITEACCESS:
                    return "NOWRITEACCESS"s;
                case IOS_ERROR_NOREADWRITEACCESS:
                    return "NOREADWRITEACCESS"s;
                case IOS_ERROR_CLIENTTXNLIMIT:
                    return "CLIENTTXNLIMIT"s;
                case IOS_ERROR_STALEHANDLE:
                    return "STALEHANDLE"s;
                case IOS_ERROR_UNKNOWNVALUE:
                    return "UNKNOWNVALUE"s;
                default:
                    return "<UNKNOWN: "s + std::to_string(std::to_underlying(e)) + ">"s;
            }
        }

    } // namespace


    IMDevice::AsyncCall::AsyncCall(IMDevice& dev) :
        device(dev)
    {}


    IMDevice::AsyncCall::~AsyncCall()
        noexcept
    {
        State old_state = State::idle;
        if (!state.compare_exchange_strong(old_state, State::shutdown_finish)) {
            LOG_WARN("shutting down while async call active: state = {}",
                     to_string(old_state));
            // !idle -> shutdown_start
            state = State::shutdown_start;
            auto start = std::chrono::steady_clock::now();
            while ((old_state = state) != State::shutdown_finish) {
                std::this_thread::sleep_for(5ms);
                auto now = std::chrono::steady_clock::now();
                if (now - start > 100ms) {
                    LOG_ERROR("timed out waiting for shutdown_finish state: state is {}",
                              to_string(old_state));
                    break;
                }
            }
        }
    }


    void
    IMDevice::AsyncCall::callback(IOSError)
    {}


    void
    IMDevice::AsyncCall::callback_helper(IOSError e,
                                         void* ctx)
        noexcept
    {
        auto self = reinterpret_cast<AsyncCall*>(ctx);

        State old_state;

        if (self->finish_shutdown())
            return;

        // waiting -> handling
        old_state = State::waiting;
        if (!self->state.compare_exchange_strong(old_state, State::handling)) {
            LOG_WARN("could not enter handling state: old_state was {}",
                     to_string(old_state));
            if (self->finish_shutdown())
                return;
        }

        try {
            self->callback(e);
        }
        catch (std::exception& e) {
            LOG_ERROR("callback failed: {}", e.what());
        }
        catch (...) {
            LOG_ERROR("callback failed (unknown exception)");
        }

        // handling -> idle
        old_state = State::handling;
        if (!self->state.compare_exchange_strong(old_state, State::idle))
            if (self->finish_shutdown())
                return;
    }


    bool
    IMDevice::AsyncCall::finish_shutdown()
        noexcept
    {
        State old_state = State::shutdown_start;
        if (state.compare_exchange_strong(old_state, State::shutdown_finish))
            return true;
        // Shouldn't happen, but just to be safe...
        if (old_state == State::shutdown_finish) {
            LOG_ERROR("finish_shutdown() called multiple times!");
            return true;
        }
        return false;
    }


    IMDevice::EventWatcher::EventWatcher(IMDevice& dev,
                                         IMEventMask ev) :
        AsyncCall{dev},
        desired{ev}
    {
        watch();
    }


    IMDevice::EventWatcher::~EventWatcher()
        noexcept
    {
        alignas(0x40) IMRequest cancel_req{};
        IM_CancelGetEventNotify(device.handle,
                                &cancel_req,
                                nullptr,
                                nullptr);

        state = State::idle;
    }


    void
    IMDevice::EventWatcher::watch()
    {
        State old_state;

        // idle -> started
        old_state = State::idle;
        if (!state.compare_exchange_strong(old_state, State::started)) {
            LOG_WARN("could not enter started state: old_state is {}",
                     to_string(old_state));
            if (finish_shutdown())
                return;
        }

        event = desired;
        if (auto e = IM_GetEventNotify(device.handle,
                                       &request,
                                       &event,
                                       &EventWatcher::callback_helper,
                                       this)) {
            // started -> idle
            old_state = State::started;
            if (!state.compare_exchange_strong(old_state, State::idle)) {
                LOG_WARN("could not enter idle state: old_state is {}",
                         to_string(old_state));
                if (finish_shutdown())
                    return;
            }
            throw std::runtime_error{"IM_GetEventNotify() failed: "s
                                     + to_string(e)};
        }

        // started -> waiting
        old_state = State::started;
        // NOTE: this can fail if we entered shutdown, don't override the shutdown state.
        if (!state.compare_exchange_strong(old_state, State::waiting)) {
            LOG_WARN("could not enter waiting state: old_state is {}",
                     to_string(old_state));
            if (finish_shutdown())
                return;
        }
    }


    void
    IMDevice::EventWatcher::callback_helper(IOSError e,
                                            void* ctx)
        noexcept
    {
        auto self = reinterpret_cast<EventWatcher*>(ctx);

        if (self->finish_shutdown())
            return;

        IMDevice::AsyncCall::callback_helper(e, static_cast<AsyncCall*>(self));

        try {
            if (!e)
                self->watch();
        }
        catch (std::exception& e) {
            LOG_ERROR("re-watch failed: {}", e.what());
        }
        catch (...) {
            LOG_ERROR("re-watch failed (unknown exception)");
        }
    }


    IMDevice::IMDevice() :
        handle{IM_Open()}
    {
        if (handle < 0)
            throw std::runtime_error{"IM_Open() failed: "s
                                     + std::to_string(handle)};
    }


    IMDevice::~IMDevice()
    {
        IM_Close(handle);
    }


    DIMTimerReader::DIMTimerReader(IMDevice& dev) :
        IMDevice::AsyncCall{dev}
    {}


    void
    DIMTimerReader::read()
    {
        State old_state;
        // idle -> started
        old_state = State::idle;
        if (!state.compare_exchange_strong(old_state, State::started)) {
            LOG_WARN("could not enter started state: old_state is {}",
                     to_string(old_state));
            return;
        }

        auto e = IM_GetTimerRemaining(device.handle,
                                      &request,
                                      IM_TIMER_DIM,
                                      &value,
                                      &IMDevice::AsyncCall::callback_helper,
                                      static_cast<IMDevice::AsyncCall*>(this));
        if (e)
            throw std::runtime_error{"IM_GetTimerRemaining() failed: "s + to_string(e)};

        // started -> waiting
        old_state = State::started;
        if (!state.compare_exchange_strong(old_state, State::waiting)) {
            LOG_WARN("could not enter waiting state: old_state is {}",
                     to_string(old_state));
            if (finish_shutdown())
                return;
        }
    }


    void
    DIMTimerReader::callback(IOSError e)
    {
        IMDevice::AsyncCall::callback(e);
        if (e)
            LOG_ERROR("DIMTimerReader: got error: {}", to_string(e));
    }


    UNDIMEventWatcher::UNDIMEventWatcher(IMDevice& dev) :
        IMDevice::EventWatcher{dev, IM_EVENT_UNDIM}
    {}


    void
    UNDIMEventWatcher::callback(IOSError e)
    {
        IMDevice::EventWatcher::callback(e);
        if (!e) {
            if (event & IM_EVENT_UNDIM) {
                LOG_DEBUG("undim event");
                App::add_task(
                    "undim event"sv,
                    []
                    {
                        App::detected_activity();
                    }
                );
            }
        }
    }


    std::string
    to_string(IMDevice::AsyncCall::State st)
    {
        switch (st) {
            using enum IMDevice::AsyncCall::State;

            case idle:
                return "idle"s;

            case started:
                return "started"s;

            case waiting:
                return "waiting"s;

            case handling:
                return "handling"s;

            case shutdown_start:
                return "shutdown_start"s;

            case shutdown_finish:
                return "shutdown_finish"s;

            default:
                return "?";
        }
    }

} // namespace WiiUIMManager

#endif // __WIIU__
