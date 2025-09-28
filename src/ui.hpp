/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UI_HPP
#define UI_HPP

#include <concepts>
#include <string>
#include <vector>

#include <imgui.h>


struct Station;


namespace ui {

    void
    show_favicon(const std::string& favicon);


    void
    show_info_row(const std::string& label,
                  const std::string& value);


    template<std::integral T>
    void
    show_info_row(const std::string& label,
                  T value);


    void
    show_label(const char* fmt,
               ...)
        IM_FMTARGS(1);


    void
    show_link_row(const std::string& label,
                  const std::string& url);


    void
    show_play_button(const Station& station);


    void
    show_station_basic_info(const Station& station,
                            ImGuiID scroll_target);


    void
    show_tags(const std::vector<std::string>& tags,
              ImGuiID scroll_target);

} // namespace ui

#endif
