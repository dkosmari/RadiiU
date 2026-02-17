/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h>

#include "imgui_extras.hpp"
#include "utils.hpp"


using std::cout;
using std::endl;


// Define this to handle kinetic scrolling for each axis independently.
#define KINETIC_AXIS


namespace ImGui {

    void
    ScrollWhenDraggingOnVoid(const ImVec2& delta,
                             ImGuiMouseButton mouse_button);

    void
    ScrollWhenDraggingOnVoid(ImGuiID scroll_target,
                             const ImVec2& delta,
                             ImGuiMouseButton mouse_button);


    namespace {

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


        struct ScrollState {
            ImVec2 velocity;
            bool dragging = false;
        };
        std::unordered_map<ImGuiID, ScrollState> scroll_states;


        unsigned pinning_down_frames = 0;


        [[maybe_unused]]
        float
        length(const ImVec2& v)
        {
            return std::hypot(v.x, v.y);
        }

    } // namespace


    bool
    BeginCombo(const std::string& label,
               const std::string& preview,
               ImGuiComboFlags flags)
    {
        return BeginCombo(label.data(),
                          preview.data(),
                          flags);
    }


    bool
    BeginPopup(const std::string& id,
               ImGuiWindowFlags flags)
    {
        return BeginPopup(id.data(), flags);
    }


    bool
    BeginPopupModal(const std::string& name,
                    bool* p_open,
                    ImGuiWindowFlags flags)
    {
        return BeginPopupModal(name.data(), p_open, flags);
    }


    bool
    BeginTabItem(const std::string& label,
                 bool* p_open,
                 ImGuiTabItemFlags flags)
    {
        return BeginTabItem(label.data(), p_open, flags);
    }


    bool
    Button(const std::string& label,
           const ImVec2& size)
    {
        return Button(label.data(), size);
    }


    ImVec2
    CalcTextSize(const std::string& text,
                 bool hide_text_after_double_hash,
                 float wrap_width)
    {
        return CalcTextSize(text.data(),
                            text.data() + text.size(),
                            hide_text_after_double_hash,
                            wrap_width);
    }


    template<concepts::arithmetic T>
    bool
    Drag(const std::string& label,
         T& v,
         T v_min,
         T v_max,
         float speed,
         const char* format,
         ImGuiSliderFlags flags)
    {
        return DragScalar(label.data(),
                          imgui_data_type_v<T>,
                          &v,
                          speed,
                          &v_min,
                          &v_max,
                          format,
                          flags);
    }

    /* ------------------------------------- */
    /* Explicit instantiations for Drag<T>() */
    /* ------------------------------------- */

