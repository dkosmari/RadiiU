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
#include "tracer.hpp"
#include "utils.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;

using std::filesystem::path;

using namespace sdl::literals;


namespace App {

    namespace {

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

        const float default_font_size = 32;
        const float ui_rounding = 8;

        std::optional<TabID> next_tab;
        TabID current_tab;

#ifdef __WIIU__
        bool old_disable_swkbd;
        std::uint32_t old_dim_countdown;
#endif

        Uint64 last_activity;
        Uint64 fade_start;

        enum class State {
            normal,
            fading,
            screen_saver,
        };

        State state = State::normal;

        Uint64 fade_duration_ms = 5'000;

    } // namespace

    void
    quit();

    void
    draw();

    void
    process_events();

    void
    process_ui();

    void
    process_screen_saver();

    void
    process();


#ifdef __WIIU__
    void
    load_system_fonts()
    {
        auto& io = ImGui::GetIO();
        // Load main font: CafeStd
        ImFontConfig config;
        config.Flags |= ImFontFlags_NoLoadError;
        config.EllipsisChar = U'…';
        config.GlyphOffset.y = - default_font_size * (4.0f / 32.0f);
        config.FontDataOwnedByAtlas = false;

        void* font_data = nullptr;
        uint32_t font_size = 0;

        if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0,
                            &font_data, &font_size)) {
            if (!io.Fonts->AddFontFromMemoryTTF(font_data, font_size,
                                                default_font_size, &config))
                throw std::runtime_error{"could not load CafeStd"};
        } else
            throw std::runtime_error{"CafeStd font is missing"};

        config.MergeMode = true;
        if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_CHINESE, 0,
                            &font_data, &font_size)) {
            io.Fonts->AddFontFromMemoryTTF(font_data, font_size,
                                           default_font_size, &config);
        }
        if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_KOREAN, 0,
                            &font_data, &font_size)) {
            io.Fonts->AddFontFromMemoryTTF(font_data, font_size,
                                           default_font_size, &config);
        }
        if (OSGetSharedData(OS_SHAREDDATATYPE_FONT_TAIWANESE, 0,
                            &font_data, &font_size)) {
            io.Fonts->AddFontFromMemoryTTF(font_data, font_size,
                                           default_font_size, &config);
        }
    }
#else // !__WIIU__
    // native version
    void
    load_system_fonts()
    {
        auto& io = ImGui::GetIO();

        ImFontConfig config;
        config.EllipsisChar = U'…';
        config.Flags |= ImFontFlags_NoLoadError;

        // TODO: use fontconfig to find a font

#if 1
        // Note: CafeStd seems to always be too low, about 1/8th of the font size
        config.GlyphOffset.y = - default_font_size * (4.0f / 32.0f);
        if (!io.Fonts->AddFontFromFileTTF("CafeStd.ttf", default_font_size, &config))
            throw std::runtime_error{"could not load CafeStd"};
#else
        if (!io.Fonts->AddFontFromFileTTF("Ubuntu-R.ttf", default_font_size, &config))
            throw std::runtime_error{"could not load main font"};
#endif
    }
