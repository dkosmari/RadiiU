/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UI_HPP
#define UI_HPP

#include <concepts>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>


struct Station;


namespace ui {

    extern const ImVec4 label_color;

    void
    show_details_button(const Station& station);

    void
    show_favicon(const Station& station);

    void
    show_favorite_button(const Station& station);

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
    show_play_button(std::shared_ptr<Station>& station);


    void
    show_station_basic_info(const Station& station,
                            ImGuiID scroll_target);


    void
    show_tags(const std::vector<std::string>& tags,
              ImGuiID scroll_target);


    // void
    // open_station_info_popup(const std::string& uuid);

    // void
    // process_station_info_popup();


    void
    show_boxed(const std::string& text,
               const std::string& tooltip,
               ImGuiID scroll_target);

    void
    show_boxed(const std::string& text,
               ImGuiID scroll_target);


    void
    show_last_bounding_box();

} // namespace ui

#endif