    template
    bool
    Drag<std::int8_t>(const std::string&,
                      std::int8_t&, std::int8_t, std::int8_t,
                      float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<std::uint8_t>(const std::string&,
                       std::uint8_t&, std::uint8_t, std::uint8_t,
                       float, const char*, ImGuiSliderFlags);

    template
    bool
    Drag<std::int16_t>(const std::string&,
                       std::int16_t&, std::int16_t, std::int16_t,
                       float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<std::uint16_t>(const std::string&,
                        std::uint16_t&, std::uint16_t, std::uint16_t,
                        float, const char*, ImGuiSliderFlags);

    template
    bool
    Drag<std::int32_t>(const std::string&,
                       std::int32_t&, std::int32_t, std::int32_t,
                       float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<std::uint32_t>(const std::string&,
                        std::uint32_t&, std::uint32_t, std::uint32_t,
                        float, const char*, ImGuiSliderFlags);

    template
    bool
    Drag<std::int64_t>(const std::string&,
                       std::int64_t&, std::int64_t, std::int64_t,
                       float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<std::uint64_t>(const std::string&,
                        std::uint64_t&, std::uint64_t, std::uint64_t,
                        float, const char*, ImGuiSliderFlags);

    template
    bool
    Drag<float>(const std::string&,
                float&, float, float,
                float, const char*, ImGuiSliderFlags);
    template
    bool
    Drag<double>(const std::string&,
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
        ScrollWhenDraggingOnVoid(target_id,
                                 -GetIO().MouseDelta,
                                 ImGuiMouseButton_Left);
    }


    void
    Image(const sdl::texture& texture,
          const sdl::vec2& size,
          const sdl::vec2f& uv0,
          const sdl::vec2f& uv1)
    {
        Image(reinterpret_cast<ImTextureID>(texture.data()),
              ToVec2(size),
              ToVec2(uv0),
              ToVec2(uv1));
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
                           ToVec2(size),
                           ToVec2(uv0),
                           ToVec2(uv1),
                           ToVec4(bg_color),
                           ToVec4(tint_color));
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
        auto window_width = GetContentRegionAvail().x;
        SetCursorPosX(0.5f * (window_width - size.x));
        Image(texture, size, uv0, uv1);
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
    Input(const std::string& label,
          T& v,
          T step,
          T step_fast,
          const char* format,
          ImGuiInputTextFlags flags)
    {
        return InputScalar(label.data(),
                           imgui_data_type_v<T>,
                           &v,
                           &step,
                           &step_fast,
                           format,
                           flags);
    }

    /* -------------------------------------- */
    /* Explicit instantiations for Input<T>() */
    /* -------------------------------------- */

    template
    bool
    Input<std::int8_t>(const std::string&,
                       std::int8_t&, std::int8_t, std::int8_t,
                       const char*, ImGuiInputTextFlags);
    template
    bool
    Input<std::uint8_t>(const std::string&,
                        std::uint8_t&, std::uint8_t, std::uint8_t,
                        const char*, ImGuiInputTextFlags);

    template
    bool
    Input<std::int16_t>(const std::string&,
                        std::int16_t&, std::int16_t, std::int16_t,
                        const char*, ImGuiInputTextFlags);
    template
    bool
    Input<std::uint16_t>(const std::string&,
                         std::uint16_t&, std::uint16_t, std::uint16_t,
                         const char*, ImGuiInputTextFlags);

    template
    bool
    Input<std::int32_t>(const std::string&,
                        std::int32_t&, std::int32_t, std::int32_t,
                        const char*, ImGuiInputTextFlags);
    template
    bool
    Input<std::uint32_t>(const std::string&,
                         std::uint32_t&, std::uint32_t, std::uint32_t,
                         const char*, ImGuiInputTextFlags);

    template
    bool
    Input<std::int64_t>(const std::string&,
                        std::int64_t&, std::int64_t, std::int64_t,
                        const char*, ImGuiInputTextFlags);
    template
    bool
    Input<std::uint64_t>(const std::string&,
                         std::uint64_t&, std::uint64_t, std::uint64_t,
                         const char*, ImGuiInputTextFlags);

    template
    bool
    Input<float>(const std::string&,
                 float&, float, float,
                 const char*, ImGuiInputTextFlags);

    template
    bool
    Input<double>(const std::string&,
                  double&, double, double,
                  const char*, ImGuiInputTextFlags);


    bool
    InputText(const std::string& label,
              std::string& value,
              ImGuiInputTextFlags flags,
              ImGuiInputTextCallback callback,
              void* ctx)
    {
        return InputText(label.data(), &value, flags, callback, ctx);
    }


    void
    KineticScrollFrameEnd()
    {
        // Try to detect the user touched and is holding in place.
        // check if user wants to stop scrolling
        ImGuiIO& io = GetIO();
        if (io.MouseDown[0] &&
            //length(io.MouseDelta) < io.MouseDragThreshold
            io.MouseDelta.x == 0 && io.MouseDelta.y == 0
            ) {
            ++pinning_down_frames;
            // cout << "pinning down frames: " << pinning_down_frames << endl;
        } else
            pinning_down_frames = 0;

        const bool lock_scroll = pinning_down_frames >= 2;

        for (auto& [id, state] : scroll_states) {
            if (!state.dragging && !lock_scroll) {
                ImGuiWindow* window = FindWindowByID(id);
                auto& scroll = window->Scroll;
                if (state.velocity.x != 0.0f)
                    SetScrollX(window, scroll.x + state.velocity.x * io.DeltaTime);
                if (state.velocity.y != 0.0f)
                    SetScrollY(window, scroll.y + state.velocity.y * io.DeltaTime);
            }

            const float scroll_speed_decay = 1.0f / 16.0f;
            if (lock_scroll)
                state.velocity = {};
            else
                state.velocity *= std::pow(scroll_speed_decay, io.DeltaTime);

            const float stop_speed = 150.0f;
#ifdef KINETIC_AXIS
            if (std::abs(state.velocity.x) < stop_speed)
                state.velocity.x = 0;
            if (std::abs(state.velocity.y) < stop_speed)
                state.velocity.y = 0;
#else
            if (length(state.velocity) < stop_speed)
                state.velocity = {};
#endif

            state.dragging = false;
        }
        std::erase_if(scroll_states,
                      [](const auto& elem)
                      {
                          return elem.second.velocity == ImVec2{};
                      });
    }


    void
    OpenPopup(const std::string& str_id,
              ImGuiPopupFlags popup_flags)
    {
        OpenPopup(str_id.data(), popup_flags);
    }


    void
    PushID(const std::string& str)
    {
        PushID(str.data(), str.data() + str.size());
    }


    void
    ScrollWhenDraggingOnVoid(const ImVec2& delta,
                             ImGuiMouseButton mouse_button)
    {
        ImGuiWindow* current_window = GetCurrentContext()->CurrentWindow;
        ScrollWhenDraggingOnVoid(current_window->ID, delta, mouse_button);
    }


    // Based on https://github.com/ocornut/imgui/issues/3379#issuecomment-1678718752
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

        // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
        if (g.HoveredId == 0)
            ButtonBehavior(current_window->Rect(),
                           overlay_id,
                           &hovered,
                           &held,
                           button_flags);

        auto& target_scroll = target_window->Scroll;
        auto& state = scroll_states[target_id];
        state.dragging = held;

        if (held) {
            if (delta.x != 0)
                SetScrollX(target_window, target_scroll.x + delta.x);
            if (delta.y != 0)
                SetScrollY(target_window, target_scroll.y + delta.y);

            state.velocity = delta / GetIO().DeltaTime;
            // don't start kinetic scrolling before it's above the threshold
            const float speed_threshold = 300.0f;
#ifdef KINETIC_AXIS
            if (std::abs(state.velocity.x) < speed_threshold)
                state.velocity.x = 0;
            if (std::abs(state.velocity.y) < speed_threshold)
                state.velocity.y = 0;
#else
            if (length(state.velocity) < speed_threshold)
                state.velocity = {};
#endif
            // Don't scroll when not scrollable in that axis.
            if (target_window->ScrollMax.x == 0)
                state.velocity.x = 0;
            if (target_window->ScrollMax.y == 0)
                state.velocity.y = 0;


        }
    }


    bool
    Selectable(const std::string& label,
               bool selected,
               ImGuiSelectableFlags flags,
               const ImVec2& size)
    {
        return Selectable(label.data(), selected, flags, size);
    }


    void
    SeparatorText(const std::string& label)
    {
        SeparatorText(label.data());
    }


    void
    SeparatorTextColored(const ImVec4& color,
                         const std::string& label)
    {
        PushStyleColor(ImGuiCol_Text, color);
        SeparatorText(label.data());
        PopStyleColor();
    }


    template<concepts::arithmetic T>
    bool
    Slider(const std::string& label,
           T& v,
           T v_min,
           T v_max,
           const char* format,
           ImGuiSliderFlags flags)
    {
        return SliderScalar(label.data(),
                            imgui_data_type_v<T>,
                            &v, &v_min, &v_max,
                            format, flags);
    }

    /* --------------------------------------- */
    /* Explicit instantiations for Slider<T>() */
    /* --------------------------------------- */

    template
    bool
    Slider<std::int8_t>(const std::string&,
                        std::int8_t&, std::int8_t, std::int8_t,
                        const char*, ImGuiSliderFlags);
    template
    bool
    Slider<std::uint8_t>(const std::string&,
                         std::uint8_t&, std::uint8_t, std::uint8_t,
                         const char*, ImGuiSliderFlags);

    template
    bool
    Slider<std::int16_t>(const std::string&,
                         std::int16_t&, std::int16_t, std::int16_t,
                         const char*, ImGuiSliderFlags);
    template
    bool
    Slider<std::uint16_t>(const std::string&,
                          std::uint16_t&, std::uint16_t, std::uint16_t,
                          const char*, ImGuiSliderFlags);

    template
    bool
    Slider<std::int32_t>(const std::string&,
                         std::int32_t&, std::int32_t, std::int32_t,
                         const char*, ImGuiSliderFlags);
    template
    bool
    Slider<std::uint32_t>(const std::string&,
                          std::uint32_t&, std::uint32_t, std::uint32_t,
                          const char*, ImGuiSliderFlags);

    template
    bool
    Slider<std::int64_t>(const std::string&,
                         std::int64_t&, std::int64_t, std::int64_t,
                         const char*, ImGuiSliderFlags);
    template
    bool
    Slider<std::uint64_t>(const std::string&,
                          std::uint64_t&, std::uint64_t, std::uint64_t,
                          const char*, ImGuiSliderFlags);

    template
    bool
    Slider<float>(const std::string&,
                  float&, float, float,
                  const char*, ImGuiSliderFlags);
    template
    bool
    Slider<double>(const std::string&,
                   double&, double, double,
                   const char*, ImGuiSliderFlags);


    void
    TextAlignedColored(float align_x,
                       float size_x,
                       const ImVec4& color,
                       const char* fmt,
                       ...)
    {
        std::va_list args;
        va_start(args, fmt);
        TextAlignedColoredV(align_x, size_x, color, fmt, args);
        va_end(args);
    }


    void
    TextAlignedColoredV(float align_x,
                        float size_x,
                        const ImVec4& color,
                        const char* fmt,
                        std::va_list args)
    {
        PushStyleColor(ImGuiCol_Text, color);
        TextAlignedV(align_x, size_x, fmt, args);
        PopStyleColor();
    }


    void
    TextCentered(const char* fmt,
                 ...)
    {
        std::string text;
        std::va_list args;
        va_start(args, fmt);
        try {
            text = utils::cpp_vsprintf(fmt, args);
            va_end(args);
        }
        catch (...) {
            va_end(args);
            throw;
        }

        float window_width = GetContentRegionAvail().x;
        float text_width   = CalcTextSize(text, true).x;

        SetCursorPosX(0.5f * (window_width - text_width));
        Text("%s", text.data());
    }


    bool
    TextLink(const std::string& label)
    {
        return TextLink(label.data());
    }


    bool
    TextLinkOpenURL(const std::string& label)
    {
        return TextLinkOpenURL(label.data());
    }


    bool
    TextLinkOpenURL(const std::string& label,
                    const std::string& url)
    {
        return TextLinkOpenURL(label.data(), url.data());
    }


    void
    TextRight(const char* fmt,
              ...)
    {
        std::va_list args;
        va_start(args, fmt);
        TextAlignedV(1.0f, -FLT_MIN, fmt, args);
        va_end(args);
    }


    void
    TextRightColored(const ImVec4& color,
                     const char* fmt,
                     ...)
    {
        std::va_list args;
        va_start(args, fmt);
        TextRightColoredV(color, fmt, args);
        va_end(args);
    }


    void
    TextRightColoredV(const ImVec4& color,
                     const char* fmt,
                      std::va_list args)
    {
        TextAlignedColoredV(1.0f, -FLT_MIN, color, fmt, args);
    }


    void
    TextUnformatted(const std::string& text)
    {
        TextUnformatted(text.data(), text.data() + text.size());
    }


    template<typename T>
    void
    Value(const std::string& prefix,
          const T& value)
    {
        const std::string fmt = "%s: %" + utils::format(value);
        Text(fmt.data(),
             prefix.data(),
             value);
    }

    /* -------------------------------------- */
    /* Explicit instantiations for Value<T>() */
    /* -------------------------------------- */

    template
    void
    Value<char>(const std::string& prefix,
                const char&);

    template
    void
    Value<std::int8_t>(const std::string& prefix,
                       const std::int8_t&);
    template
    void
    Value<std::uint8_t>(const std::string& prefix,
                        const std::uint8_t&);

    template
    void
    Value<std::int16_t>(const std::string& prefix,
                        const std::int16_t&);
    template
    void
    Value<std::uint16_t>(const std::string& prefix,
                         const std::uint16_t&);

    template
    void
    Value<std::int32_t>(const std::string& prefix,
                        const std::int32_t&);
    template
    void
    Value<std::uint32_t>(const std::string& prefix,
                         const std::uint32_t&);

    template
    void
    Value<std::int64_t>(const std::string& prefix,
                        const std::int64_t&);
    template
    void
    Value<std::uint64_t>(const std::string& prefix,
                         const std::uint64_t&);

    template
    void
    Value<char*>(const std::string& prefix,
                 char* const &);
    template
    void
    Value<const char*>(const std::string& prefix,
                       const char* const &);

    // Specialization for std::string
    template<>
    void
    Value<std::string>(const std::string& prefix,
                       const std::string& value)
    {
        Value<const char*>(prefix, value.data());
    }


    template<typename T>
    void
    ValueWrapped(const std::string& prefix,
                 const T& value)
    {
        const std::string fmt = "%s: %" + utils::format(value);
        TextWrapped(fmt.data(), prefix.data(), value);
    }

    /* --------------------------------------------- */
    /* Explicit instantiations for ValueWrapped<T>() */
    /* --------------------------------------------- */

    template
    void
    ValueWrapped<int8_t>(const std::string&,
                         const std::int8_t&);
    template
    void
    ValueWrapped<uint8_t>(const std::string&,
                          const std::uint8_t&);

    template
    void
    ValueWrapped<int16_t>(const std::string&,
                          const std::int16_t&);
    template
    void
    ValueWrapped<uint16_t>(const std::string&,
                           const std::uint16_t&);

    template
    void
    ValueWrapped<int32_t>(const std::string&,
                          const std::int32_t&);
    template
    void
    ValueWrapped<uint32_t>(const std::string&,
                           const std::uint32_t&);


    template
    void
    ValueWrapped<int64_t>(const std::string&,
                          const std::int64_t&);
    template
    void
    ValueWrapped<uint64_t>(const std::string&,
                           const std::uint64_t&);

    template
    void
    ValueWrapped<char*>(const std::string&,
                        char* const&);
    template
    void
    ValueWrapped<const char*>(const std::string&,
                              const char* const&);

    // Specialization for std::string
    template<>
    void
    ValueWrapped<std::string>(const std::string& prefix,
                              const std::string& value)
    {
        ValueWrapped<const char*>(prefix, value.data());
    }

} // namespace ImGui
