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

#include "cfg.hpp"

#include "App.hpp"
#include "LogManager.hpp"
#include "Serializer.hpp"
#include "TabIDGlaze.hpp"
#include "tracer.hpp"


using std::string;


namespace cfg {

    State state;

    void
    load_defaults()
    {
        state = {};
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
        save();
    }


    void
    load()
    {
        TRACE_FUNC;
        try {
            auto filename = App::get_config_path() / "settings.json";
            Serializer::load(state, filename);
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
            auto filename = App::get_config_path() / "settings.json";
            Serializer::save(state, filename);
        }
        catch (std::exception& e) {
            LOG_ERROR("save(): {}", e.what());
        }
    }

} // namespace cfg
