/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <functional>
#include <exception>

#include "StationClicking.hpp"

#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "Settings.hpp"
#include "tracer.hpp"


using Settings::cfg;


namespace StationClicking {

    namespace {

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        handle_click_exception(const ConstStationPtr& station,
                               const std::exception& e);

        void
        handle_click_result(const ConstStationPtr& station,
                            RadioBrowserAPI::ClickResult result);

        void
        handle_click_update(const ConstStationPtr& station,
                            RadioBrowserAPI::Station rb_station);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        handle_click_exception(const ConstStationPtr& station,
                               const std::exception& e)
        {
            TRACE_FUNC;

            LOG_ERROR("Click processing for {:?} failed: {}",
                      station->stationuuid,
                      e.what());
        }


        void
        handle_click_result(const ConstStationPtr& station,
                            RadioBrowserAPI::ClickResult result)
        {
            TRACE_FUNC;

            LOG_INFO("Click result for {}: ok={}",
                     result.stationuuid,
                     result.ok);

            // Update clicks fields on success
            if (result.ok)
                RadioBrowserAPI::get_station(station->stationuuid,
                                             std::bind_front(handle_click_update, station),
                                             std::bind_front(handle_click_exception, station));
        }


        void
        handle_click_update(const ConstStationPtr& station,
                            RadioBrowserAPI::Station rb_station)
        {
            TRACE_FUNC;
            station->click_count = rb_station.clickcount;
            station->click_trend = rb_station.clicktrend;
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    click(const ConstStationPtr& station)
    {
        TRACE_FUNC;

        if (!cfg.send_clicks)
            return;

        if (!station || station->stationuuid.empty())
            return;

        RadioBrowserAPI::send_click(station->stationuuid,
                                    std::bind_front(handle_click_result, station),
                                    std::bind_front(handle_click_exception, station));
    }

} // namespace StationClicking
