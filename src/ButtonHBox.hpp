/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BUTTONHBOX_HPP
#define BUTTONHBOX_HPP

#include <functional>
#include <string>
#include <vector>

#include <imgui.h>


struct ButtonHBox {

    using ClickCallbackSignature = void();
    using ClickFunction = std::move_only_function<ClickCallbackSignature>;

    struct Button {
        std::string label = {};
        std::string tooltip = {};
        bool is_default = false;
        ClickFunction on_click = {};
    };

    float halign = 0.5f;
    float valign = -1;
    bool expand = false;
    std::vector<Button> buttons = {};

    ImVec2 button_size   = {};  // recalculated after each add()
    float  buttons_width = 0;   // recalculated after each add()
    float  total_width   = 0;   // recalculated after each add()

    void
    add(Button&& b);

    void
    add(const std::string& label,
        const std::string& tooltip,
        bool is_default,
        ClickFunction on_click);

    void
    add(const std::string& label,
        bool is_default,
        ClickFunction on_click);

    void
    add(const std::string& label,
        ClickFunction on_click);

    void
    show();

private:

    void
    update();

}; // struct ButtonHBox

#endif
