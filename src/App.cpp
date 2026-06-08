/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <filesystem>
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
#include "FontManager.hpp"
#include "IconManager.hpp"
#include "IconsFontAwesome4.h"
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
    {
#ifdef __WIIU__
        nn::act::Initialize();
        char buf[256];
        std::snprintf(buf, sizeof buf, "/vol/save/%08x", nn::act::GetPersistentId());
        config_path = buf;
        SAVEInit();
        if (!exists(config_path)) {
            cout << "Creating save dir..." << endl;
            auto status = SAVEInitSaveDir(nn::act::GetSlotNo());
            if (status)
                cout << "SAVEInitSaveDir() returned " << int(status) << endl;
            else
                cout << "Save dir created." << endl;
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
            cout << "Creating " << config_path << endl;
            if (!create_directory(config_path))
                cout << "Could not create " << config_path << endl;
        }
#endif
    }


    void
    finalize_config_dir()
    {
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
        auto& style = ImGui::GetStyle();

        const ImVec2 padding = {9, 9};
        const float rounding = ui_rounding;
        const ImVec2 spacing = {9, 9};

        style.CellPadding       = {padding.x, padding.y / 2};
        style.ChildBorderSize   = 0;
        style.ChildRounding     = rounding;
        style.FrameBorderSize   = 0;
        style.FramePadding      = padding;
        style.FrameRounding     = rounding;
        style.GrabMinSize       = 32;
        style.GrabRounding      = rounding;
        style.ImageBorderSize   = 0;
        style.ItemInnerSpacing  = spacing;
        style.ItemSpacing       = spacing;
        style.PopupRounding     = rounding;
        style.ScrollbarRounding = rounding;
        style.ScrollbarSize     = 32;
        style.TabBarBorderSize  = 2;
        style.TabBorderSize     = 0;
        style.TabRounding       = rounding;
        style.WindowBorderSize  = 0;
        style.WindowPadding     = padding;
        style.WindowRounding    = 0;
    }


    void
    initialize_imgui()
    {
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

        FontManager::initialize(); // load system font(s)
        FontManager::load_dir(get_content_path() / "fonts");
        FontManager::load_dir(get_config_path() / "fonts");

        setup_imgui_style();

        ImGui_ImplSDL2_InitForSDLRenderer(res->window.data(),
                                          res->renderer.data());
        ImGui_ImplSDLRenderer2_Init(res->renderer.data());
    }


    void
    finalize_imgui()
    {
        FontManager::finalize();
        ImGui::DestroyContext();
    }


    void
    initialize()
    {
        TRACE_FUNC;

        initialize_config_dir();
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
        IconManager::initialize(res->renderer);
        RadioBrowserAPI::initialize(get_user_agent());
        RadioBrowserAPI::set_server(cfg::state.server);

        // Initialize tabs.
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

        // Finalize modules.
        RadioBrowserAPI::finalize();
        IconManager::finalize();
        Styles::finalize();

        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        // Finalize cfg module last.
        cfg::state.remember_tab = cfg::state.initial_tab == TabID::last_active;
        if (cfg::state.remember_tab)
            cfg::state.initial_tab = current_tab;
        cfg::finalize();
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
                    cout << "Added controller: " << gc.get_name() << endl;
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
            ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize, ImGuiCond_Always);
            if (ImGui::RAII::Window main_window{
                    PACKAGE_STRING,
                    nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoResize
                }) {

                ImGui::RAII::StyleVar window_border_size{ImGuiStyleVar_WindowBorderSize,
                                                         1.0f};
                ImGui::RAII::StyleVar window_rounding{ImGuiStyleVar_WindowRounding,
                                                      ui_rounding};

                {
                    // App name, centered
                    {
                        ImGui::RAII::Font title_font{nullptr, 48};
                        UI::show_text_centered("%s", PACKAGE_STRING);
                    }

                    ImGui::SameLine();
                    // Put a close button on the top right
                    auto tex = IconManager::get("ui/close-button.svg");
                    auto tex_size = ImGui::ToVec2(tex->get_size());
                    ImVec2 close_button_size = tex_size
                        + 2 * (style.FramePadding + style.FrameBorderSize * ImVec2{1, 1});
                    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x
                                         - close_button_size.x);

                    if (UI::show_image_button("close_button", *tex))
                        quit();
                }

                if (ImGui::RAII::TabBar tab_bar{"main_tabs"}) {

                    if (ImGui::RAII::TabItem fav_tab{
                            to_label(TabID::favorites),
                            nullptr,
                            get_tab_item_flags_for(TabID::favorites)
                        }) {
                        current_tab = TabID::favorites;
                        FavoritesTab::process_ui();
                    }

                    if (ImGui::RAII::TabItem browser_tab{
                            to_label(TabID::browser),
                            nullptr,
                            get_tab_item_flags_for(TabID::browser)}) {
                        current_tab = TabID::browser;
                        BrowserTab::process_ui();
                    }

                    if (ImGui::RAII::TabItem recent_tab{
                            to_label(TabID::recent),
                            nullptr,
                            get_tab_item_flags_for(TabID::recent)
                        }) {
                        current_tab = TabID::recent;
                        RecentTab::process_ui();
                    }

                    if (ImGui::RAII::TabItem player_tab{
                            to_label(TabID::player),
                            nullptr,
                            get_tab_item_flags_for(TabID::player)
                        }) {
                        current_tab = TabID::player;
                        PlayerTab::process_ui();
                    }

                    if (ImGui::RAII::TabItem settings_tab{
                            to_label(TabID::settings),
                            nullptr,
                            get_tab_item_flags_for(TabID::settings)
                        }) {
                        current_tab = TabID::settings;
                        SettingsTab::process_ui();
                    }

                    if (ImGui::RAII::TabItem about_tab{
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

            } // main_window

            Styles::process_ui();

            // ImGui::ShowStyleEditor();

        }

        ImGui::EndFrame();
        ImGui::Render();
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

        RadioBrowserAPI::process();

        FavoritesTab::process_logic();
        RecentTab::process_logic();
        PlayerTab::process_logic();


        Uint64 now = SDL_GetTicks64();
        // process transitions to screen saver
        switch (state) {
            case State::normal:
                // normal -> fading
                if (cfg::state.screen_saver_timeout
                    && (now - last_activity) > cfg::state.screen_saver_timeout * 1000) {
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
            if ((now - last_activity) <= cfg::state.screen_saver_timeout * 1000) {
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


    void
    set_tab(TabID id)
    {
        if (id == TabID::last_active)
            id = TabID::browser;
        next_tab = id;
    }

} // namespace App
