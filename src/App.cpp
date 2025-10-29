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
#include <coreinit/energysaver.h>
#include <coreinit/memory.h>
#include <vpad/input.h>
#endif

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <misc/freetype/imgui_freetype.h>

#include <sdl2xx/sdl.hpp>
#include <sdl2xx/img.hpp>

#include "App.hpp"

#include "About.hpp"
#include "Browser.hpp"
#include "cfg.hpp"
#include "Favorites.hpp"
#include "IconsFontAwesome4.h"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "Player.hpp"
#include "Recent.hpp"
#include "rest.hpp"
#include "Settings.hpp"
#include "TabID.hpp"
#include "ui.hpp" // DEBUG
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

        sdl::window window;
        sdl::renderer renderer;

        sdl::vector<sdl::game_controller::device> controllers;

    }; // struct Resources

    std::optional<Resources> res;


    bool running;

    const float cafe_size = 32;
    const float ui_rounding = 8;

    std::optional<TabID> next_tab;
    TabID current_tab;

#ifdef __WIIU__
    bool old_disable_swkbd;
#endif


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

// For desktop build: comment this to load Ubuntu-R.ttf from the current dir.
#define USE_CAFE

#ifdef USE_CAFE
        // Note: CafeStd seems to always be too low, about 1/8th of the font size
        const float correct_y = cafe_size * (4.0f / 32.0f);
#else
        const float correct_y = 0;
#endif

        // Load main font: CafeStd
        {
            ImFontConfig config;
            config.EllipsisChar = U'…';
            config.GlyphOffset.y = - correct_y;
#ifdef __WIIU__
            config.FontDataOwnedByAtlas = false;
            void* cafe_font_ptr = nullptr;
            uint32_t cafe_font_size = 0;
            if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD,
                                0,
                                &cafe_font_ptr,
                                &cafe_font_size)) {
                if (!io.Fonts->AddFontFromMemoryTTF(cafe_font_ptr,
                                                    cafe_font_size,
                                                    cafe_size,
                                                    &config))
                    throw std::runtime_error{"Could not load font!"};
            } else
                throw std::runtime_error{"Is CafeStd font missing?"};
#else // !__WIIU__
#  ifdef USE_CAFE
            if (!io.Fonts->AddFontFromFileTTF("CafeStd.ttf",
                                              cafe_size,
                                              &config))
                throw std::runtime_error{"Could not load font!"};
#  else // !USE_CAFE
            if (!io.Fonts->AddFontFromFileTTF("Ubuntu-R.ttf",
                                              cafe_size,
                                              &config))
                throw std::runtime_error{"Could not load font!"};
