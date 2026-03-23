/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
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
#include "string_utils.hpp"


using std::cout;
using std::endl;


namespace ImGui {


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


        [[maybe_unused]]
        float
        length(const ImVec2& v)
        {
            return std::hypot(v.x, v.y);
        }

    } // namespace



    bool
    Begin(const std::string& name,
          bool* p_open,
          ImGuiWindowFlags flags)
    {
        return Begin(name.data(), p_open, flags);
    }


    bool
    BeginChild(const std::string& str_id,
               const ImVec2& size,
               ImGuiChildFlags child_flags,
               ImGuiWindowFlags window_flags)
    {
        return BeginChild(str_id.data(), size, child_flags, window_flags);
    }


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
    BeginTabBar(const std::string& id,
                ImGuiTabBarFlags flags)
    {
        return BeginTabBar(id.data(), flags);
    }


    bool
    BeginTabItem(const std::string& label,
                 bool* p_open,
                 ImGuiTabItemFlags flags)
    {
        return BeginTabItem(label.data(), p_open, flags);
    }


    bool
    BeginTable(const std::string& id,
               int columns,
               ImGuiTableFlags flags,
               const ImVec2& outer_size,
               float inner_width)
    {
        return BeginTable(id.data(), columns, flags, outer_size, inner_width);
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


    bool
    InputTextWithHint(const std::string& label,
                      const std::string& hint,
                      std::string& value,
                      ImGuiInputTextFlags flags,
                      ImGuiInputTextCallback callback,
                      void* ctx)
    {
        return InputTextWithHint(label.data(), hint.data(), &value, flags, callback, ctx);
    }


    bool
    IsPopupOpen(const std::string& str_id,
                ImGuiPopupFlags flags)
    {
        return IsPopupOpen(str_id.data(), flags);
    }


    void
    OpenPopup(const std::string& str_id,
              ImGuiPopupFlags popup_flags)
    {
        OpenPopup(str_id.data(), popup_flags);
    }


    void
    PushID(const std::string& id)
    {
        PushID(id.data(), id.data() + id.size());
    }


    void
    PushID(std::string_view id)
    {
        PushID(id.data(), id.data() + id.size());
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


    void
    SetItemTooltip(const std::string& text)
    {
        SetItemTooltip("%s", text.data());
    }


    void
    SetTooltip(const std::string& text)
    {
        SetTooltip("%s", text.data());
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
    Text(const std::string& text)
    {
        return Text("%s", text.data());
    }


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
            text = string_utils::cpp_vsprintf(fmt, args);
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
    TextRight(const std::string& text)
    {
        TextRight("%s", text.data());
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
    TextRightColored(const ImVec4& color,
                     const std::string& text)
    {
        TextRightColored(color, "%s", text.data());
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


    void
    TextWrapped(const std::string& text)
    {
        TextWrapped("%s", text.data());
    }


    template<typename T>
    void
    Value(const std::string& prefix,
          const T& value)
    {
        const std::string fmt = "%s: %" + string_utils::format(value);
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
        const std::string fmt = "%s: %" + string_utils::format(value);
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


    ChildGuard::ChildGuard(const char* str_id,
                           const ImVec2& size,
                           ImGuiChildFlags child_flags,
                           ImGuiWindowFlags window_flags)
        noexcept :
        status{BeginChild(str_id, size, child_flags, window_flags)}
    {}


    ChildGuard::ChildGuard(const std::string& str_id,
                           const ImVec2& size,
                           ImGuiChildFlags child_flags,
                           ImGuiWindowFlags window_flags)
        noexcept :
        status{BeginChild(str_id, size, child_flags, window_flags)}
    {}


    ChildGuard::~ChildGuard()
        noexcept
    {
        // Note: ImGui::EndChild() is unconditional
        EndChild();
    }


    ChildGuard::operator bool()
        const noexcept
    {
        return status;
    }


    ComboGuard::ComboGuard(const char* label,
                           const char* preview_value,
                           ImGuiComboFlags flags)
        noexcept :
        status{BeginCombo(label, preview_value, flags)}
    {}


    ComboGuard::ComboGuard(const std::string& label,
                           const std::string& preview_value,
                           ImGuiComboFlags flags)
        noexcept :
        status{BeginCombo(label, preview_value, flags)}
    {}


    ComboGuard::~ComboGuard()
        noexcept
    {
        if (status)
            EndCombo();
    }


    ComboGuard::operator bool()
        const noexcept
    {
        return status;
    }


    DisabledGuard::DisabledGuard(bool disabled)
        noexcept
    {
        BeginDisabled(disabled);
    }


    DisabledGuard::~DisabledGuard()
        noexcept
    {
        EndDisabled();
    }


    FontGuard::FontGuard(ImFont* font,
                         float size)
        noexcept
    {
        PushFont(font, size);
    }


    FontGuard::~FontGuard()
        noexcept
    {
        PopFont();
    }


    GroupGuard::GroupGuard()
        noexcept
    {
        BeginGroup();
    }


    GroupGuard::~GroupGuard()
        noexcept
    {
        EndGroup();
    }


    IDGuard::IDGuard(const char* id)
        noexcept
    {
        PushID(id);
    }


    IDGuard::IDGuard(const std::string& id)
        noexcept
    {
        PushID(id);
    }


    IDGuard::IDGuard(const char* id_begin,
                     const char* id_end)
        noexcept
    {
        PushID(id_begin, id_end);
    }


    IDGuard::IDGuard(std::string_view id)
        noexcept
    {
        PushID(id);
    }


    IDGuard::IDGuard(const void* id)
        noexcept
    {
        PushID(id);
    }


    IDGuard::IDGuard(int id)
        noexcept
    {
        PushID(id);
    }


    IDGuard::~IDGuard()
        noexcept
    {
        PopID();
    }


    IndentGuard::IndentGuard(float amount)
        noexcept:
        width{amount}
    {
        Indent(width);
    }


    IndentGuard::~IndentGuard()
        noexcept
    {
        Unindent(width);
    }


    ItemTooltipGuard::ItemTooltipGuard()
        noexcept :
        status{BeginItemTooltip()}
    {}


    ItemTooltipGuard::~ItemTooltipGuard()
        noexcept
    {
        if (status)
            EndTooltip();
    }


    ItemTooltipGuard::operator bool()
        const noexcept
    {
        return status;
    }


    ItemWidthGuard::ItemWidthGuard(float w)
        noexcept
    {
        PushItemWidth(w);
    }


    ItemWidthGuard::~ItemWidthGuard()
        noexcept
    {
        PopItemWidth();
    }


    PopupGuard::PopupGuard(const char* str_id,
                           ImGuiWindowFlags flags)
        noexcept :
        status{BeginPopup(str_id, flags)}
    {}


    PopupGuard::PopupGuard(const std::string& str_id,
                           ImGuiWindowFlags flags)
        noexcept :
        status{BeginPopup(str_id, flags)}
    {}


    PopupGuard::~PopupGuard()
        noexcept
    {
        if (status)
            EndPopup();
    }


    PopupGuard::operator bool()
        const noexcept
    {
        return status;
    }


    PopupModalGuard::PopupModalGuard(const char* name,
                                     bool* p_open,
                                     ImGuiWindowFlags flags)
        noexcept :
        status{BeginPopupModal(name, p_open, flags)}
    {}

    PopupModalGuard::PopupModalGuard(const std::string& name,
                                     bool* p_open,
                                     ImGuiWindowFlags flags)
        noexcept :
        status{BeginPopupModal(name, p_open, flags)}
    {}


    PopupModalGuard::~PopupModalGuard()
        noexcept
    {
        if (status)
            EndPopup();
    }


    PopupModalGuard::operator bool()
        const noexcept
    {
        return status;
    }


    StyleVarGuard::StyleVarGuard(ImGuiStyleVar idx,
                                 float val)
        noexcept
    {
        PushStyleVar(idx, val);
    }


    StyleVarGuard::StyleVarGuard(ImGuiStyleVar idx,
                                 float x, float y)
        noexcept :
        StyleVarGuard{idx, ImVec2{x, y}}
    {}


    StyleVarGuard::StyleVarGuard(ImGuiStyleVar idx,
                                 const ImVec2& val)
        noexcept
    {
        PushStyleVar(idx, val);
    }


    StyleVarGuard::~StyleVarGuard()
        noexcept
    {
        PopStyleVar();
    }


    TabBarGuard::TabBarGuard(const char* id,
                    ImGuiTabBarFlags flags)
        noexcept :
        status{BeginTabBar(id, flags)}
    {}


    TabBarGuard::TabBarGuard(const std::string& id,
                    ImGuiTabBarFlags flags)
        noexcept :
        status{BeginTabBar(id, flags)}
    {}


    TabBarGuard::~TabBarGuard()
        noexcept
    {
        if (status)
            EndTabBar();
    }


    TabBarGuard::operator bool()
        const noexcept
    {
        return status;
    }


    TabItemGuard::TabItemGuard(const char* label,
                     bool* p_open,
                     ImGuiTabItemFlags flags)
        noexcept :
        status{BeginTabItem(label, p_open, flags)}
    {}


    TabItemGuard::TabItemGuard(const std::string& label,
                               bool* p_open,
                               ImGuiTabItemFlags flags)
        noexcept :
        status{BeginTabItem(label, p_open, flags)}
    {}


    TabItemGuard::~TabItemGuard()
        noexcept
    {
        if (status)
            EndTabItem();
    }


    TabItemGuard::operator bool()
        const noexcept
    {
        return status;
    }


    TableGuard::TableGuard(const char* id,
                   int columns,
                   ImGuiTableFlags flags,
                   const ImVec2& outer_size,
                   float inner_width)
        noexcept :
        status{BeginTable(id, columns, flags, outer_size, inner_width)}
    {}


    TableGuard::TableGuard(const std::string& id,
                           int columns,
                           ImGuiTableFlags flags,
                           const ImVec2& outer_size,
                           float inner_width)
        noexcept :
        status{BeginTable(id, columns, flags, outer_size, inner_width)}
    {}


    TableGuard::~TableGuard()
        noexcept
    {
        if (status)
            EndTable();
    }


    TableGuard::operator bool()
        const noexcept
    {
        return status;
    }


    TooltipGuard::TooltipGuard()
        noexcept :
        status{BeginTooltip()}
    {}


    TooltipGuard::~TooltipGuard()
        noexcept
    {
        if (status)
            EndTooltip();
    }


    TooltipGuard::operator bool()
        const noexcept
    {
        return status;
    }


    WindowGuard::WindowGuard(const char* name,
                             bool* p_open,
                             ImGuiWindowFlags flags)
        noexcept :
        status{Begin(name, p_open, flags)}
    {}


    WindowGuard::WindowGuard(const std::string& name,
                             bool* p_open,
                             ImGuiWindowFlags flags)
        noexcept :
        status{Begin(name, p_open, flags)}
    {}


    WindowGuard::~WindowGuard()
        noexcept
    {
        // Note: ImGui::End() is unconditional
        End();
    }


    WindowGuard::operator bool()
        const noexcept
    {
        return status;
    }

} // namespace ImGui
