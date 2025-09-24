/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>

#ifdef __WIIU__
#include <coreinit/memory.h>
#endif

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <misc/freetype/imgui_freetype.h>

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
#include "utils.hpp"

#include "net/resolver.hpp"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;

using std::filesystem::path;

using namespace sdl::literals;


App* App::instance = nullptr;


namespace {

    const float cafe_size = 32;
    const float symbola_size = cafe_size * 1.2f;


    void
    load_fonts()
    {
        auto& io = ImGui::GetIO();

        ImFontConfig font_config;

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
        }

        font_config.FontDataOwnedByAtlas = true;

#else

        if (!io.Fonts->AddFontFromFileTTF("CafeStd.ttf",
                                          cafe_size,
                                          &font_config))
            throw std::runtime_error{"Could not load font!"};

#endif

        font_config.MergeMode = true;
        font_config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
        font_config.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;

        if (!io.Fonts->AddFontFromFileTTF((utils::get_content_path() / "Symbola.ttf").c_str(),
                                          symbola_size,
                                          &font_config))
            throw std::runtime_error{"Could not load font!"};

    }


    enum class TabIndex {
        favorites,
        browser,
        recent,
        player,
        settings,
        about,
    };

    std::optional<TabIndex> next_tab;

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


} // namespace


App::App() :
    sdl_init{sdl::init::flag::video,
             sdl::init::flag::audio,
             sdl::init::flag::game_controller},
    running{false}
{
    instance = this;

    cfg::initialize();

    if (cfg::start_on_favorites)
        next_tab = TabIndex::favorites;
    else
        next_tab = TabIndex::browser;

    // Create an audio device to stop the boot sound.
    sdl::audio::spec aspec;
    aspec.freq = 44100;
    aspec.format = AUDIO_S16SYS;
    aspec.channels = 2;
    aspec.samples = 2048;
    sdl::audio::device adev{nullptr, false, aspec};

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "2");

    window.create(PACKAGE,
                  sdl::window::pos_centered,
                  {1280, 720},
                  0);

    window_size = window.get_size();
    cout << "SDL window size: " << window_size << endl;

    renderer.create(window, -1,
                    sdl::renderer::flag::accelerated,
                    sdl::renderer::flag::present_vsync);
    renderer.set_logical_size(window_size);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
    io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;

    io.LogFilename = nullptr;
    io.IniFilename = nullptr;

    load_fonts();

    ImGui::StyleColorsDark();

    // Custom styling
    auto& style = ImGui::GetStyle();
    const float radius = 8;

    style.WindowBorderSize = 0;
    style.WindowRounding = 0;
    style.WindowPadding = {12, 12};

    style.ScrollbarSize = 32;
    style.ScrollbarRounding = radius;

    style.GrabMinSize = 32;
    style.GrabRounding = radius;

    style.FrameRounding = radius;
    style.FramePadding = {12, 12};

    style.ImageBorderSize = 0;

    style.ItemSpacing = {12, 12};
    style.ItemInnerSpacing = {12, 12};

    style.ChildBorderSize = 0;
    style.ChildRounding = 0;

    style.TabRounding = radius;

    ImGui_ImplSDL2_InitForSDLRenderer(window.data(), renderer.data());
    ImGui_ImplSDLRenderer2_Init(renderer.data());

    title_texture = make_logo_texture(renderer);

    IconManager::initialize();
    rest::initialize(utils::get_user_agent());

    Favorites::initialize();
    Browser::initialize();
    Recent::initialize();
    Player::initialize();

    cout << "Finished App constructor" << endl;
}


App::~App()
{
    Player::finalize();
    Recent::finalize();
    Browser::finalize();
    Favorites::finalize();
    rest::finalize();
    IconManager::finalize();

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    cfg::finalize();
    cout << "All done!" << endl;
}


void
App::run()
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
App::process_events()
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
                controllers.push_back(std::move(gc));
                break;
            }

            case sdl::events::type::e_controller_device_removed:
            {
                std::erase_if(controllers,
                              [id=event.cdevice.which](sdl::game_controller::device& gc) -> bool
                              {
                                  return id == gc.get_id();
                              });
            }
            break;
        } // switch (event.type)

    }
}


void
App::process_ui()
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
        ImGui::ImageCentered(title_texture);
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

            if (ImGui::BeginTabItem("★ Favorites",
                                    nullptr,
                                    get_tab_item_flags_for(TabIndex::favorites))) {
                Favorites::process_ui();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🔍 Browser",
                                    nullptr,
                                    get_tab_item_flags_for(TabIndex::browser))) {
                Browser::process_ui();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🕓 Recent",
                                    nullptr,
                                    get_tab_item_flags_for(TabIndex::recent))) {
                Recent::process_ui();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🎧 Player",
                                    nullptr,
                                    get_tab_item_flags_for(TabIndex::player))) {
                Player::process_ui();
                ImGui::EndTabItem();

            }

            if (ImGui::BeginTabItem("⚙ Settings",
                                    nullptr,
                                    get_tab_item_flags_for(TabIndex::settings))) {
                Settings::process_ui();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("About",
                                    nullptr,
                                    get_tab_item_flags_for(TabIndex::about))) {
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
App::process()
{
    process_events();
    if (!running)
        return;

    rest::process();

    Browser::process();
    Player::process_playback();

    process_ui();
}


void
App::quit()
{
    running = false;
}


void
App::draw()
{
    renderer.set_color(sdl::color::black);
    renderer.clear();

    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer.data());

#ifdef __WIIU__
    // WORKAROUND: the Wii U port does not update the clipping until the next draw
    renderer.set_color(sdl::color::transparent);
    renderer.draw_point(0, 0);
#endif

    renderer.present();
}


App*
App::get_instance()
{
    return instance;
}
