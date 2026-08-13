/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2025-2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include <sdl2xx/vec2.hpp>

#include "UI.hpp"

#include "App.hpp"
#include "BrowserTab.hpp"
#include "cfg.hpp"
#include "FavoritesTab.hpp"
#include "ImageLoader.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "PlayerTab.hpp"
#include "RadioBrowserAPI.hpp"
#include "Station.hpp"
#include "string_utils.hpp"
#include "TabID.hpp"


using namespace std::literals;


namespace UI {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        struct FramedItemExt {
            std::string_view text;
            std::string_view tooltip;
            std::variant<std::monostate, int, std::string> id;
            float width = -1;
            float x;
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        const int favicon_height = 173;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        std::pair<double, double>
        get_scales_for(const sdl::vec2& input,
                       const sdl::vec2& limits);

        void
        show_one_framed_line(const std::vector<FramedItemExt>& line);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        std::pair<double, double>
        get_scales_for(const sdl::vec2& input,
                       const sdl::vec2& limits)
        {
            double x_scale = double(limits.x) / input.x;
            double y_scale = double(limits.y) / input.y;
            return { x_scale, y_scale };
        }


        void
        show_one_framed_line(const std::vector<FramedItemExt>& line)
        {
            bool first = true;
            for (auto& item : line) {
                if (!first)
                    ImGui::SameLine();
                first = false;
                FramedText(
                    item.text,
                    item.tooltip,
                    {
                        .id = item.id,
                        .width = item.width
                    }
                );
            }
        }

    } // namespace


    /*------------------*/
    /* Public variables */
    /*------------------*/

    const ImVec2 play_button_size = {114, 114};


    /*------------------*/
    /* Public functions */
    /*------------------*/

