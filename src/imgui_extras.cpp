/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cinttypes>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <imgui_internal.h>

#include "imgui_extras.hpp"



namespace ImGui {


    void
    ScrollWhenDraggingOnVoid(const ImVec2& delta,
                             ImGuiMouseButton mouse_button);

    void
    ScrollWhenDraggingOnVoid(ImGuiID target_id,
                             const ImVec2& delta,
                             ImGuiMouseButton mouse_button);


    namespace {

        std::string
        cpp_sprintf(const char* fmt, va_list args)
        {
            std::va_list args2;
            va_copy(args2, args);
            int size = std::vsnprintf(nullptr, 0, fmt, args2);
            va_end(args2);
            if (size < 0)
                throw std::runtime_error{"vsnprintf() failed"};

            std::string text(size, '\0');
            std::vsnprintf(text.data(), text.size() + 1, fmt, args);
            return text;
        }


#if 0
        std::string
        cpp_sprintf(const char* fmt, ...)
        {
            std::va_list args;
            va_start(args, fmt);
            try {
                auto result = cpp_sprintf(fmt, args);
                va_end(args);
                return result;
            }
            catch (...) {
                va_end(args);
                throw;
            }
        }
#endif

        ImVec2
        to_imgui(sdl::vec2 p)
        {
            return ImVec2(p.x, p.y);
        }


        ImVec2
        to_imgui(sdl::vec2f p)
        {
            return ImVec2(p.x, p.y);
        }


        ImVec4
        to_imgui(sdl::color c)
        {
            auto rgba = c.to_rgba();
            return ImVec4(rgba.r, rgba.g, rgba.b, rgba.a);
        }


        template<concepts::arithmetic T>
        constexpr ImGuiDataType imgui_data_type_v;


        template<>
        constexpr ImGuiDataType imgui_data_type_v<char> =
            std::numeric_limits<char>::is_signed ? ImGuiDataType_S8 : ImGuiDataType_U8;

        template<>
        constexpr ImGuiDataType imgui_data_type_v<std::int8_t> = ImGuiDataType_S8;
        template<>
        constexpr ImGuiDataType imgui_data_type_v<std::uint8_t> = ImGuiDataType_U8;

        template<>
        constexpr ImGuiDataType imgui_data_type_v<std::int16_t> = ImGuiDataType_S16;
        template<>
        constexpr ImGuiDataType imgui_data_type_v<std::uint16_t> = ImGuiDataType_U16;

        template<>
        constexpr ImGuiDataType imgui_data_type_v<std::int32_t> = ImGuiDataType_S32;
        template<>
        constexpr ImGuiDataType imgui_data_type_v<std::uint32_t> = ImGuiDataType_U32;

        template<>
        constexpr ImGuiDataType imgui_data_type_v<std::int64_t> = ImGuiDataType_S64;
        template<>
        constexpr ImGuiDataType imgui_data_type_v<std::uint64_t> = ImGuiDataType_U64;

        template<>
        constexpr ImGuiDataType imgui_data_type_v<float> = ImGuiDataType_Float;
        template<>
        constexpr ImGuiDataType imgui_data_type_v<double> = ImGuiDataType_Double;

    } // namespace


    bool
    BeginCombo(const char* label,
               const std::string& preview,
               ImGuiComboFlags flags)
    {
        return BeginCombo(label, preview.data(), flags);
    }


    template<concepts::arithmetic T>
    bool
    Drag(const char* label,
         T& v,
         T v_min,
         T v_max,
         float speed,
         const char* format,
         ImGuiSliderFlags flags)
    {
        return DragScalar(label, imgui_data_type_v<T>, &v, speed, &v_min, &v_max, format, flags);
    }

