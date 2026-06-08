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
#include <iostream>
#include <optional>

#include "cfg.hpp"

#include "App.hpp"
#include "Serializer.hpp"


using std::cout;
using std::endl;
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
        load_defaults();
        load();
    }


    void
    finalize()
    {
        save();
    }


    void
    load()
    {
        try {
            auto filename = App::get_config_path() / "settings.json";
            Serializer::load(state, filename);
        }
        catch (std::exception& e) {
            cout << "Error loading settings: " << e.what() << endl;
        }
    }


    void
    save()
    {
        try {
            auto filename = App::get_config_path() / "settings.json";
            Serializer::save(state, filename);
        }
        catch (std::exception& e) {
            cout << "Error saving settings: " << e.what() << endl;
        }
    }

} // namespace cfg
