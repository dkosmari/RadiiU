/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <exception>
#include <iostream>

#include <glaze/core/meta.hpp>

#include "Settings.hpp"

#include "App.hpp"
#include "LogManager.hpp"
#include "Resolution.hpp"
#include "Serializer.hpp"
#include "TabIDGlaze.hpp"
#include "tracer.hpp"


using namespace std::literals;

using std::cerr;
using std::endl;
using std::string;


namespace Settings {

    namespace {

        /*-----------*/
        /* Variables */
        /*-----------*/

        Cfg last_cfg;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        load()
            noexcept;

        void
        save()
            noexcept;


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        load()
            noexcept
        {
            TRACE_FUNC;

            try {
                auto filename = App::get_config_dir() / "settings.json";
                Serializer::load(cfg, filename);
                last_cfg = cfg;
            }
            catch (std::exception& e) {
                LOG_ERROR("Loading settings.json: {}", e.what());
            }
        }


        void
        save()
            noexcept
        {
            TRACE_FUNC;

            try {
                auto filename = App::get_config_dir() / "settings.json";
                Serializer::save(cfg, filename);
                last_cfg = cfg;
            }
            catch (std::exception& e) {
                if (App::is_running())
                    LOG_ERROR("Saving settings.json: {}", e.what());
                else
                    cerr << "Error saving settings.json: " << e.what() << endl;
            }
        }


    } // namespace


    Cfg cfg;


    void
    initialize()
    {
        TRACE_FUNC;

        last_cfg = cfg = {};
        load();
    }


    void
    finalize()
    {
        TRACE_FUNC;

        if (cfg != last_cfg)
            save();
    }


    void
    reset()
        noexcept
    {
        last_cfg = cfg = {};
        save();
    }

} // namespace Settings
