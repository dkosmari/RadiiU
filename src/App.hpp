/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef APP_HPP
#define APP_HPP

#include <filesystem>
#include <memory>
#include <vector>

#include <sdl2xx/sdl.hpp>
#include <sdl2xx/img.hpp>
#include <sdl2xx/ttf.hpp>

#include <imgui.h>

#include "Station.hpp"


struct App {

    std::filesystem::path content_path;

    sdl::init sdl_init;
    sdl::img::init img_init;
    sdl::ttf::init ttf_init;

    sdl::window window;
    sdl::renderer renderer;
    sdl::texture title_texture;

    sdl::vec2 window_size;

    sdl::vector<sdl::game_controller::device> controllers;

    bool running;


    App();
    ~App();


    void
    run();


    void
    quit();


    void
    draw();


    void
    process_events();

    void
    process_ui();


    void
    process();


    static App* instance;
    static App* get_instance();

};

#endif
