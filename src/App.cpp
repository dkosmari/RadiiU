/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>


#ifdef __WIIU__
#include <coreinit/memory.h>
#endif

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <misc/freetype/imgui_freetype.h>

#include <sdl2xx/sdl.hpp>
#include <sdl2xx/img.hpp>
#include <sdl2xx/ttf.hpp>

#include "App.hpp"

#include "About.hpp"
#include "Browser.hpp"
#include "cfg.hpp"
#include "Favorites.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "Player.hpp"
#include "Recent.hpp"
#include "rest.hpp"
#include "Settings.hpp"
#include "TabIndex.hpp"
#include "utils.hpp"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;

using std::filesystem::path;

using namespace sdl::literals;


namespace App {

    // RAII-managed resources are stored here.
    struct Resources {

        sdl::init sdl_init{sdl::init::flag::video,
                           sdl::init::flag::audio,
                           sdl::init::flag::game_controller};
        sdl::img::init img_init;
        sdl::ttf::init ttf_init;

        sdl::window window;
        sdl::renderer renderer;
        sdl::texture title_texture;

        sdl::vector<sdl::game_controller::device> controllers;

    }; // struct Resources

    std::optional<Resources> res;


    bool running;

    const float cafe_size = 32;
    const float symbola_size = cafe_size * 1.2f;

    std::optional<TabIndex> next_tab;
    TabIndex current_tab;

    sdl::vec2 window_size;


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


    void
    load_fonts()
    {
        auto& io = ImGui::GetIO();

        // Load main font: CafeStd
        {
            ImFontConfig font_config;
            // Note: CafeStd seems to always be too low
            font_config.GlyphOffset.y = - cafe_size * (5.0f / 32.0f);
#ifdef __WIIU__
            font_config.FontDataOwnedByAtlas = false;
            void* cafe_font_ptr = nullptr;
            uint32_t cafe_font_size = 0;
            if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD,
                                0,
                                &cafe_font_ptr,
                                &cafe_font_size)) {
                if (!io.Fonts->AddFontFromMemoryTTF(cafe_font_ptr,
                                                    cafe_font_size,
                                                    cafe_size,
                                                    &font_config))
                    throw std::runtime_error{"Could not load font!"};
            } else
                throw std::runtime_error{"Is CafeStd font missing?"};
#else
            if (!io.Fonts->AddFontFromFileTTF("CafeStd.ttf",
                                              cafe_size,
                                              &font_config))
                throw std::runtime_error{"Could not load font!"};
#endif
        }

        // Load Symbola font
        {
            ImFontConfig font_config;
            font_config.GlyphOffset.y = - symbola_size * (5.0f / 32.0f);
            font_config.MergeMode = true;
            // font_config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
            // font_config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;
            if (!io.Fonts->AddFontFromFileTTF((utils::get_content_path() / "Symbola.ttf").c_str(),
                                              symbola_size,
                                              &font_config))
                throw std::runtime_error{"Could not load font!"};
        }

    }


    ImGuiTabItemFlags
    get_tab_item_flags_for(TabIndex idx)
    {
        ImGuiTabItemFlags result = ImGuiTabItemFlags_None;
        // If switching tabs, set the SetSelected flag
        if (next_tab && idx == *next_tab)
            result |= ImGuiTabItemFlags_SetSelected;
        return result;
    }


    sdl::texture
    make_logo_texture(sdl::renderer& renderer)
    {
        sdl::ttf::font title_font;
#ifdef __WIIU__
        void* cafe_font_ptr = nullptr;
        uint32_t cafe_font_size = 0;
        if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD,
                            0,
                            &cafe_font_ptr,
                            &cafe_font_size)) {
            auto rw = SDL_RWFromMem(cafe_font_ptr, cafe_font_size);
            title_font.create(rw, true, 60);
        }
#else
        title_font.create("CafeStd.ttf", 60);
