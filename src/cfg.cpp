/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdio>
#include <exception>
#include <iostream>
#include <filesystem>

#ifdef __WIIU__
#include <nn/act.h>
#include <nn/save.h>
#else
#include <wordexp.h>
#endif

#include "json.hpp"

#include "cfg.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;


namespace cfg {

    std::filesystem::path base_dir;

    std::string server;
    unsigned    player_buffer_size      = 8192;
    bool        disable_auto_power_down = true;
    unsigned    browser_page_size       = 20;


    namespace {
        std::filesystem::path
        get_user_config_path()
        {
#ifdef __WIIU__
            char buf[256];
            std::snprintf(buf, sizeof buf, "/vol/save/%08x", nn::act::GetPersistentId());
            return buf;
#else
            wordexp_t expanded{};
            if (wordexp("${XDG_CONFIG_HOME:-~/.config}", &expanded, WRDE_NOCMD)) {
                wordfree(&expanded);
                return ".";
            }
            std::filesystem::path config_dir = expanded.we_wordv[0];
            wordfree(&expanded);
            return config_dir / PACKAGE_NAME;
#endif
        }
    } // namespace


    void
    initialize()
    {
#ifdef __WIIU__
        nn::act::Initialize();
        base_dir = get_user_config_path();
        SAVEInit();
        if (!exists(base_dir)) {
            cout << "Creating save dir..." << endl;
            auto status = SAVEInitSaveDir(nn::act::GetSlotNo());
            if (status)
                cout << "SAVEInitSaveDir() returned " << int(status) << endl;
            else
                cout << "Save dir created." << endl;
        }
#else
        base_dir = get_user_config_path();
        if (!exists(base_dir)) {
            cout << "Creating " << base_dir << endl;
            if (!create_directory(base_dir))
                cout << "Could not create " << base_dir << endl;
        }
#endif

        load();
    }


    void
    finalize()
    {
        save();
#ifdef __WIIU__
        SAVEShutdown();
        nn::act::Finalize();
#else
#endif
    }


    void
    load()
    {
        try {
            auto root = json::load(base_dir / "settings.json").as<json::object>();

            if (root.contains("server"))
                server = root.at("server").as<json::string>();

            if (root.contains("player_buffer_size"))
                player_buffer_size = root.at("player_buffer_size").as<json::integer>();

            if (root.contains("disable_auto_power_down"))
                disable_auto_power_down = root.at("disable_auto_power_down").as<bool>();

            if (root.contains("browser_page_size"))
                browser_page_size = root.at("browser_page_size").as<json::integer>();
        }
        catch (std::exception& e) {
            cout << "Could not load settings: " << e.what() << endl;
        }
    }


    void
    save()
    {
        try {
            json::object root;
            if (!server.empty())
                root["server"] = server;
            root["player_buffer_size"]      = player_buffer_size;
            root["disable_auto_power_down"] = disable_auto_power_down;
            root["browser_page_size"]       = browser_page_size;

            std::filesystem::path old_settings = base_dir / "settings.json";
            std::filesystem::path new_settings = base_dir / "settings.json.new";
#ifdef __WIIU__
            if (exists(new_settings))
                remove(new_settings);
#endif
            json::save(root, new_settings);
#ifdef __WIIU__
            if (exists(old_settings))
                remove(old_settings);
#endif
            rename(new_settings, old_settings);
        }
        catch (std::exception& e) {
            cout << "Error saving settings: " << e.what() << endl;
        }
    }

} // namespace cfg
