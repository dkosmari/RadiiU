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

#include "LogManager.hpp"
#include "string_utils.hpp"


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


        std::string
        im_event_to_string(unsigned mask)
        {
            using string_utils::concat;

            std::string result;
            const std::string separator = ",";

            if (mask & IM_EVENT_ACTIVE)
                result = concat(result, "active", separator);
            if (mask & IM_EVENT_INACTIVE)
                result = concat(result, "inactive", separator);
            if (mask & IM_EVENT_DIM)
                result = concat(result, "dim", separator);
            if (mask & IM_EVENT_UNDIM)
                result = concat(result, "undim", separator);
            if (mask & IM_EVENT_APD)
                result = concat(result, "apd", separator);
            if (mask & IM_EVENT_POWER)
                result = concat(result, "power", separator);
            if (mask & IM_EVENT_HOME)
                result = concat(result, "home", separator);
            if (mask & IM_EVENT_SYNC)
                result = concat(result, "sync", separator);
            if (mask & IM_EVENT_RESET)
                result = concat(result, "reset", separator);
            if (mask & IM_EVENT_CANCELLED)
                result = concat(result, "cancelled", separator);

            return result;
        }

    } // namespace


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


    AsyncCall::AsyncCall(IMDevice& dev) :
        device(dev)
    {}


    AsyncCall::~AsyncCall()
        noexcept
    {
        State old_state = State::idle;
        if (!state.compare_exchange_strong(old_state, State::shutdown_finishing)) {
            LOG_WARN("shutting down while async call active: state = {}",
                     to_string(old_state));
            // !idle -> shutdown_starting
            state = State::shutdown_starting;
            auto start = std::chrono::steady_clock::now();
            while ((old_state = state) != State::shutdown_finishing) {
                std::this_thread::sleep_for(5ms);
                auto now = std::chrono::steady_clock::now();
                if (now - start > 100ms) {
                    LOG_ERROR("timed out waiting for shutdown_finishing state: state is {}",
                              to_string(old_state));
                    break;
                }
            }
        }
    }


    void
    AsyncCall::callback(IOSError)
    {}


    void
    AsyncCall::callback_helper(IOSError e,
                               void* ctx)
        noexcept
    {
        auto self = reinterpret_cast<AsyncCall*>(ctx);

        State old_state;

        if (self->finish_shutdown())
            return;

        // requesting -> handling
        old_state = State::requesting;
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
    AsyncCall::finish_shutdown()
        noexcept
    {
        State old_state = State::shutdown_starting;
        if (state.compare_exchange_strong(old_state, State::shutdown_finishing))
            return true;
        // Shouldn't happen, but just to be safe...
        if (old_state == State::shutdown_finishing) {
            LOG_ERROR("finish_shutdown() called multiple times!");
            return true;
        }
        return false;
    }


    std::string
    to_string(AsyncCall::State st)
    {
        switch (st) {
            using enum AsyncCall::State;

            case idle:
                return "idle"s;

            case requesting:
                return "requesting"s;

            case handling:
                return "handling"s;

            case shutdown_starting:
                return "shutdown_starting"s;

            case shutdown_finishing:
                return "shutdown_finishing"s;

            default:
                return "?";
        }
    }


    EventWatcher::EventWatcher(IMDevice& dev,
                               IMEventMask ev,
                               Function f) :
        AsyncCall{dev},
        desired{ev},
        func{std::move(f)}
    {
        request_watch();
    }


    EventWatcher::~EventWatcher()
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
    EventWatcher::request_watch()
    {
        State old_state;

        // idle -> requesting
        old_state = State::idle;
        if (!state.compare_exchange_strong(old_state, State::requesting)) {
            LOG_WARN("EventWatcher::request_watch(): could not enter requesting state: old_state is {}",
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
            // requesting -> idle
            old_state = State::requesting;
            if (!state.compare_exchange_strong(old_state, State::idle)) {
                LOG_WARN("could not enter idle state: old_state is {}",
                         to_string(old_state));
                if (finish_shutdown())
                    return;
            }
            throw std::runtime_error{"IM_GetEventNotify() failed: "s
                                     + to_string(e)};
        }
    }


    void
    EventWatcher::callback(IOSError e)
    {
        AsyncCall::callback(e);

        LOG_DEBUG("im event watcher: {}", im_event_to_string(event));

        if (!e)
            if (func)
                func(event);
    }


    void
    EventWatcher::callback_helper(IOSError e,
                                  void* ctx)
        noexcept
    {
        auto self = reinterpret_cast<EventWatcher*>(ctx);

        if (self->finish_shutdown())
            return;

        AsyncCall::callback_helper(e, static_cast<AsyncCall*>(self));

        try {
            if (!e)
                self->request_watch(); // watch again
        }
        catch (std::exception& e) {
            LOG_ERROR("re-watch failed: {}", e.what());
        }
        catch (...) {
            LOG_ERROR("re-watch failed (unknown exception)");
        }
    }


    DIMTimerReader::DIMTimerReader(IMDevice& dev) :
        AsyncCall{dev}
    {}


    void
    DIMTimerReader::request_read()
    {
        State old_state;
        // idle -> requesting
        old_state = State::idle;
        if (!state.compare_exchange_strong(old_state, State::requesting))
            return;

        auto e = IM_GetTimerRemaining(device.handle,
                                      &request,
                                      IM_TIMER_DIM,
                                      &value,
                                      &AsyncCall::callback_helper,
                                      static_cast<AsyncCall*>(this));
        if (e)
            throw std::runtime_error{"IM_GetTimerRemaining() failed: "s + to_string(e)};
    }


    void
    DIMTimerReader::callback(IOSError e)
    {
        AsyncCall::callback(e);
        if (e)
            LOG_ERROR("DIMTimerReader: got error: {}", to_string(e));
    }


    std::uint32_t
    DIMTimerReader::get_value()
        const noexcept
    {
        return value;
    }

} // namespace WiiUIMManager

#endif // __WIIU__
