/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UI_UTILS_HPP
#define UI_UTILS_HPP

#include <string>
#include <vector>

#include <imgui.h>


namespace ui_utils {

    void
    show_favicon(const std::string& favicon);


    void
    show_tags(const std::vector<std::string>& tags,
              ImGuiID scroll_target);

} // namespace ui_utils

#endif
