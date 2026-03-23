/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef IMGUI_EXTRAS_HPP
#define IMGUI_EXTRAS_HPP

#include <concepts>
#include <cstdarg>
#include <cstdint>
#include <string>
#include <string_view>
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


        template<typename T>
        concept vec2 = std::convertible_to<decltype(T::x), float> &&
                       std::convertible_to<decltype(T::y), float>;

    } // namespace concepts


    bool
    Begin(const std::string& name,
          bool* p_open = nullptr,
          ImGuiWindowFlags flags = 0);


    bool
    BeginChild(const std::string& str_id,
               const ImVec2& size = ImVec2(0, 0),
               ImGuiChildFlags child_flags = 0,
               ImGuiWindowFlags window_flags = 0);

    bool
    BeginCombo(const std::string& label,
               const std::string& preview,
               ImGuiComboFlags flags = 0);


    bool
    BeginPopup(const std::string& id,
               ImGuiWindowFlags flags = 0);


    bool
    BeginPopupModal(const std::string& name,
                    bool* p_open = nullptr,
                    ImGuiWindowFlags flags = 0);


    bool
    BeginTabBar(const std::string& id,
                ImGuiTabBarFlags flags = 0);


    bool
    BeginTabItem(const std::string& label,
                 bool* p_open = nullptr,
                 ImGuiTabItemFlags flags = 0);


    bool
    BeginTable(const std::string& id,
               int columns,
               ImGuiTableFlags flags = 0,
               const ImVec2& outer_size = {},
               float inner_width = 0);


    bool
    Button(const std::string& label,
           const ImVec2& size = ImVec2(0, 0));


    ImVec2
    CalcTextSize(const std::string& text,
                 bool hide_text_after_double_hash = false,
                 float wrap_width = -1.0f);


    template<concepts::arithmetic T>
    bool
    Drag(const std::string& label,
         T& v,
         T v_min,
         T v_max,
         float speed = 1.0f,
         const char* format = nullptr,
         ImGuiSliderFlags flags = 0);


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
    Input(const std::string& label,
          T& v,
          T step = T{1},
          T step_fast = T{100},
          const char* format = nullptr,
          ImGuiInputTextFlags flags = 0);


    bool
    InputText(const std::string& label,
              std::string& value,
              ImGuiInputTextFlags flags = 0,
              ImGuiInputTextCallback callback = nullptr,
              void* ctx = nullptr);


    bool
    InputTextWithHint(const std::string& label,
                      const std::string& hint,
                      std::string& value,
                      ImGuiInputTextFlags flags = 0,
                      ImGuiInputTextCallback callback = nullptr,
                      void* ctx = nullptr);


    bool
    IsPopupOpen(const std::string& str_id,
                ImGuiPopupFlags flags = 0);


    void
    OpenPopup(const std::string& str_id,
              ImGuiPopupFlags popup_flags = 0);


    void
    PushID(const std::string& id);


    void
    PushID(std::string_view id);


    bool
    Selectable(const std::string& label,
               bool selected = false,
               ImGuiSelectableFlags flags = 0,
               const ImVec2& size = ImVec2(0, 0));


    void
    SeparatorText(const std::string& label);


    void
    SeparatorTextColored(const ImVec4& color,
                         const std::string& label);


    void
    SetItemTooltip(const std::string& text);


    void
    SetTooltip(const std::string& text);


    template<concepts::arithmetic T>
    bool
    Slider(const std::string& label,
           T& v,
           T v_min,
           T v_max,
           const char* format = nullptr,
           ImGuiSliderFlags flags = 0);


    void
    Text(const std::string& text);


    void
    TextAlignedColored(float align_x,
                       float size_x,
                       const ImVec4& color,
                       const char* fmt,
                       ...)
        IM_FMTARGS(4);


    void
    TextAlignedColoredV(float align_x,
                        float size_x,
                        const ImVec4& color,
                        const char* fmt,
                        std::va_list args)
        IM_FMTLIST(4);


    void
    TextCentered(const char* fmt,
                 ...)
        IM_FMTARGS(1);


    bool
    TextLink(const std::string& label);


    bool
    TextLinkOpenURL(const std::string& label);

    bool
    TextLinkOpenURL(const std::string& label,
                    const std::string& url);


    void
    TextRight(const char* fmt,
              ...)
        IM_FMTARGS(1);


    void
    TextRight(const std::string& text);


    void
    TextRightColored(const ImVec4& color,
                     const char* fmt,
                     ...)
        IM_FMTARGS(2);

    void
    TextRightColored(const ImVec4& color,
                     const std::string& text);


    void
    TextRightColoredV(const ImVec4& color,
                     const char* fmt,
                      std::va_list args)
        IM_FMTLIST(2);


    void
    TextUnformatted(const std::string& text);


    void
    TextWrapped(const std::string& text);


    template<concepts::vec2 V>
    ImVec2
    ToVec2(const V& v)
    {
        return ImVec2(v.x, v.y);
    }


    template<typename T>
    requires std::convertible_to<decltype(T::r), float> &&
             std::convertible_to<decltype(T::g), float> &&
             std::convertible_to<decltype(T::b), float> &&
             std::convertible_to<decltype(T::a), float>
    ImVec4
    ToVec4(const T& c)
    {
        return ImVec4(c.r, c.g, c.b, c.a);
    }


    template<typename T>
    void
    Value(const std::string& prefix,
          const T& value);


    template<typename T>
    void
    ValueWrapped(const std::string& prefix,
                 const T& value);


    struct ChildGuard {

        ChildGuard(const char* str_id,
                   const ImVec2& size = ImVec2(0, 0),
                   ImGuiChildFlags child_flags = 0,
                   ImGuiWindowFlags window_flags = 0)
            noexcept;

        ChildGuard(const std::string& str_id,
                   const ImVec2& size = ImVec2(0, 0),
                   ImGuiChildFlags child_flags = 0,
                   ImGuiWindowFlags window_flags = 0)
            noexcept;

        // forbid moving
        ChildGuard(ChildGuard&&) = delete;

        ~ChildGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct ChildGuard


    struct ComboGuard {

        ComboGuard(const char* label,
                   const char* preview_value,
                   ImGuiComboFlags flags = 0)
            noexcept;

        ComboGuard(const std::string& label,
                   const std::string& preview_value,
                   ImGuiComboFlags flags = 0)
            noexcept;

        // forbid moving
        ComboGuard(ComboGuard&&) = delete;

        ~ComboGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct ComboGuard


    struct DisabledGuard {

        DisabledGuard(bool disabled)
            noexcept;

        // forbid moving
        DisabledGuard(DisabledGuard&&) = delete;

        ~DisabledGuard()
            noexcept;

    }; // struct DisabledGuard


    struct FontGuard {

        FontGuard(ImFont* font,
                  float size)
            noexcept;

        // forbid moving
        FontGuard(FontGuard&&) = delete;

        ~FontGuard()
            noexcept;

    }; // struct FontGuard


    struct GroupGuard {

        GroupGuard()
            noexcept;

        // forbid moving
        GroupGuard(GroupGuard&&) = delete;

        ~GroupGuard()
            noexcept;

    }; // struct GroupGuard


    struct IDGuard {

        IDGuard(const char* id)
            noexcept;

        IDGuard(const std::string& id)
            noexcept;

        IDGuard(const char* id_begin,
                const char* id_end)
            noexcept;

        IDGuard(std::string_view id)
            noexcept;

        explicit
        IDGuard(const void* id)
            noexcept;

        explicit
        IDGuard(int id)
            noexcept;

        // forbid moving
        IDGuard(IDGuard&&) = delete;

        ~IDGuard()
            noexcept;

    }; // struct IDGuard


    struct IndentGuard {

        IndentGuard(float amount = 0)
            noexcept;

        // forbid movign
        IndentGuard(IndentGuard&&) = delete;

        ~IndentGuard()
            noexcept;

    private:

        const float width;

    }; // struct IndentGuard


    struct ItemTooltipGuard {

        ItemTooltipGuard()
            noexcept;

        // forbid moving
        ItemTooltipGuard(ItemTooltipGuard&&) = delete;

        ~ItemTooltipGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct ItemTooltipGuard


    struct ItemWidthGuard
    {

        ItemWidthGuard(float w)
            noexcept;

        // forbid moving
        ItemWidthGuard(ItemWidthGuard&&) = delete;

        ~ItemWidthGuard()
            noexcept;

    }; // struct ItemWidthGuard


    struct PopupGuard {

        PopupGuard(const char* str_id,
                   ImGuiWindowFlags flags = 0)
            noexcept;

        PopupGuard(const std::string& str_id,
                   ImGuiWindowFlags flags = 0)
            noexcept;

        // forbid moving
        PopupGuard(PopupGuard&&) = delete;

        ~PopupGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct PopupGuard


    struct PopupModalGuard {

        PopupModalGuard(const char* name,
                        bool* p_open = nullptr,
                        ImGuiWindowFlags flags = 0)
            noexcept;

        PopupModalGuard(const std::string& name,
                        bool* p_open = nullptr,
                        ImGuiWindowFlags flags = 0)
            noexcept;

        // forbid moving
        PopupModalGuard(PopupModalGuard&&) = delete;

        ~PopupModalGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct PopupModalGuard


    struct StyleVarGuard {

        StyleVarGuard(ImGuiStyleVar idx,
                      float val)
            noexcept;

        StyleVarGuard(ImGuiStyleVar idx,
                      const ImVec2& val)
            noexcept;

        StyleVarGuard(ImGuiStyleVar idx,
                      float x,
                      float y)
            noexcept;

        template<concepts::vec2 V>
        StyleVarGuard(ImGuiStyleVar idx,
                      const V& val)
            noexcept :
            StyleVarGuard{idx, ToVec2(val)}
        {}

        // forbid moving
        StyleVarGuard(StyleVarGuard&&) = delete;

        ~StyleVarGuard()
            noexcept;

    }; // struct StyleVarGuard


    struct TabBarGuard {

        TabBarGuard(const char* id,
                    ImGuiTabBarFlags flags = 0)
            noexcept;

        TabBarGuard(const std::string& id,
                    ImGuiTabBarFlags flags = 0)
            noexcept;

        // forbid moving
        TabBarGuard(TabBarGuard&&) = delete;

        ~TabBarGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct TabBarGuard


    struct TabItemGuard {

        TabItemGuard(const char* label,
                     bool* p_open = nullptr,
                     ImGuiTabItemFlags flags = 0)
            noexcept;

        TabItemGuard(const std::string& label,
                     bool* p_open = nullptr,
                     ImGuiTabItemFlags flags = 0)
            noexcept;

        // forbid moving
        TabItemGuard(TabItemGuard&& other) = delete;

        ~TabItemGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct TabItemGuard


    struct TableGuard {

        TableGuard(const char* id,
                   int columns,
                   ImGuiTableFlags flags = 0,
                   const ImVec2& outer_size = {0, 0},
                   float inner_width = 0)
            noexcept;

        TableGuard(const std::string& id,
                   int columns,
                   ImGuiTableFlags flags = 0,
                   const ImVec2& outer_size = {0, 0},
                   float inner_width = 0)
            noexcept;


        // forbid moving
        TableGuard(TableGuard&&) = delete;

        ~TableGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct TableGuard


    struct TooltipGuard {

        TooltipGuard()
            noexcept;

        // forbid moving
        TooltipGuard(TooltipGuard&&) = delete;

        ~TooltipGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct TooltipGuard


    struct WindowGuard {

        WindowGuard(const char* name,
                    bool* p_open = nullptr,
                    ImGuiWindowFlags flags = 0)
            noexcept;

        WindowGuard(const std::string& name,
                    bool* p_open = nullptr,
                    ImGuiWindowFlags flags = 0)
            noexcept;

        // forbid moving
        WindowGuard(WindowGuard&&) = delete;

        ~WindowGuard()
            noexcept;

        explicit
        operator bool()
            const noexcept;

    private:

        const bool status;

    }; // struct WindowGuard

} // namespace ImGui

#endif
