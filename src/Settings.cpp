/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <concepts>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>

#include <glaze/core/meta.hpp>

#include "Settings.hpp"

#include "App.hpp"
#include "LogManager.hpp"
#include "Resolution.hpp"
#include "Serializer.hpp"
#include "TabIDGlaze.hpp"
#include "tracer.hpp"


using std::string;


namespace Settings {

    Cfg cfg;
    Cfg last_cfg;


    void
    load_defaults()
    {
        cfg = {};
        last_cfg = cfg;
    }


    void
    initialize()
    {
        TRACE_FUNC;
        load_defaults();
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
    load()
    {
        TRACE_FUNC;

        try {
            auto filename = App::get_config_dir() / "settings.json";
            Serializer::load(cfg, filename);
            last_cfg = cfg;
        }
        catch (std::exception& e) {
            LOG_ERROR("load(): {}", e.what());
        }
    }


    void
    save()
    {
        TRACE_FUNC;

        try {
            auto filename = App::get_config_dir() / "settings.json";
            Serializer::save(cfg, filename);
            last_cfg = cfg;
        }
        catch (std::exception& e) {
            LOG_ERROR("save(): {}", e.what());
        }
    }

} // namespace Settings