    const ImVec4&
    get_label_color()
        noexcept
    {
        return ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive);
    }


    const ImVec2
    get_small_button_size()
    {
        const auto& style = ImGui::GetStyle();
        float width = (play_button_size.x - style.ItemSpacing.x) / 2;
        float height = ImGui::GetFrameHeight();
        return { width, height };
    }


    void
    FavIcon(const Station& station)
    {
        using namespace ImGui::RAII;
        if (station.favicon.empty())
            return;

        using sdl::vec2;

        const vec2 max_size = {400, favicon_height};
        auto icon = ImageLoader::get(station.favicon, max_size);
        vec2 icon_size = icon->get_size();
        auto [scale_x, scale_y] = get_scales_for(icon_size, max_size);
        auto scale = std::fmin(scale_x, scale_y);
        vec2 display_size = vec2{ scale * sdl::vec2f{icon_size} };
        Image(*icon, display_size);
        ImGui::SetItemTooltip(station.favicon);
    }


    void
    FavoriteButton(const Station& station)
    {
        if (FavoritesTab::contains(station)) {
            if (ImGui::Button(ICON_FA_HEART, get_small_button_size())) // ♥
                FavoritesTab::remove(station);
        } else {
            if (ImGui::Button(ICON_FA_HEART_O, get_small_button_size())) // ♡
                FavoritesTab::add(station);
        }
    }


    void
    Image(const sdl::texture& texture,
          const sdl::vec2& size,
          const sdl::vec2f& uv0,
          const sdl::vec2f& uv1)
    {
        ImGui::Image(reinterpret_cast<ImTextureID>(texture.data()),
                     ImGui::ToVec2(size),
                     ImGui::ToVec2(uv0),
                     ImGui::ToVec2(uv1));
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
        return ImGui::ImageButton(str_id,
                                  reinterpret_cast<ImTextureID>(texture.data()),
                                  ImGui::ToVec2(size),
                                  ImGui::ToVec2(uv0),
                                  ImGui::ToVec2(uv1),
                                  ImGui::ToVec4(bg_color),
                                  ImGui::ToVec4(tint_color));
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
        auto window_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(0.5f * (window_width - size.x));
        Image(texture, size, uv0, uv1);
    }


    void
    ImageCentered(const sdl::texture& texture,
                  const sdl::vec2f& uv0,
                  const sdl::vec2f& uv1)
    {
        ImageCentered(texture, texture.get_size(), uv0, uv1);
    }


    void
    InfoRow(const std::string& label,
            const std::string& value)
    {
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        Label(label);
        // show_last_bounding_box();

        ImGui::TableNextColumn();
        ImGui::TextWrapped(value);
        // show_last_bounding_box();
    }


    void
    InfoRow(const std::string& label,
            const std::vector<std::string>& values)
    {
        InfoRow(label, string_utils::to_csv(values));
    }


    void
    LinkRow(const std::string& label,
                  const std::string& url)
    {
        using namespace ImGui::RAII;

        ImGui::TableNextRow();

        ID label_id{label};

        ImGui::TableNextColumn();
        Label(label);

        ImGui::TableNextColumn();
        TextLinkOpenURL(url);
    }


    void
    PlayButton(StationPtr& station)
    {
        using namespace ImGui::RAII;

        Font font{nullptr, 64};
        if (PlayerTab::is_playing(*station)) {
            auto playing_color = ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered);
            StyleColor text_color{ImGuiCol_Text, playing_color};
            if (ImGui::Button(ICON_FA_STOP, play_button_size))
                PlayerTab::stop();
        } else {
            if (ImGui::Button(ICON_FA_PLAY, play_button_size)) {
                if (cfg::state.switch_to_player)
                    App::set_tab(TabID::player);
                PlayerTab::play(station);
                // TODO: call RadioBrowserAPI directly.
                BrowserTab::send_click(station);
            }
        }
    }


    void
    StationInfo(const Station& station,
                bool show_extra)
    {
        using namespace ImGui::RAII;

        if (Child favicon_and_name{"favicon_and_name",
                                   {0, favicon_height},
                                   ImGuiChildFlags_NavFlattened}) {

            if (!station.favicon.empty()) {
                FavIcon(station);
                ImGui::SameLine();
            }

            Group name_and_homepage;

            if (ImGui::TextAligned(0.0f, -1, station.name)) {
                if (ItemTooltip name_tooltip{}) {
                    TextWrapPos wrap_at{900};
                    ImGui::Text(station.name);
                }
            }

            if (!station.homepage.empty())
                TextLinkOpenURL(station.homepage);

        }

        ImGuiChildFlags details_flags = ImGuiChildFlags_NavFlattened;
        if (station.expanded)
            details_flags |= ImGuiChildFlags_AutoResizeY;
        if (Child details{"details",
                          {0.0f, station.expanded ? 0 : ImGui::GetFrameHeight()},
                          details_flags}) {

            // Font font{nullptr, 28};

            std::vector<FramedItem> items;
            if (!station.countrycode.empty())
                items.emplace_back(ICON_FA_FLAG_O " " + station.countrycode,
                                   BrowserTab::get_country_name(station.countrycode));
            for (auto& lang : station.language)
                items.emplace_back(ICON_FA_LANGUAGE " " + lang,
                                   "");

            if (show_extra) {
                if (station.click_count && station.click_trend)
                    items.emplace_back(std::format(ICON_FA_BAR_CHART " {} ({:+d})",
                                                   *station.click_count,
                                                   *station.click_trend),
                                       "Daily total clicks and trend.");

                if (station.codec)
                    items.emplace_back(ICON_FA_FLASK " " + *station.codec,
                                       "The codec used in this broadcast.");

                if (station.bitrate)
                    items.emplace_back(std::format(ICON_FA_HEADPHONES " {} kbps",
                                                   *station.bitrate),
                                       "The advertised stream quality.");
            }

            for (auto& tag : station.tags)
                items.emplace_back(ICON_FA_TAG " " + tag);

            station.expanded = FramedList(items, !station.expanded)
                ? !station.expanded
                : station.expanded;
        }
    }


    ImVec2
    CalcFramedTextSize(std::string_view text,
                       float width)
    {
        auto& style = ImGui::GetStyle();
        auto size = ImGui::CalcTextSize(text) + 2 * style.FramePadding;
        if (width > 0)
            size.x = width;
        return size;
    }


    ImVec2
    CalcFramedTextSize(const FramedItem& item)
    {
        return CalcFramedTextSize(item.text, item.spec.width);
    }


    void
    FramedText(std::string_view text,
               std::string_view tooltip,
               const FramedSpec& spec)
    {
        using namespace ImGui::RAII;

        const auto& style = ImGui::GetStyle();
        const float padding = 2 * style.FramePadding.x;

        std::optional<ID> spec_id;
        if (auto int_id = get_if<int>(&spec.id))
            spec_id.emplace(*int_id);
        else if (auto str_id = get_if<std::string>(&spec.id))
            spec_id.emplace(*str_id);

        ID text_id{text};

        auto text_size = CalcFramedTextSize(text, spec.width);
        if (Child boxed_child{
                "boxed",
                text_size,
                ImGuiChildFlags_FrameStyle
            }) {

            if (spec.width > 0) {
                if (spec.width > padding) {
                    ImGui::TextAligned(0, spec.width - padding, text);
                }
            } else {
                ImGui::Text(text);
            }

        }
        if (!tooltip.empty())
            ImGui::SetItemTooltip(tooltip);

    }


    void
    FramedText(const FramedItem& item)
    {
        FramedText(item.text, item.tooltip, item.spec);
    }


    bool
    FramedList(const std::vector<FramedItem>& items,
               bool only_first_line)
    {
        const std::string ellipsis = "…";
        const float ellipsis_width = CalcFramedTextSize(ellipsis).x;
        const auto& style = ImGui::GetStyle();
        const float spacing = style.ItemSpacing.x;
        float total_width = ImGui::GetContentRegionAvail().x;
        const float frame_padding = 2 * style.FramePadding.x;

        float cur_x = 0;
        std::vector<FramedItemExt> line;
        std::size_t idx;
        bool stopped_early = false;

        for (idx = 0; idx < items.size(); ++idx) {
            auto& item = items[idx];
            auto& [text, tooltip, spec] = item;
            float width = CalcFramedTextSize(item).x;
            line.emplace_back(text,
                              tooltip,
                              (holds_alternative<std::monostate>(spec.id)
                               ? static_cast<int>(idx)
                               : spec.id),
                              width,
                              cur_x);

            cur_x += width + spacing;

            if (cur_x >= total_width) {
                // Next item will be out of bounds, so stop accumulating.
                if (only_first_line) {
                    stopped_early = true;
                    break;
                }

                auto& last = line.back();
                if (line.size() == 1) {
                    // If only one item on this line, may need to truncate it.
                    if (last.x + last.width > total_width)
                        last.width = total_width - last.x;
                } else {
                    // Multiple items on this line, so it's safe to pop one.
                    if (last.x + last.width > total_width) {
                        line.pop_back();
                        --idx;
                    }
                }


                show_one_framed_line(line);

                line.clear();
                cur_x = 0;
                total_width = ImGui::GetContentRegionAvail().x;
            }
        }

        // The last line is handled here.

        if (line.empty())
            return false;

        if (stopped_early) {
            // Stopped early, so we show the ellipsis button.
            // That means we need to pop items until the ellipsis fits.
            while (!line.empty() &&
                   line.back().x + frame_padding + spacing + ellipsis_width > total_width) {
                line.pop_back();
            }

            // If there's at least one item, check if we need to shrink it to fit the ellipsis.
            if (!line.empty()) {
                auto& last = line.back();
                const float room_left = total_width - last.x;
                if (last.width + spacing + ellipsis_width > room_left) {
                    // shrink last item
                    last.width = room_left - spacing - ellipsis_width;
                }
            }
        }

        show_one_framed_line(line);

        if (stopped_early) {
            if (!line.empty())
                ImGui::SameLine();
            auto available = ImGui::GetContentRegionAvail();
            float offset = available.x - ellipsis_width;
            if (offset > 0)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
            bool result = ImGui::Button(ellipsis);
            ImGui::SetItemTooltip("Show more.");
            return result;
        }

        return false;
    }


    void
    BoundingBox()
    {
        auto min = ImGui::GetItemRectMin();
        auto max = ImGui::GetItemRectMax();
        ImU32 col = ImGui::GetColorU32(ImVec4{1.0f, 0.0f, 0.0f, 0.5f});
        auto draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(min, max, col);
    }


    void
    Label(std::string_view label)
    {
        auto available = ImGui::GetContentRegionAvail();
        auto label_size = ImGui::CalcTextSize(label);
        float offset = available.x - label_size.x;
        if (offset > 0)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        ImGui::TextColored(get_label_color(), label);
        // BoundingBox();
    }


    bool
    TextLinkOpenURL(const std::string& url)
    {
        using namespace ImGui::RAII;
#if 1
        const auto& style = ImGui::GetStyle();
        const auto link_color = style.Colors[ImGuiCol_TextLink];
        float url_width = ImGui::CalcTextSize(url).x;
        {
            StyleColor text_color{ImGuiCol_Text, link_color};
            ImGui::TextUnformatted(ICON_FA_LINK);
            ImGui::SameLine();
            if (ImGui::TextAligned(0, -1, url))
                url_width = -1; // got truncated
        }

        // Draw underline
        auto draw_list = ImGui::GetWindowDrawList();
        auto min = ImGui::GetItemRectMin();
        auto max = ImGui::GetItemRectMax();
        auto baked = ImGui::GetFontBaked();
        float underline_y = max.y
            + std::floor(baked->Descent * style.FontSizeBase / baked->Size * 0.2f);
        float underline_max = url_width < 0 ? max.x : min.x + url_width;
        draw_list->AddLineH(min.x,
                            underline_max,
                            underline_y,
                            ImGui::GetColorU32(link_color),
                            2.0f);

        // Mouse hover
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        // Left-click
        if (ImGui::IsItemClicked()) {
            // TODO: show QR popup
            LOG_DEBUG("clicked on link: {}", url);
            return true;
        }

        // Right-click
        if (PopupContextItem context_menu{"context-" + url}) {
            if (ImGui::MenuItem("Copy link to clipboard"))
                ImGui::SetClipboardText(url);
        }
        return false;
#else
        TextWrapPos wrapper{0};
        return ImGui::TextLinkOpenURL(url);
#endif
    }


} // namespace UI
