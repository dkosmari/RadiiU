/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef __WIIU__
#include <coreinit/energysaver.h>
#include <coreinit/memory.h>
#include <nn/act.h>
#include <nn/save.h>
#include <vpad/input.h>
#else // !__WIIU__
#include <fontconfig/fontconfig.h>
#include <wordexp.h>
#endif

#include <cafe_glyphs.h>

#include <curlxx/global.hpp>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <imgui_freetype.h>

#include <sdl2xx/sdl.hpp>
#include <sdl2xx/img.hpp>

#include "App.hpp"

#include "AboutTab.hpp"
#include "BrowserTab.hpp"
#include "cfg.hpp"
#include "FavoritesTab.hpp"
#include "FontLoader.hpp"
#include "ImageLoader.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "LogsTab.hpp"
#include "PlayerTab.hpp"
#include "RadioBrowserAPI.hpp"
#include "RecentTab.hpp"
#include "SettingsTab.hpp"
#include "Styles.hpp"
#include "tracer.hpp"
#include "UI.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;

using std::filesystem::path;

using namespace std::literals;
using namespace sdl::literals;


namespace App {

    void
    process_events();

    void
    process_ui();

    void
    process_screen_saver();

    void
    process();

    void
    quit();

    void
    draw();


    namespace {

        // RAII-managed resources are stored here.
        struct Resources {

            curl::global::init curl_init;

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

        std::filesystem::path config_path;

    } // namespace


    const std::string&
    get_user_agent()
    {
        static const std::string user_agent = PACKAGE_NAME "/" PACKAGE_VERSION
                                              + " ("s + SDL_GetPlatform() + ")"s;
        return user_agent;
    }


    const std::filesystem::path&
    get_content_path()
    {
        static const std::filesystem::path content_path =
#ifdef __WIIU__
            "/vol/content"
#else
            "assets/content"
#endif
            ;
        return content_path;
    }


    void
    initialize_config_dir()
        noexcept
    try {
        TRACE_FUNC;

#ifdef __WIIU__
        nn::act::Initialize();
        config_path = std::format("/vol/save/{:08x}", nn::act::GetPersistentId());
        SAVEInit();
        if (!exists(config_path)) {
            LOG_INFO("Creating save dir: {:?}", config_path.string());
            auto status = SAVEInitSaveDir(nn::act::GetSlotNo());
            if (status)
                LOG_ERROR("SAVEInitSaveDir() failed: {}", static_cast<int>(status));
            else
                LOG_INFO("Save dir created.");
        }
#else
        std::filesystem::path config_home;
        wordexp_t expanded{};
        if (wordexp("${XDG_CONFIG_HOME:-~/.config}", &expanded, WRDE_NOCMD)) {
            // env variables not set properly, just use the "current" directory and hope
            // it works
            config_home = ".";
        } else {
            config_home = expanded.we_wordv[0];
            config_path = config_home / PACKAGE;
        }
        wordfree(&expanded);

        if (!exists(config_path)) {
            LOG_INFO("Creating config dir: {:?}", config_path.string());
            if (!create_directory(config_path))
                LOG_ERROR("create_directories() failed.");
        }
#endif
    }
    catch (std::exception& e) {
        LOG_ERROR("{}(): {}", __func__, e.what());
    }


    void
    finalize_config_dir()
    {
        TRACE_FUNC;

#ifdef __WIIU__
        SAVEShutdown();
        nn::act::Finalize();
#endif
    }


