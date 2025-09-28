/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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

    unsigned browser_page_size  = 20;
    bool     disable_apd        = true;
    TabIndex initial_tab        = TabIndex::browser;
    unsigned max_recent         = 10;
    unsigned player_buffer_size = 8192;
    bool     remember_tab       = true;
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
            return config_dir / PACKAGE_NAME;
#endif
    }


    template<typename T>
    std::optional<T>
    try_get(const json::object& obj,
            const string& key)
    {
        try {
            return obj.at(key).as<T>();
        }
        catch (...) {
            return {};
        }
    }


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
#endif
    }


    void
    load()
    {
        try {
            auto root = json::load(base_dir / "settings.json").as<json::object>();

            if (auto val = try_get<json::string>(root, "server"))
                server = *val;

            if (auto val = try_get<json::integer>(root, "player_buffer_size"))
                player_buffer_size = *val;

            if (auto val = try_get<bool>(root, "disable_apd"))
                disable_apd = *val;

            if (auto val = try_get<json::integer>(root, "browser_page_size"))
                browser_page_size = *val;

            if (auto val = try_get<json::string>(root, "initial_tab"))
                initial_tab = to_tab_index(*val);

            if (auto val = try_get<bool>(root, "remember_tab"))
                remember_tab = *val;
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
            if (!server.empty())
                root["server"] = server;
            root["player_buffer_size"] = player_buffer_size;
            root["disable_apd"]        = disable_apd;
            root["browser_page_size"]  = browser_page_size;
            root["initial_tab"]        = to_string(initial_tab);
            root["remember_tab"]       = remember_tab;
            json::save(std::move(root), base_dir / "settings.json");
        }
        catch (std::exception& e) {
            cout << "Error saving settings: " << e.what() << endl;
        }
    }

} // namespace cfg