    template
    bool
    Drag<std::int8_t>(const char*,
                      std::int8_t&, std::int8_t, std::int8_t,
                      float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<std::uint8_t>(const char*,
                       std::uint8_t&, std::uint8_t, std::uint8_t,
                       float, const char*, ImGuiSliderFlags);

    template
    bool
    Drag<std::int16_t>(const char*,
                       std::int16_t&, std::int16_t, std::int16_t,
                       float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<std::uint16_t>(const char*,
                        std::uint16_t&, std::uint16_t, std::uint16_t,
                        float, const char*, ImGuiSliderFlags);

    template
    bool
    Drag<std::int32_t>(const char*,
                       std::int32_t&, std::int32_t, std::int32_t,
                       float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<std::uint32_t>(const char*,
                        std::uint32_t&, std::uint32_t, std::uint32_t,
                        float, const char*, ImGuiSliderFlags);

    template
    bool
    Drag<std::int64_t>(const char*,
                       std::int64_t&, std::int64_t, std::int64_t,
                       float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<std::uint64_t>(const char*,
                        std::uint64_t&, std::uint64_t, std::uint64_t,
                        float, const char*, ImGuiSliderFlags);

    template
    bool
    Drag<float>(const char*,
                float&, float, float,
                float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<double>(const char*,
                 double&, double, double,
                 float, const char*, ImGuiSliderFlags);


    void
    HandleDragScroll()
    {
        ScrollWhenDraggingOnVoid(-GetIO().MouseDelta, ImGuiMouseButton_Left);
    }


    void
    HandleDragScroll(ImGuiID target_id)
    {
        ImGui::ScrollWhenDraggingOnVoid(target_id, -GetIO().MouseDelta, ImGuiMouseButton_Left);
    }


    void
    Image(const sdl::texture& texture,
          const sdl::vec2& size,
          const sdl::vec2f& uv0,
          const sdl::vec2f& uv1)
    {
        Image(reinterpret_cast<ImTextureID>(texture.data()),
              to_imgui(size),
              to_imgui(uv0),
              to_imgui(uv1));
    }


    void
    Image(const sdl::texture& texture,
          const sdl::vec2f& uv0,
          const sdl::vec2f& uv1)
    {
        Image(texture, texture.get_size(), uv0, uv1);
    }


    bool
    ImageButton(const char* str_id,
                const sdl::texture& texture,
                const sdl::vec2& size,
                const sdl::vec2f& uv0,
                const sdl::vec2f& uv1,
                sdl::color bg_color,
                sdl::color tint_color)
    {
        return ImageButton(str_id,
                           reinterpret_cast<ImTextureID>(texture.data()),
                           to_imgui(size),
                           to_imgui(uv0),
                           to_imgui(uv1),
                           to_imgui(bg_color),
                           to_imgui(tint_color));
    }


    bool
    ImageButton(const char* str_id,
                const sdl::texture& texture,
                const sdl::vec2f& uv0,
                const sdl::vec2f& uv1,
                sdl::color bg_color,
                sdl::color tint_color)
    {
        return ImageButton(str_id,
                           texture,
                           texture.get_size(),
                           uv0,
                           uv1,
                           bg_color,
                           tint_color);
    }


    void
    ImageCentered(const sdl::texture& texture,
                  const sdl::vec2& size,
                  const sdl::vec2f& uv0,
                  const sdl::vec2f& uv1)
    {
        auto window_width = ImGui::GetWindowSize().x;
        ImGui::SetCursorPosX(0.5f * (window_width - size.x));
        ImGui::Image(texture, size, uv0, uv1);
    }

    void
    ImageCentered(const sdl::texture& texture,
                  const sdl::vec2f& uv0,
                  const sdl::vec2f& uv1)
    {
        ImageCentered(texture, texture.get_size(), uv0, uv1);
    }


    template<concepts::arithmetic T>
    bool
    Input(const char* label,
          T& v,
          T step,
          T step_fast,
          const char* format,
          ImGuiInputTextFlags flags)
    {
        return InputScalar(label, imgui_data_type_v<T>, &v, &step, &step_fast, format, flags);
    }


    template
    bool
    Input<std::int8_t>(const char*,
                       std::int8_t&, std::int8_t, std::int8_t,
                       const char*, ImGuiInputTextFlags);
    template
    bool
    Input<std::uint8_t>(const char*,
                        std::uint8_t&, std::uint8_t, std::uint8_t,
                        const char*, ImGuiInputTextFlags);

    template
    bool
    Input<std::int16_t>(const char*,
                        std::int16_t&, std::int16_t, std::int16_t,
                        const char*, ImGuiInputTextFlags);
    template
    bool
    Input<std::uint16_t>(const char*,
                         std::uint16_t&, std::uint16_t, std::uint16_t,
                         const char*, ImGuiInputTextFlags);

    template
    bool
    Input<std::int32_t>(const char*,
                        std::int32_t&, std::int32_t, std::int32_t,
                        const char*, ImGuiInputTextFlags);
    template
    bool
    Input<std::uint32_t>(const char*,
                         std::uint32_t&, std::uint32_t, std::uint32_t,
                         const char*, ImGuiInputTextFlags);

    template
    bool
    Input<std::int64_t>(const char*,
                        std::int64_t&, std::int64_t, std::int64_t,
                        const char*, ImGuiInputTextFlags);
    template
    bool
    Input<std::uint64_t>(const char*,
                         std::uint64_t&, std::uint64_t, std::uint64_t,
                         const char*, ImGuiInputTextFlags);

    template
    bool
    Input<float>(const char*,
                 float&, float, float,
                 const char*, ImGuiInputTextFlags);

    template
    bool
    Input<double>(const char*,
                  double&, double, double,
                  const char*, ImGuiInputTextFlags);


    // From https://github.com/ocornut/imgui/issues/3379#issuecomment-1678718752
    void
    ScrollWhenDraggingOnVoid(const ImVec2& delta,
                             ImGuiMouseButton mouse_button)
    {
        ImGuiContext& g = *GetCurrentContext();
        ImGuiWindow* window = g.CurrentWindow;
        bool hovered = false;
        bool held = false;
        ImGuiID id = window->GetID("##scrolldraggingoverlay");
        KeepAliveID(id);
        ImGuiButtonFlags button_flags = (mouse_button == 0)
            ? ImGuiButtonFlags_MouseButtonLeft
            : (mouse_button == 1)
                ? ImGuiButtonFlags_MouseButtonRight
                : ImGuiButtonFlags_MouseButtonMiddle;
        if (g.HoveredId == 0) // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
            ButtonBehavior(window->Rect(), id, &hovered, &held, button_flags);
        if (held && delta.x != 0.0f)
            SetScrollX(window, window->Scroll.x + delta.x);
        if (held && delta.y != 0.0f)
            SetScrollY(window, window->Scroll.y + delta.y);
    }


    void
    ScrollWhenDraggingOnVoid(ImGuiID target_id,
                             const ImVec2& delta,
                             ImGuiMouseButton mouse_button)
    {
        auto& g = *GetCurrentContext();
        ImGuiWindow* target_window = FindWindowByID(target_id);
        if (!target_window)
            return;
        bool hovered = false;
        bool held = false;
        ImGuiWindow* current_window = g.CurrentWindow;
        ImGuiID overlay_id = current_window->GetID("##scrolldraggingoverlay");
        KeepAliveID(overlay_id);
        ImGuiButtonFlags button_flags = (mouse_button == 0)
            ? ImGuiButtonFlags_MouseButtonLeft
            : (mouse_button == 1)
                ? ImGuiButtonFlags_MouseButtonRight
                : ImGuiButtonFlags_MouseButtonMiddle;

        if (g.HoveredId == 0)
            // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
            ButtonBehavior(current_window->Rect(),
                           overlay_id,
                           &hovered,
                           &held,
                           button_flags);

        if (held && delta.x != 0.0f)
            SetScrollX(target_window, target_window->Scroll.x + delta.x);
        if (held && delta.y != 0.0f)
            SetScrollY(target_window, target_window->Scroll.y + delta.y);
    }


    bool
    Selectable(const std::string& label,
               bool selected,
               ImGuiSelectableFlags flags,
               const ImVec2& size)
    {
        return Selectable(label.data(), selected, flags, size);
    }


    template<concepts::arithmetic T>
    bool
    Slider(const char* label,
           T& v,
           T v_min,
           T v_max,
           const char* format,
           ImGuiSliderFlags flags)
    {
        return SliderScalar(label, imgui_data_type_v<T>, &v, &v_min, &v_max, format, flags);
    }


    template
    bool
    Slider<std::int8_t>(const char*,
                        std::int8_t&, std::int8_t, std::int8_t,
                        const char*, ImGuiSliderFlags);
    template
    bool
    Slider<std::uint8_t>(const char*,
                         std::uint8_t&, std::uint8_t, std::uint8_t,
                         const char*, ImGuiSliderFlags);

    template
    bool
    Slider<std::int16_t>(const char*,
                         std::int16_t&, std::int16_t, std::int16_t,
                         const char*, ImGuiSliderFlags);
    template
    bool
    Slider<std::uint16_t>(const char*,
                          std::uint16_t&, std::uint16_t, std::uint16_t,
                          const char*, ImGuiSliderFlags);

    template
    bool
    Slider<std::int32_t>(const char*,
                         std::int32_t&, std::int32_t, std::int32_t,
                         const char*, ImGuiSliderFlags);
    template
    bool
    Slider<std::uint32_t>(const char*,
                          std::uint32_t&, std::uint32_t, std::uint32_t,
                          const char*, ImGuiSliderFlags);

    template
    bool
    Slider<std::int64_t>(const char*,
                         std::int64_t&, std::int64_t, std::int64_t,
                         const char*, ImGuiSliderFlags);
    template
    bool
    Slider<std::uint64_t>(const char*,
                          std::uint64_t&, std::uint64_t, std::uint64_t,
                          const char*, ImGuiSliderFlags);

    template
    bool
    Slider<float>(const char*,
                  float&, float, float,
                  const char*, ImGuiSliderFlags);
    template
    bool
    Slider<double>(const char*,
                   double&, double, double,
                   const char*, ImGuiSliderFlags);


    void
    TextCentered(const char* fmt, ...)
    {
        std::string text;
        std::va_list args;
        va_start(args, fmt);
        try {
            text = cpp_sprintf(fmt, args);
            va_end(args);
        }
        catch (...) {
            va_end(args);
            throw;
        }

        auto window_width = ImGui::GetWindowSize().x;
        auto text_width   = ImGui::CalcTextSize(text.data(), nullptr, true).x;

        ImGui::SetCursorPosX(0.5f * (window_width - text_width));
        ImGui::Text("%s", text.data());
    }


    void
    TextUnformatted(const std::string& text)
    {
        TextUnformatted(text.data(), text.data() + text.size());
    }


    void
    Value(const char* prefix,
          std::int64_t val)
    {
        Text("%s: %" PRId64, prefix, val);
    }


    void
    Value(const char* prefix,
          std::uint64_t val)
    {
        Text("%s: %" PRIu64, prefix, val);
    }


    void
    Value(const char* prefix,
          const std::string& str)
    {
        Text("%s: %s", prefix, str.data());
    }


    void
    ValueWrapped(const char* prefix,
                 std::int64_t val)
    {
        TextWrapped("%s: %" PRId64, prefix, val);
    }


    void
    ValueWrapped(const char* prefix,
                 std::uint64_t val)
    {
        TextWrapped("%s: %" PRIu64, prefix, val);
    }


    void
    ValueWrapped(const char* prefix,
                 const std::string& str)
    {
        TextWrapped("%s: %s", prefix, str.data());
    }

} // namespace ImGui
