/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef IMGUI_EXTRAS_HPP
#define IMGUI_EXTRAS_HPP

#include <cstdint>
#include <string>
#include <type_traits>

#include <imgui.h>

#include <sdl2xx/sdl.hpp>


constexpr ImGuiDataType_ ImGuiDataType_SizeT = sizeof(std::size_t) == 8
              ? ImGuiDataType_U64 : ImGuiDataType_U32;

constexpr ImGuiDataType_ ImGuiDataType_Uint = sizeof(unsigned) == 8
              ? ImGuiDataType_U64
              : sizeof(unsigned) == 4 ? ImGuiDataType_U32 : ImGuiDataType_U16;


namespace ImGui {

    namespace concepts {

        template<typename T>
        concept arithmetic = std::is_arithmetic_v<T>;

    } // namespace concepts


    bool
    BeginCombo(const char* label,
               const std::string& preview,
               ImGuiComboFlags flags = 0);


    bool
    BeginPopupModal(const std::string& name,
                    bool* p_open = nullptr,
                    ImGuiWindowFlags flags = 0);


    bool
    Button(const std::string& label,
           const ImVec2& size = ImVec2(0, 0));


    ImVec2
    CalcTextSize(const std::string& text,
                 bool hide_text_after_double_hash = false,
                 float wrap_width = -1.0f);


    template<concepts::arithmetic T>
    bool
    Drag(const char* label,
         T& v,
         T v_min,
         T v_max,
         float speed = 1.0f,
         const char* format = nullptr,
         ImGuiSliderFlags flags = 0);


    void
    HandleDragScroll();

    void
    HandleDragScroll(ImGuiID target_id);


    void
    Image(const sdl::texture& texture,
          const sdl::vec2& size,
          const sdl::vec2f& uv0 = {0, 0},
          const sdl::vec2f& uv1 = {1, 1});

    void
    Image(const sdl::texture& texture,
          const sdl::vec2f& uv0 = {0, 0},
          const sdl::vec2f& uv1 = {1, 1});


    bool
    ImageButton(const char* str_id,
                const sdl::texture& texture,
                const sdl::vec2& size,
                const sdl::vec2f& uv0 = {0, 0},
                const sdl::vec2f& uv1 = {1, 1},
                sdl::color bg_color = sdl::color::transparent,
                sdl::color tint_color = sdl::color::white);

    bool
    ImageButton(const char* str_id,
                const sdl::texture& texture,
                const sdl::vec2f& uv0 = {0, 0},
                const sdl::vec2f& uv1 = {1, 1},
                sdl::color bg_color = sdl::color::transparent,
                sdl::color tint_color = sdl::color::white);


    void
    ImageCentered(const sdl::texture& texture,
                  const sdl::vec2& size,
                  const sdl::vec2f& uv0 = {0, 0},
                  const sdl::vec2f& uv1 = {1, 1});

    void
    ImageCentered(const sdl::texture& texture,
                  const sdl::vec2f& uv0 = {0, 0},
                  const sdl::vec2f& uv1 = {1, 1});


    template<concepts::arithmetic T>
    bool
    Input(const char* label,
          T& v,
          T step = T{1},
          T step_fast = T{100},
          const char* format = nullptr,
          ImGuiInputTextFlags flags = 0);


    void
    KineticScrollFrameEnd();


    void
    OpenPopup(const std::string& str_id,
              ImGuiPopupFlags popup_flags = 0);


    void
    PushID(const std::string& str);


    bool
    Selectable(const std::string& label,
               bool selected = false,
               ImGuiSelectableFlags flags = 0,
               const ImVec2& size = ImVec2(0, 0));


    template<concepts::arithmetic T>
    bool
    Slider(const char* label,
           T& v,
           T v_min,
           T v_max,
           const char* format = nullptr,
           ImGuiSliderFlags flags = 0);


    void
    TextAlignedColored(float align_x,
                       float size_x,
                       const ImVec4& color,
                       const char* fmt,
                       ...)
        IM_FMTARGS(4);


    void
    TextCentered(const char* fmt, ...)
        IM_FMTARGS(1);


    void
    TextUnformatted(const std::string& text);


    void
    Value(const char* prefix,
          std::int64_t val);

    void
    Value(const char* prefix,
          std::uint64_t val);

    void
    Value(const char* prefix,
          const std::string& str);


    void
    ValueWrapped(const char* prefix,
                 std::int64_t val);

    void
    ValueWrapped(const char* prefix,
                 std::uint64_t val);

    void
    ValueWrapped(const char* prefix,
                 const std::string& str);

} // namespace ImGui

#endif