    const std::filesystem::path&
    get_config_path()
    {
        return config_path;
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
    setup_imgui_style()
    {
        TRACE_FUNC;

        auto& style = ImGui::GetStyle();

        style.FontSizeBase = 32;

        const ImVec2 padding = {9, 9};
        const float rounding = 9;
        const ImVec2 spacing = {12, 9};
        const float thickness = 2;

        // Thickness of borders and lines
        style.ChildBorderSize          = thickness;
        style.DragDropTargetBorderSize = thickness;
        style.FrameBorderSize          = 0;
        style.ImageBorderSize          = 0;
        style.InputTextCursorSize      = 2;
        style.PopupBorderSize          = thickness;
        style.SeparatorSize            = 2;
        style.SeparatorTextBorderSize  = 2;
        style.TabBarBorderSize         = thickness;
        style.TabBarOverlineSize       = thickness;
        style.TabBorderSize            = 0;
        style.TreeLinesSize            = thickness;
        style.WindowBorderSize         = 0;

        // Padding
        style.CellPadding           = padding;
        style.DragDropTargetPadding = padding.x;
        style.FramePadding          = padding;
        style.ScrollbarPadding      = 4;
        style.SeparatorTextPadding  = { style.FontSizeBase, padding.y };
        style.WindowPadding         = {6, 6};

        // Rounding
        style.ChildRounding          = rounding;
        style.DragDropTargetRounding = rounding;
        style.FrameRounding          = rounding;
        style.GrabRounding           = rounding;
        style.ImageRounding          = 0;
        style.MenuItemRounding       = rounding;
        style.PopupRounding          = rounding;
        style.ScrollbarRounding      = rounding;
        style.SelectableRounding     = rounding;
        style.TabRounding            = rounding;
        style.TreeLinesRounding      = rounding;
        style.WindowRounding         = rounding;

        // Spacing
        style.IndentSpacing    = style.FontSizeBase + 2 * padding.x;
        style.ItemInnerSpacing = spacing;
        style.ItemSpacing      = spacing;

        // Sizes
        style.ColorMarkerSize         = 12;
        style.GrabMinSize             = 32;
        style.ScrollbarSize           = 32;

        // Misc
        style.ColumnsMinSpacing        = padding.x + 1;
        style.DisplaySafeAreaPadding   = {9, 9};
        style.DisplayWindowPadding     = {12, 12};
        style.LogSliderDeadzone        = 12;
        style.MouseCursorScale         = 2;
        style.TabMinWidthBase          = 128;
        style.TabMinWidthShrink        = 63;
        style.TouchExtraPadding        = {4, 4};
        style.WindowBorderHoverPadding = 6;
        style.WindowMinSize            = {64, 64};

        style.DisplaySafeAreaPadding = {10, 10};
    }


    void
    initialize_imgui()
    {
        TRACE_FUNC;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        io.ConfigDragScroll = true;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.MouseDragThreshold = 25;

        io.LogFilename = nullptr; // don't save log
        io.IniFilename = nullptr; // don't save ini

        setup_imgui_style();

        FontLoader::initialize(); // load system font(s)
        FontLoader::load_dir(get_content_path() / "fonts");
        FontLoader::load_dir(get_config_path() / "fonts");

        ImGui_ImplSDL2_InitForSDLRenderer(res->window.data(),
                                          res->renderer.data());
        ImGui_ImplSDLRenderer2_Init(res->renderer.data());
    }


    void
    finalize_imgui()
    {
        TRACE_FUNC;

        FontLoader::finalize();
        ImGui::DestroyContext();
    }


    void
    initialize()
    {
        TRACE_FUNC;

        initialize_config_dir();
        LogManager::initialize();
        // Note: initialize cfg module early.
        cfg::initialize();
        set_tab(cfg::state.initial_tab);
        if (cfg::state.remember_tab)
            cfg::state.initial_tab = TabID::last_active;

#ifdef __WIIU__
        old_disable_swkbd = cfg::state.disable_swkbd;
        if (cfg::state.disable_swkbd) {
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
        Styles::initialize();
        ImageLoader::initialize(res->renderer);
        RadioBrowserAPI::initialize(get_user_agent(), cfg::state.server);
        RadioBrowserAPI::set_server(cfg::state.server);

        // Initialize tabs.
        LogsTab::initialize();
        AboutTab::initialize();
        FavoritesTab::initialize();
        BrowserTab::initialize();
        RecentTab::initialize();
        PlayerTab::initialize();
    }


    void
    finalize()
    {
        TRACE_FUNC;

        // Finalize tabs.
        PlayerTab::finalize();
        RecentTab::finalize();
        BrowserTab::finalize();
        FavoritesTab::finalize();
        AboutTab::finalize();
        LogsTab::finalize();

        // Finalize modules.
        RadioBrowserAPI::finalize();
        ImageLoader::finalize();
        Styles::finalize();

        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        // Finalize cfg module last.
        cfg::state.remember_tab = cfg::state.initial_tab == TabID::last_active;
        if (cfg::state.remember_tab)
            cfg::state.initial_tab = current_tab;
        cfg::finalize();
        LogManager::finalize();
        finalize_config_dir();

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
        Uint64 now = SDL_GetTicks64();

        sdl::events::event event;
        while (sdl::events::poll(event)) {

            ImGui_ImplSDL2_ProcessEvent(&event);

            switch (sdl::events::type{event.type}) {

                using enum sdl::events::type;

                case quit:
                    App::quit();
                    break;

                case controller_device_added: {
                    auto gc = sdl::game_controller::device(event.cdevice.which);
                    LOG_INFO("Added controller: {:?}", gc.get_name());
                    res->controllers.push_back(std::move(gc));
                    last_activity = now;
                    break;
                }

                case controller_device_removed: {
                    std::erase_if(res->controllers,
                                  [id=event.cdevice.which](sdl::game_controller::device& gc)
                                  {
                                      return id == gc.get_id();
                                  });
                    last_activity = now;
                    break;
                }

                case controller_axis:
                case controller_down:
                case controller_up:
                case key_down:
                case key_up:
                case mouse_down:
                case mouse_motion:
                case mouse_up:
                case mouse_wheel:
                case text_editing:
                case text_editing_ext:
                case text_input:
                case will_enter_foreground:
                    last_activity = now;
                    break;

                case window:
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
        using namespace ImGui::RAII;

        const auto& style = ImGui::GetStyle();

        {
            /*
             * Main window:
             * - Occupy the whole viewort.
             * - No decorations, no move.
             */
            auto viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
            if (Window main_window{PACKAGE_STRING,
                                   nullptr,
                                   ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings}) {
                const auto content_begin = ImGui::GetCursorStartPos();
                const auto content_end = content_begin + ImGui::GetContentRegionAvail();

                // App name, centered
                {
                    Font title_font{nullptr, 48};
                    ImGui::TextAligned(0.5f, -1, PACKAGE_STRING);
                    // UI::show_last_bounding_box();
                }

                if (TabBar tab_bar{"main_tabs"}) {

                    if (TabItem fav_tab{
                            to_label(TabID::favorites),
                            nullptr,
                            get_tab_item_flags_for(TabID::favorites)
                        }) {
                        current_tab = TabID::favorites;
                        FavoritesTab::process_ui();
                    }

                    if (TabItem browser_tab{
                            to_label(TabID::browser),
                            nullptr,
                            get_tab_item_flags_for(TabID::browser)}) {
                        current_tab = TabID::browser;
                        BrowserTab::process_ui();
                    }

                    if (TabItem recent_tab{
                            to_label(TabID::recent),
                            nullptr,
                            get_tab_item_flags_for(TabID::recent)
                        }) {
                        current_tab = TabID::recent;
                        RecentTab::process_ui();
                    }

                    if (TabItem player_tab{
                            to_label(TabID::player),
                            nullptr,
                            get_tab_item_flags_for(TabID::player)
                        }) {
                        current_tab = TabID::player;
                        PlayerTab::process_ui();
                    }

                    if (TabItem settings_tab{
                            to_label(TabID::settings),
                            nullptr,
                            get_tab_item_flags_for(TabID::settings)
                        }) {
                        current_tab = TabID::settings;
                        SettingsTab::process_ui();
                    }

                    if (TabItem logs_tab{
                            to_label(TabID::logs),
                            nullptr,
                            get_tab_item_flags_for(TabID::logs)
                        }) {
                        current_tab = TabID::logs;
                        LogsTab::process_ui();
                    }

                    if (TabItem about_tab{
                            to_label(TabID::about),
                            nullptr,
                            get_tab_item_flags_for(TabID::about)
                        }) {
                        current_tab = TabID::about;
                        AboutTab::process_ui();
                    }

                    if (next_tab) {
                        current_tab = *next_tab;
                        next_tab.reset();
                    }

                } // tab_bar

                // Put an exit button on the top right
                {
                    Font font{nullptr, 28};
                    const std::string label = ICON_FA_SIGN_OUT;
                    ImVec2 size = ImGui::CalcTextSize(label) + 2 * style.FramePadding;
                    const ImVec2 pos = { content_end.x - size.x, content_begin.y };
                    ImGui::SetCursorPos(pos);
                    if (ImGui::Button(label, size))
                        quit();
                }

            } // main_window
        }

        ImGui::ShowStyleEditor();

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
        if (old_disable_swkbd != cfg::state.disable_swkbd) {
            SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, cfg::state.disable_swkbd ? "0" : "1");
            old_disable_swkbd = cfg::state.disable_swkbd;
        }

        std::uint32_t dim_enabled = 0;
        IMError dim_error = IMIsDimEnabled(&dim_enabled);
        VPADLcdMode current_vpad_mode;
        VPADGetLcdMode(VPAD_CHAN_0, &current_vpad_mode);
        if (!dim_error && dim_enabled) {
            std::uint32_t dim_countdown = 0;
            dim_error = IMGetTimeBeforeDimming(&dim_countdown);
            if (!dim_error) {
                if (cfg::state.inactive_screen_off) {
                    // This is the logic to turn the gamepad LCD off when the system
                    // enters the dimmed state (screen burn-in protection.)

                    // TODO: find out how to do it with TV also.
                    if (dim_countdown == 0) {
                        if (current_vpad_mode != VPAD_LCD_STANDBY) {
                            LOG_DEBUG("Screen dimming started, putting gamepad on standby.");
                            current_vpad_mode = VPAD_LCD_STANDBY;
                            VPADSetLcdMode(VPAD_CHAN_0, current_vpad_mode);
                        }
                    }
                }

                // If we leave the dimmed state, it counts as user input, for detecting
                // activity. Note that this event can be triggered by the gamepad's
                // accelerometers.
                if (dim_countdown > old_dim_countdown) {
                    LOG_DEBUG("Detected activity from DIM");
                    last_activity = SDL_GetTicks64();
                    // Normally a standby gamepad only wakes up when using buttons or
                    // sticks, this will wake on accelerometer and touch activity too.
                    if (current_vpad_mode == VPAD_LCD_STANDBY) {
                        LOG_DEBUG("Turning gamepad LCD backon.");
                        current_vpad_mode = VPAD_LCD_ON;
                        VPADSetLcdMode(VPAD_CHAN_0, current_vpad_mode);
                    }
                }
                if (dim_countdown == 0 && old_dim_countdown > 0) {
                    LOG_DEBUG("Entered DIM state");
                }

                old_dim_countdown = dim_countdown;
            } else {
                LOG_ERROR("IMGetTimeBeforeDimming() failed: {}", static_cast<int>(dim_error));
            }

        }
#endif // __WIIU__

        process_events();
        if (!running)
            return;

        RadioBrowserAPI::process();

        FavoritesTab::process_logic();
        RecentTab::process_logic();
        PlayerTab::process_logic();

        ImageLoader::process();

        Uint64 now = SDL_GetTicks64();
        // process transitions to screen saver
        switch (state) {
            case State::normal:
                // normal -> fading
                if (cfg::state.screen_saver_timeout
                    && (now - last_activity) > cfg::state.screen_saver_timeout * 1000) {
                    LOG_DEBUG("Fading out...");
                    state = State::fading;
                    fade_start = now;
                }
                break;
            case State::fading:
                // fading -> screen_saver
                if ((now - fade_start) > fade_duration_ms) {
                    LOG_DEBUG("Fade out finished.");
                    state = State::screen_saver;
                }
                break;
            case State::screen_saver:
                ;
        }
        // any user activity forces it back to normal state
        if (state != State::normal)
            if ((now - last_activity) <= cfg::state.screen_saver_timeout * 1000) {
                LOG_DEBUG("Returned to normal.");
                state = State::normal;
            }

        LogManager::process();

        // ImGui frame processing
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();

        ImGui::NewFrame();

        if (state == State::normal || state == State::fading) {

            auto& style = ImGui::GetStyle();

            // Apply fading effect if fading is active.
            if (state == State::fading) {
                Uint64 now = SDL_GetTicks64();
                float ratio = 1.0f - (now - fade_start) / float(fade_duration_ms);
                if (ratio < 0)
                    ratio = 0;
                style.Alpha = ratio;
            } else {
                style.Alpha = 1.0f;
            }

            process_ui();

        }

        ImGui::EndFrame();
        ImGui::Render();

        process_screen_saver();
    }


    void
    quit()
    {
        TRACE_FUNC;

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


    void
    set_tab(TabID id)
    {
        if (id == TabID::last_active)
            id = TabID::browser;
        next_tab = id;
    }

} // namespace App
