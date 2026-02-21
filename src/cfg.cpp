/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <concepts>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>

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
using std::string;

namespace cfg {

    std::filesystem::path base_dir;

    unsigned browser_page_limit;
    bool     disable_apd;
    bool     disable_swkbd;
    bool     inactive_screen_off;
    TabID    initial_tab;
    unsigned player_buffer_size;
    unsigned player_history_limit;
    bool     remember_tab;
    unsigned recent_limit;
    unsigned screen_saver_timeout;
    bool     send_clicks;
    string   server;


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
            return config_dir / PACKAGE;
#endif
    }


    void
    load_defaults()
    {
        browser_page_limit   = 20;
        disable_apd          = true;
        disable_swkbd        = false;
        inactive_screen_off  = false;
        initial_tab          = TabID::browser;
        player_buffer_size   = 8;
        player_history_limit = 20;
        recent_limit         = 10;
        remember_tab         = true;
        screen_saver_timeout = 120;
        send_clicks          = false;
        server.clear();
    }


    void
    initialize()
    {
        load_defaults();

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
#endif
    }


    void
    load()
    {
        try {
            auto root = json::load(base_dir / "settings.json").as<json::object>();

            try_get(root, "browser_page_limit",   browser_page_limit);
            try_get(root, "disable_apd",          disable_apd);
            try_get(root, "disable_swkbd",        disable_swkbd);
            try_get(root, "inactive_screen_off",  inactive_screen_off);
            if (auto v = try_get<json::string>(root, "initial_tab"))
                initial_tab = TabID::from_string(*v);
            try_get(root, "player_buffer_size",   player_buffer_size);
            try_get(root, "player_history_limit", player_history_limit);
            try_get(root, "recent_limit",         recent_limit);
            try_get(root, "remember_tab",         remember_tab);
            try_get(root, "screen_saver_timeout", screen_saver_timeout);
            try_get(root, "send_clicks",          send_clicks);
            try_get(root, "server",               server);

            // TODO: remove after 0.2
            if (player_buffer_size > 64)
                player_buffer_size = 64;
        }
        catch (std::exception& e) {
            cout << "Error loading settings: " << e.what() << endl;
        }
    }


    void
    save()
    {
        try {
            json::object root;

            root["browser_page_limit"]   = browser_page_limit;
            root["disable_apd"]          = disable_apd;
            root["disable_swkbd"]        = disable_swkbd;
            root["inactive_screen_off"]  = inactive_screen_off;
            root["initial_tab"]          = to_string(initial_tab);
            root["player_buffer_size"]   = player_buffer_size;
            root["player_history_limit"] = player_history_limit;
            root["recent_limit"]         = recent_limit;
            root["remember_tab"]         = remember_tab;
            root["screen_saver_timeout"] = screen_saver_timeout;
            root["send_clicks"]          = send_clicks;
            root["server"]               = server;

            json::save(std::move(root), base_dir / "settings.json");
        }
        catch (std::exception& e) {
            cout << "Error saving settings: " << e.what() << endl;
        }
    }

} // namespace cfg
