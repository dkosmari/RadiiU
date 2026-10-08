/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cmath>
#include <ranges>
#include <utility>

#include <imgui_stdlib.h>

#include "ButtonHBox.hpp"

namespace {

    ImVec2
    max(const ImVec2& a,
        const ImVec2& b)
    {
        return {
            std::fmax(a.x, b.x),
            std::fmax(a.y, b.y)
        };
    }

} // namespace


void
ButtonHBox::add(Button&& button)
{
    buttons.push_back(std::move(button));
    update_layout();
}


void
ButtonHBox::add(const std::string& label,
                const std::string& tooltip,
                bool is_default,
                ClickFunction on_click)
{
    buttons.emplace_back(label, tooltip, is_default, std::move(on_click));
    update_layout();
}


void
ButtonHBox::add(const std::string& label,
                bool is_default,
                ClickFunction on_click)
{
    add(label, {}, is_default, std::move(on_click));
}


void
ButtonHBox::add(const std::string& label,
                ClickFunction on_click)
{
    add(label, {}, false, std::move(on_click));
}


void
ButtonHBox::show()
{
    if (buttons.empty())
        return;

    const auto available = ImGui::GetContentRegionAvail();

    // Special handling: for single button, spread is ignored.
    if (spread && buttons.size() == 1) {
        spread = false;
        update_layout();
    }

    if (halign >= 0) {
        const float empty_hspace = available.x - allocated_size.x;
        if (empty_hspace > 0)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + empty_hspace * halign);
    }

    if (valign >= 0) {
        const float empty_vspace = available.y - allocated_size.y;
        if (empty_vspace > 0)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + empty_vspace * valign);
    }

    float spacing = -1; // use ItemSpacing.x
    const auto n = buttons.size();
    if (spread && n > 1)
        spacing = (allocated_size.x - buttons_width) / (n - 1);

    for (auto [idx, b] : buttons | std::views::enumerate) {
        if (idx > 0)
            ImGui::SameLine(0, spacing);

        if (ImGui::Button(b.label, uniform_size))
            if (b.on_click)
                b.on_click();

        if (!b.tooltip.empty())
            ImGui::SetItemTooltip(b.tooltip);

        if (b.is_default)
            ImGui::SetItemDefaultFocus();
    }
}


float
ButtonHBox::get_height_with_spacing()
    const
{
    const auto& style = ImGui::GetStyle();
    return style.ItemSpacing.y + allocated_size.y;
}


void
ButtonHBox::update_layout()
{
    if (buttons.empty())
        return;

    const auto& style = ImGui::GetStyle();

    const auto n = buttons.size();

    uniform_size = {};
    if (uniform) {
        for (const auto& button : buttons) {
            auto size = ImGui::CalcTextSize(button.label) + 2 * style.FramePadding;
            uniform_size = max(uniform_size, size);
        }
        buttons_width = n * uniform_size.x;
    } else {
        buttons_width = 0;
        for (const auto& button : buttons) {
            auto size = ImGui::CalcTextSize(button.label) + 2 * style.FramePadding;
            buttons_width += size.x;
        }
    }

    if (spread)
        allocated_size.x = ImGui::GetContentRegionAvail().x;
    else
        allocated_size.x = buttons_width + (n - 1) * style.ItemSpacing.x;

    allocated_size.y = ImGui::GetFrameHeight();
}