#endif
        auto title_img = title_font.render_blended(PACKAGE_STRING, sdl::color::white);
        return sdl::texture{renderer, title_img};
    }


    void
    setup_imgui_style()
    {
        ImGui::StyleColorsDark();

        auto& style = ImGui::GetStyle();
        const float rounding = 8;
        const ImVec2 padding = {9, 9};
        const ImVec2 spacing = {9, 9};

        // style.Alpha = 0.5f;

        style.WindowPadding = padding;
        style.WindowRounding = 0;
        style.WindowBorderSize = 0;

        style.ChildRounding = rounding;
        style.ChildBorderSize = 1;

        style.PopupRounding = rounding;

        style.FramePadding = padding;
        style.FrameRounding = rounding;
        style.FrameBorderSize = 0;

        style.ItemSpacing = spacing;
        style.ItemInnerSpacing = spacing;

        style.CellPadding = padding / 2;

        style.ScrollbarSize = 32;
        style.ScrollbarRounding = rounding;

        style.GrabMinSize = 32;
        style.GrabRounding = rounding;

        style.ImageBorderSize = 0;

        style.TabRounding = rounding;
        style.TabBorderSize = 0;

        style.TabBarBorderSize = 2;

    }


    void
    initialize_imgui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
        io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;

        io.LogFilename = nullptr; // don't save log
        io.IniFilename = nullptr; // don't save ini

        load_fonts();

        setup_imgui_style();

        ImGui_ImplSDL2_InitForSDLRenderer(res->window.data(),
                                          res->renderer.data());
        ImGui_ImplSDLRenderer2_Init(res->renderer.data());
    }


    void
    initialize()
    {
        // Note: initialize cfg module early.
        cfg::initialize();
        next_tab = cfg::start_tab;

        res.emplace();

        // Create a temporary audio device to stop the boot sound.
        sdl::audio::spec aspec;
        aspec.freq = 44100;
        aspec.format = AUDIO_S16SYS;
        aspec.channels = 2;
        aspec.samples = 2048;
        sdl::audio::device adev{nullptr, false, aspec};

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "2");

        res->window.create(PACKAGE,
                           sdl::window::pos_centered,
                           {1280, 720},
                           0);

        window_size = res->window.get_size();
        cout << "SDL window size: " << window_size << endl;

        res->renderer.create(res->window,
                             -1,
                             sdl::renderer::flag::accelerated,
                             sdl::renderer::flag::present_vsync);
        res->renderer.set_logical_size(window_size);

        initialize_imgui();

        res->title_texture = make_logo_texture(res->renderer);

        // Initialize modules.
        IconManager::initialize(res->renderer);
        rest::initialize(utils::get_user_agent());

        // Initialize tabs.
        Favorites::initialize();
        Browser::initialize();
        Recent::initialize();
        Player::initialize();

        cout << "Finished App::initialize()" << endl;
    }


    void
    finalize()
    {
        // Finalize tabs.
        Player::finalize();
        Recent::finalize();
        Browser::finalize();
        Favorites::finalize();

        // Finalize modules.
        rest::finalize();
        IconManager::finalize();

        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        // Finalize cfg module last.
        if (cfg::remember_last_tab)
            cfg::start_tab = current_tab;
        cfg::finalize();

        res.reset();

        cout << "finished App::finalize()" << endl;
    }


    void
    run()
    {
        running = true;

        while (running) {

            process();

            if (!running)
                break;

            draw();

        }
        cout << "Stopped running" << endl;
    }


    void
    process_events()
    {
        sdl::events::event event;
        while (sdl::events::poll(event)) {

            ImGui_ImplSDL2_ProcessEvent(&event);

            switch (event.type) {

                case sdl::events::type::e_quit:
                    quit();
                    break;

                case sdl::events::type::e_window:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            window_size.x = event.window.data1;
                            window_size.y = event.window.data2;
                            break;
                    }
                    break;

                case sdl::events::type::e_controller_device_added:
                {
                    auto gc = sdl::game_controller::device(event.cdevice.which);
                    cout << "Added controller: " << gc.get_name() << endl;
                    res->controllers.push_back(std::move(gc));
                    break;
                }

                case sdl::events::type::e_controller_device_removed:
                {
                    std::erase_if(res->controllers,
                                  [id=event.cdevice.which](sdl::game_controller::device& gc)
                                  {
                                      return id == gc.get_id();
                                  });
                    break;
                }
            } // switch (event.type)

        }
    }


    void
    process_ui()
    {
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();

        auto& style = ImGui::GetStyle();

        ImGui::SetNextWindowPos({0, 0},
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(window_size.x, window_size.y),
                                 ImGuiCond_Always);
        if (ImGui::Begin("##MainWindow",
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoResize)) {

            // Draw logo on top, centered
            auto window_width = ImGui::GetWindowSize().x;
            ImGui::ImageCentered(res->title_texture);
            ImGui::SameLine();
            // Put a button to the right of the logo.
            auto close_button_tex = IconManager::get("ui/close-button.png");
            ImGui::SetCursorPosX(window_width
                                 - close_button_tex->get_size().x
                                 - 2 * style.FramePadding.x
                                 - style.WindowPadding.x);
            if (ImGui::ImageButton("close_button", *close_button_tex))
                quit();

            if (ImGui::BeginTabBar("main_tabs")) {

                if (ImGui::BeginTabItem(to_ui_string(TabIndex::favorites),
                                        nullptr,
                                        get_tab_item_flags_for(TabIndex::favorites))) {
                    current_tab = TabIndex::favorites;
                    Favorites::process_ui();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(to_ui_string(TabIndex::browser),
                                        nullptr,
                                        get_tab_item_flags_for(TabIndex::browser))) {
                    current_tab = TabIndex::browser;
                    Browser::process_ui();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(to_ui_string(TabIndex::recent),
                                        nullptr,
                                        get_tab_item_flags_for(TabIndex::recent))) {
                    current_tab = TabIndex::recent;
                    Recent::process_ui();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(to_ui_string(TabIndex::player),
                                        nullptr,
                                        get_tab_item_flags_for(TabIndex::player))) {
                    current_tab = TabIndex::player;
                    Player::process_ui();
                    ImGui::EndTabItem();

                }

                if (ImGui::BeginTabItem(to_ui_string(TabIndex::settings),
                                        nullptr,
                                        get_tab_item_flags_for(TabIndex::settings))) {
                    current_tab = TabIndex::settings;
                    Settings::process_ui();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(to_ui_string(TabIndex::about),
                                        nullptr,
                                        get_tab_item_flags_for(TabIndex::about))) {
                    current_tab = TabIndex::about;
                    About::process_ui();
                    ImGui::EndTabItem();
                }

                next_tab.reset();

                ImGui::EndTabBar();
            }

        }

        ImGui::End();

        // ImGui::ShowStyleEditor();

        ImGui::EndFrame();
        ImGui::Render();

        ImGui::KineticScrollFrameEnd();
    }


    void
    process()
    {
        process_events();
        if (!running)
            return;

        rest::process();

        Favorites::process_logic();
        Browser::process_logic();
        Recent::process_logic();
        Player::process_logic();

        res->renderer.set_color(sdl::color::black);
        res->renderer.clear();
        process_ui();
    }


    void
    quit()
    {
        running = false;
    }


    void
    draw()
    {
        // res->renderer.set_color(sdl::color::black);
        // res->renderer.clear();

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),
                                              res->renderer.data());

#ifdef __WIIU__
        // WORKAROUND: the Wii U port does not update the clipping until the next draw
        res->renderer.set_color(sdl::color::transparent);
        res->renderer.draw_point(0, 0);
#endif

        res->renderer.present();
    }

} // namespace App