#  endif
#endif
        }


        // Load FontAwesome
        {
            ImFontConfig config;
            config.GlyphOffset.y = - correct_y;
            config.MergeMode = true;
            // config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
            // config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;

            if (!io.Fonts->AddFontFromFileTTF((utils::get_content_path() / FONT_ICON_FILE_NAME_FA).c_str(),
                                              cafe_size,
                                              &config))
                throw std::runtime_error{"Could not load font!"};
        }

    }


    ImGuiTabItemFlags
    get_tab_item_flags_for(TabID tab)
    {
        ImGuiTabItemFlags result = ImGuiTabItemFlags_None;
        // If switching tabs, set the SetSelected flag
        if (next_tab && tab == *next_tab)
            result |= ImGuiTabItemFlags_SetSelected;
        return result;
    }


    void
    setup_imgui_colors()
    {
        auto colors = ImGui::GetStyle().Colors;

        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.04f, 0.01f, 0.01f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.94f, 0.00f, 0.00f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.63f, 0.25f, 0.00f, 0.45f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.97f, 0.45f, 0.12f, 0.40f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.97f, 0.45f, 0.12f, 0.67f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.00f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.66f, 0.30f, 0.07f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.65f, 0.30f, 0.07f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.53f, 0.25f, 0.07f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.49f, 0.54f, 0.00f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.69f, 0.76f, 0.00f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.90f, 1.00f, 0.00f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.71f, 0.78f, 0.01f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.42f, 0.47f, 0.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.90f, 1.00f, 0.00f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.42f, 0.47f, 0.00f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.48f, 0.53f, 0.00f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.68f, 0.76f, 0.00f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.88f, 0.00f, 0.00f, 0.31f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.94f, 0.00f, 0.00f, 0.80f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.94f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.94f, 0.00f, 0.00f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.94f, 0.00f, 0.00f, 0.78f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.94f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.94f, 0.00f, 0.00f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.94f, 0.00f, 0.00f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.94f, 0.00f, 0.00f, 0.95f);
        colors[ImGuiCol_InputTextCursor]        = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.71f, 0.78f, 0.01f, 0.80f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.29f, 0.32f, 0.00f, 0.86f);
        colors[ImGuiCol_TabSelected]            = ImVec4(0.48f, 0.53f, 0.00f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.52f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TabDimmed]              = ImVec4(0.71f, 0.78f, 0.01f, 0.97f);
        colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.71f, 0.78f, 0.01f, 1.00f);
        colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.94f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.04f, 0.01f, 0.01f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        colors[ImGuiCol_TextLink]               = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.24f, 0.00f, 0.00f, 0.68f);
        colors[ImGuiCol_TreeLines]              = ImVec4(0.97f, 0.45f, 0.12f, 0.50f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 0.83f, 0.00f, 0.90f);
        colors[ImGuiCol_UnsavedMarker]          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_NavCursor]              = ImVec4(0.50f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    }


    void
    setup_imgui_style()
    {
        setup_imgui_colors();

        auto& style = ImGui::GetStyle();

        const ImVec2 padding = {9, 9};
        const float rounding = ui_rounding;
        const ImVec2 spacing = {9, 9};

        // style.Alpha = 0.5f;

        style.WindowPadding = padding;
        style.WindowRounding = 0;
        style.WindowBorderSize = 0;

        style.ChildRounding = rounding;
        style.ChildBorderSize = 0;

        style.PopupRounding = rounding;

        style.FramePadding = padding;
        style.FrameRounding = rounding;
        style.FrameBorderSize = 0;

        style.ItemSpacing = spacing;
        style.ItemInnerSpacing = spacing;

        style.CellPadding = {padding.x, padding.y / 2};

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
        next_tab = cfg::initial_tab;
        if (cfg::remember_tab)
            cfg::initial_tab = TabID::last_active;

#ifdef __WIIU__
        old_disable_swkbd = cfg::disable_swkbd;
        if (cfg::disable_swkbd)
            SDL_WiiUSetSWKBDEnabled(SDL_FALSE);
#endif

        res.emplace();

        // Create a temporary audio device to stop the boot sound.
        sdl::audio::spec aspec;
        aspec.freq = 48000;
        aspec.format = AUDIO_S16SYS;
        aspec.channels = 2;
        aspec.samples = 2048;
        sdl::audio::device adev{nullptr, false, aspec};

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "2");

        res->window.create(PACKAGE_STRING,
                           sdl::window::pos_centered,
                           {1280, 720},
                           0);

        res->renderer.create(res->window,
                             -1,
                             sdl::renderer::flag::accelerated,
                             sdl::renderer::flag::present_vsync);
        res->renderer.set_logical_size(res->window.get_size());

        initialize_imgui();

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
        cfg::remember_tab = cfg::initial_tab == TabID::last_active;
        if (cfg::remember_tab)
            cfg::initial_tab = current_tab;
        cfg::finalize();

        res.reset();

        cout << "Finished App::finalize()" << endl;
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
        cout << "Finished App::run()" << endl;
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

        ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::ToVec2(res->window.get_size()), ImGuiCond_Always);
        if (ImGui::Begin(PACKAGE_STRING,
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoResize)) {

            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui_rounding);

            {
                // App name, centered
                ImGui::PushFont(nullptr, 48);
                ImGui::TextCentered("%s", PACKAGE_STRING);
                ImGui::PopFont();
                // ui::show_last_bounding_box();
                ImGui::SameLine();
                // Put a close button on the top right
                auto tex = IconManager::get("ui/close-button.png");
                auto tex_size = ImGui::ToVec2(tex->get_size());
                ImVec2 close_button_size = tex_size
                    + 2 * (style.FramePadding + style.FrameBorderSize * ImVec2{1, 1});
                ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x
                                     - close_button_size.x);

                if (ImGui::ImageButton("close_button", *tex))
                    quit();
            }

            if (ImGui::BeginTabBar("main_tabs")) {

                if (ImGui::BeginTabItem(to_ui_string(TabID::favorites),
                                        nullptr,
                                        get_tab_item_flags_for(TabID::favorites))) {
                    current_tab = TabID::favorites;
                    Favorites::process_ui();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(to_ui_string(TabID::browser),
                                        nullptr,
                                        get_tab_item_flags_for(TabID::browser))) {
                    current_tab = TabID::browser;
                    Browser::process_ui();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(to_ui_string(TabID::recent),
                                        nullptr,
                                        get_tab_item_flags_for(TabID::recent))) {
                    current_tab = TabID::recent;
                    Recent::process_ui();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(to_ui_string(TabID::player),
                                        nullptr,
                                        get_tab_item_flags_for(TabID::player))) {
                    current_tab = TabID::player;
                    Player::process_ui();
                    ImGui::EndTabItem();

                }

                if (ImGui::BeginTabItem(to_ui_string(TabID::settings),
                                        nullptr,
                                        get_tab_item_flags_for(TabID::settings))) {
                    current_tab = TabID::settings;
                    Settings::process_ui();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem(to_ui_string(TabID::about),
                                        nullptr,
                                        get_tab_item_flags_for(TabID::about))) {
                    current_tab = TabID::about;
                    About::process_ui();
                    ImGui::EndTabItem();
                }

                next_tab.reset();

                ImGui::EndTabBar();
            }

            ImGui::PopStyleVar(2);
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
#ifdef __WIIU__
        if (old_disable_swkbd != cfg::disable_swkbd) {
            SDL_WiiUSetSWKBDEnabled(cfg::disable_swkbd ? SDL_FALSE : SDL_TRUE);
            old_disable_swkbd = cfg::disable_swkbd;
        }

        if (cfg::inactive_screen_off) {
            // TODO: find out how to do it with TV also.
            IMError e;
            std::uint32_t dim_enabled = 0;
            e = IMIsDimEnabled(&dim_enabled);
            if (!e && dim_enabled) {
                static bool screen_is_off = false;
                std::uint32_t seconds_to_dim = 0;
                e = IMGetTimeBeforeDimming(&seconds_to_dim);
                if (!e) {
                    if (seconds_to_dim == 0) {
                        if (!screen_is_off) {
                            cout << "Screen dimming started, putting DRC on standby." << endl;
                            VPADSetLcdMode(VPAD_CHAN_0, VPAD_LCD_STANDBY);
                            screen_is_off = true;
                        }
                    } else {
                        if (screen_is_off) {
                            cout << "Screen dimming ended, turning DRC back on." << endl;
                            screen_is_off = false;
                            VPADSetLcdMode(VPAD_CHAN_0, VPAD_LCD_ON);
                        }
                    }
                } else {
                    cout << "IMGetTimeBeforeDimming() returned " << e << endl;
                    screen_is_off = false;
                }
            } // dim_enabled
        }
#endif

        process_events();
        if (!running)
            return;

        rest::process();

        Favorites::process_logic();
        Browser::process_logic();
        Recent::process_logic();
        Player::process_logic();

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
        res->renderer.set_color(sdl::color::black);
        res->renderer.clear();

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),
                                              res->renderer.data());

#ifdef __WIIU__
        // WORKAROUND: the Wii U SDL2 port does not update the clipping until the next
        // draw, so we need to draw a transparent point here to reset the GX2 state.
        res->renderer.set_color(sdl::color::transparent);
        res->renderer.draw_point(0, 0);
#endif

        res->renderer.present();
    }

} // namespace App