#endif // __WIIU__

    void
    load_fonts()
    {
        load_system_fonts();

        auto& io = ImGui::GetIO();
        // Load FontAwesome
        ImFontConfig config;
        config.GlyphOffset.y = - default_font_size * (4.0f / 32.0f);
        config.Flags |= ImFontFlags_NoLoadError;
        config.MergeMode = true;
        // config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
        // config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;

        std::filesystem::path font_path = utils::get_content_path() / FONT_ICON_FILE_NAME_FA;
        if (!io.Fonts->AddFontFromFileTTF(font_path.c_str(), default_font_size, &config))
            throw std::runtime_error{"could not load \"" + font_path.string() + "\""};
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
#if 0
        using ImGui::GetStyle;
        ImVec4* colors = GetStyle().Colors;
        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.10f, 0.00f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.00f, 0.41f, 0.00f, 0.42f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(1.00f, 0.91f, 0.79f, 0.40f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.45f, 0.54f, 0.35f, 0.67f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.31f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.70f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(1.00f, 0.57f, 0.00f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.73f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.72f, 0.00f, 0.00f, 0.83f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.79f, 0.00f, 0.00f, 0.74f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 0.57f, 0.00f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.79f, 0.00f, 0.00f, 0.74f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(1.00f, 0.57f, 0.00f, 1.00f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.72f, 0.09f, 0.09f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(1.00f, 0.57f, 0.00f, 1.00f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.72f, 0.09f, 0.11f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(1.00f, 0.57f, 0.00f, 1.00f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(1.00f, 0.00f, 0.00f, 0.95f);
        colors[ImGuiCol_InputTextCursor]        = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(1.00f, 0.57f, 0.00f, 1.00f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.57f, 0.00f, 0.00f, 0.86f);
        colors[ImGuiCol_TabSelected]            = ImVec4(0.85f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline]    = ImVec4(1.00f, 0.43f, 0.43f, 1.00f);
        colors[ImGuiCol_TabDimmed]              = ImVec4(0.32f, 0.22f, 0.19f, 0.97f);
        colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.32f, 0.22f, 0.19f, 1.00f);
        colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(1.00f, 0.57f, 0.00f, 0.10f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        colors[ImGuiCol_TextLink]               = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(1.00f, 0.00f, 0.00f, 0.55f);
        colors[ImGuiCol_TreeLines]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_DragDropTargetBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_UnsavedMarker]          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_NavCursor]              = ImVec4(1.00f, 0.57f, 0.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
#endif
    }


    void
    setup_imgui_style()
    {
        setup_imgui_colors();

        auto& style = ImGui::GetStyle();

        const ImVec2 padding = {9, 9};
        const float rounding = ui_rounding;
        const ImVec2 spacing = {9, 9};

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
        TRACE_FUNC;

        // Note: initialize cfg module early.
        cfg::initialize();
        next_tab = cfg::initial_tab;
        if (cfg::remember_tab)
            cfg::initial_tab = TabID::last_active;

#ifdef __WIIU__
        old_disable_swkbd = cfg::disable_swkbd;
        if (cfg::disable_swkbd) {
            SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
            // SDL_StartTextInput();
        }
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
    }


    void
    finalize()
    {
        TRACE_FUNC;

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
    }


    void
    run()
    {
        TRACE_FUNC;

        running = true;

        while (running) {

            process();

            if (!running)
                break;

            draw();

        }
    }


    void
    process_events()
    {
        // TRACE_FUNC;

        Uint64 now = SDL_GetTicks64();

        sdl::events::event event;
        while (sdl::events::poll(event)) {

            ImGui_ImplSDL2_ProcessEvent(&event);

            switch (sdl::events::type{event.type}) {

                case sdl::events::type::quit:
                    quit();
                    break;

                case sdl::events::type::controller_device_added: {
                    auto gc = sdl::game_controller::device(event.cdevice.which);
                    cout << "Added controller: " << gc.get_name() << endl;
                    res->controllers.push_back(std::move(gc));
                    last_activity = now;
                    break;
                }

                case sdl::events::type::controller_device_removed: {
                    std::erase_if(res->controllers,
                                  [id=event.cdevice.which](sdl::game_controller::device& gc)
                                  {
                                      return id == gc.get_id();
                                  });
                    last_activity = now;
                    break;
                }

                case sdl::events::type::controller_axis:
                case sdl::events::type::controller_down:
                case sdl::events::type::controller_up:
                case sdl::events::type::key_down:
                case sdl::events::type::key_up:
                case sdl::events::type::mouse_down:
                case sdl::events::type::mouse_motion:
                case sdl::events::type::mouse_up:
                case sdl::events::type::mouse_wheel:
                case sdl::events::type::text_editing:
                case sdl::events::type::text_editing_ext:
                case sdl::events::type::text_input:
                case sdl::events::type::will_enter_foreground:
                    last_activity = now;
                    break;

                case sdl::events::type::window:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_SHOWN:
                        case SDL_WINDOWEVENT_EXPOSED:
                        case SDL_WINDOWEVENT_RESTORED:
                        case SDL_WINDOWEVENT_FOCUS_GAINED:
                        case SDL_WINDOWEVENT_ENTER:
                            last_activity = now;
                            break;
                    }
                    break;

                default:
                    ;

            } // switch (event.type)

        } // while (sdl::events::poll())

    }


    void
    process_ui()
    {
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();

        if (state == State::normal || state == State::fading) {

            auto& style = ImGui::GetStyle();
            if (state == State::fading) {
                Uint64 now = SDL_GetTicks64();
                float ratio = 1.0f - (now - fade_start) / float(fade_duration_ms);
                if (ratio < 0)
                    ratio = 0;
                style.Alpha = ratio;
            } else {
                style.Alpha = 1.0f;
            }

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
                    ImGui::SameLine();
                    // Put a close button on the top right
                    auto tex = IconManager::get("ui/close-button.svg");
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

        }

        ImGui::EndFrame();
        ImGui::Render();

        ImGui::KineticScrollFrameEnd();
    }


    void
    process_screen_saver()
    {
        // TODO
    }


    void
    process()
    {
#ifdef __WIIU__
        if (old_disable_swkbd != cfg::disable_swkbd) {
            SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, cfg::disable_swkbd ? "0" : "1");
            old_disable_swkbd = cfg::disable_swkbd;
        }

        std::uint32_t dim_enabled = 0;
        IMError dim_error = IMIsDimEnabled(&dim_enabled);
        VPADLcdMode current_vpad_mode;
        VPADGetLcdMode(VPAD_CHAN_0, &current_vpad_mode);
        if (!dim_error && dim_enabled) {
            std::uint32_t dim_countdown = 0;
            dim_error = IMGetTimeBeforeDimming(&dim_countdown);
            if (!dim_error) {
                if (cfg::inactive_screen_off) {
                    // This is the logic to turn the gamepad LCD off when the system
                    // enters the dimmed state (screen burn-in protection.)

                    // TODO: find out how to do it with TV also.
                    if (dim_countdown == 0) {
                        if (current_vpad_mode != VPAD_LCD_STANDBY) {
                            cout << "Screen dimming started, putting gamepad on standby." << endl;
                            current_vpad_mode = VPAD_LCD_STANDBY;
                            VPADSetLcdMode(VPAD_CHAN_0, current_vpad_mode);
                        }
                    }
                }

                // If we leave the dimmed state, it counts as user input, for detecting
                // activity. Note that this event can be triggered by the gamepad's
                // accelerometers.
                if (dim_countdown > old_dim_countdown) {
                    cout << "Detected activity from DIM" << endl;
                    last_activity = SDL_GetTicks64();
                    // Normally a standby gamepad only wakes up when using buttons or
                    // sticks, this will wake on accelerometer and touch activity too.
                    if (current_vpad_mode == VPAD_LCD_STANDBY) {
                        cout << "Turning gamepad LCD backon." << endl;
                        current_vpad_mode = VPAD_LCD_ON;
                        VPADSetLcdMode(VPAD_CHAN_0, current_vpad_mode);
                    }
                }
                if (dim_countdown == 0 && old_dim_countdown > 0) {
                    cout << "Entered DIM state" << endl;
                }

                old_dim_countdown = dim_countdown;
            } else {
                cout << "IMGetTimeBeforeDimming() returned " << dim_error << endl;
            }

        }
#endif // __WIIU__

        process_events();
        if (!running)
            return;

        rest::process();

        Favorites::process_logic();
        Browser::process_logic();
        Recent::process_logic();
        Player::process_logic();


        Uint64 now = SDL_GetTicks64();
        // process transitions to screen saver
        switch (state) {
            case State::normal:
                // normal -> fading
                if (cfg::screen_saver_timeout
                    && (now - last_activity) > cfg::screen_saver_timeout * 1000) {
                    cout << "Fading out..." << endl;
                    state = State::fading;
                    fade_start = now;
                }
                break;
            case State::fading:
                // fading -> screen_saver
                if ((now - fade_start) > fade_duration_ms) {
                    cout << "Full screen saver" << endl;
                    state = State::screen_saver;
                }
                break;
            case State::screen_saver:
                ;
        }
        // any user activity forces it back to normal state
        if (state != State::normal)
            if ((now - last_activity) <= cfg::screen_saver_timeout * 1000) {
                cout << "Returning to normal" << endl;
                state = State::normal;
            }

        process_ui();

        process_screen_saver();
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
