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
#include "CountryManager.hpp"
#include "FavoritesTab.hpp"
#include "IconsFontAwesome4.h"
#include "ImageLoader.hpp"
#include "LogManager.hpp"
#include "PlayerTab.hpp"
#include "RadioBrowserAPI.hpp"
#include "Settings.hpp"
#include "Station.hpp"
#include "string_utils.hpp"
#include "TabID.hpp"
#include "TraceFunction.hpp"
#include "tracer.hpp"


using std::cout;
using std::endl;

using namespace std::literals;

using Settings::cfg;


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

        constexpr const char* key_smooth_scroll_start_pos_x = "smooth_scroll.start_pos.x";
        constexpr const char* key_smooth_scroll_start_pos_y = "smooth_scroll.start_pos.y";
        constexpr const char* key_smooth_scroll_final_pos_x = "smooth_scroll.final_pos.x";
        constexpr const char* key_smooth_scroll_final_pos_y = "smooth_scroll.final_pos.y";
        constexpr const char* key_smooth_scroll_start_time  = "smooth_scroll.start_time";
        constexpr const char* key_smooth_scroll_duration    = "smooth_scroll.duration";


        /*-----------*/
        /* Variables */
        /*-----------*/


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        template<typename Pt>
        Pt
        cubic_hermite_spline(const Pt& p0,
                             const Pt& p1,
                             const Pt& d0,
                             const Pt& d1,
                             float t);

        void
        erase_from_storage(ImGuiStorage* storage,
                           ImGuiID id);

        std::pair<double, double>
        get_scales_for(const sdl::vec2& input,
                       const sdl::vec2& limits);

        void
        show_one_framed_line(const std::vector<FramedItemExt>& line);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        template<typename Pt>
        Pt
        cubic_hermite_spline(const Pt& p0,
                             const Pt& p1,
                             const Pt& d0,
                             const Pt& d1,
                             float t)
        {
            if (t < 0)
                return p0;
            if (t > 1)
                return p1;
            Pt a =  2 * p0 - 2 * p1 +     d0 + d1;
            Pt b = -3 * p0 + 3 * p1 - 2 * d0 - d1;
            Pt c =                        d0;
            Pt d =      p0;
            return a * t*t*t + b * t*t + c * t + d;
        }


        void
        erase_from_storage(ImGuiStorage* storage,
                           ImGuiID id)
        {
            auto it = std::ranges::lower_bound(storage->Data,
                                               id,
                                               {},
                                               &ImGuiStoragePair::key);
            if (it != storage->Data.end() && it->key == id)
                storage->Data.erase(it);
        }


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
        return ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
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
            ImGui::SetItemTooltip("Click to remove station from favorites.");
        } else {
            if (ImGui::Button(ICON_FA_HEART_O, get_small_button_size())) // ♡
                FavoritesTab::add(station);
            ImGui::SetItemTooltip("Click to add station to favorites.");
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
    InfoRowOpt(const std::string& label,
               const std::optional<std::string>& value)
    {
        if (value)
            InfoRow(label, *value);
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

        if (!url.empty())
            TextLinkOpenURL(url);
    }


    void
    PlayButton(StationPtr& station)
    {
        using namespace ImGui::RAII;

        Font font{nullptr, 76};
        if (PlayerTab::is_playing(*station)) {
            auto playing_color = ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered);
            StyleColor text_color{ImGuiCol_Text, playing_color};
            if (ImGui::Button(ICON_FA_STOP, play_button_size))
                PlayerTab::stop();
        } else {
            if (ImGui::Button(ICON_FA_PLAY, play_button_size)) {
                if (cfg.switch_to_player)
                    App::set_tab(TabID::player);
                PlayerTab::play(station);
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

            std::vector<FramedItem> items;
            if (!station.countrycode.empty())
                items.emplace_back(CountryManager::get_utf8(station.countrycode),
                                   CountryManager::get_name(station.countrycode));
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

            if (station.expanded)
                station.expanded = !FramedListFull("items_full", items);
            else
                station.expanded = FramedListBrief("items_brief", items);

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

            bool truncated = false;
            if (spec.width > 0) {
                if (spec.width > padding) {
                    truncated = ImGui::TextAligned(0, spec.width - padding, text);
                }
            } else {
                ImGui::Text(text);
            }

            // When no explicit tooltip given, show the text that would be truncated.
            std::string actual_tooltip;
            if (!tooltip.empty())
                actual_tooltip = tooltip;
            else if (truncated)
                actual_tooltip = text;

            // NOTE: can't use ImGui::SetItemTooltip() here because of messy hovered detection.
            if (!actual_tooltip.empty())
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ForTooltip))
                    ImGui::SetTooltip(actual_tooltip);
        }
    }


    void
    FramedText(const FramedItem& item)
    {
        FramedText(item.text, item.tooltip, item.spec);
    }


    bool
    FramedListBrief(const std::string& str_id,
                    const std::vector<FramedItem>& items)
    {
        using namespace ImGui::RAII;

        // Early out: no items.
        if (items.empty())
            return false;

        ID id{str_id};

        const std::string more_label = ICON_FA_PLUS_SQUARE;
        const float more_width = CalcFramedTextSize(more_label).x;

        const auto& style = ImGui::GetStyle();
        const float spacing = style.ItemSpacing.x;
        const float framed_ellipsis_width = CalcFramedTextSize("…").x;
        const float min_item_width = framed_ellipsis_width;
        float total_width = ImGui::GetContentRegionAvail().x;

        float cur_x = 0;
        std::vector<FramedItemExt> line;
        std::size_t idx;

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

            // NOTE: stop once the right side of the item is cut off
            if (cur_x + width >= total_width)
                break;

            cur_x += width + spacing;
        }

        // If not all items were added to the line.
        bool need_more = idx + 1 < items.size();

        // If last item is cut off.
        if (!line.empty()
            && line.back().x + line.back().width > total_width)
            need_more = true;

        // Exception: there's only one item in total, we never add a "more" button.
        if (items.size() == 1)
            need_more = false;

        if (need_more) {
            // Update total_width boundary to make room for the "more" button.
            total_width -= spacing + more_width;

            // Pop every item that would shrink too much to fit.
            while (!line.empty()) {
                const float left = line.back().x;
                const float right = left + line.back().width;
                const float min_right = left + min_item_width;
                if (right <= total_width)
                    break; // right side fits, we can stop
                if (min_right <= total_width)
                    break; // shrunk item fits,
                line.pop_back();
            }

        }

        // The last item might need to be shrunk.
        if (!line.empty()) {
            const float left = line.back().x;
            const float right = left + line.back().width;
            if (right > total_width)
                line.back().width = total_width - left;
        }

        show_one_framed_line(line);

        if (need_more) {
            ImGui::SameLine();
            // Put the button all the way to the right.
            const auto available = ImGui::GetContentRegionAvail();
            const float offset = available.x - more_width;
            if (offset > 0)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
            bool result = ImGui::Button(more_label);
            ImGui::SetItemTooltip("Show more.");
            return result;
        } else
            return false;
    }


    bool
    FramedListFull(const std::string& str_id,
                   const std::vector<FramedItem>& items)
    {
        using namespace ImGui::RAII;

        // Early out: no items.
        if (items.empty())
            return false;

        ID id{str_id};

        const std::string less_label = ICON_FA_MINUS_SQUARE;
        const float less_width = CalcFramedTextSize(less_label).x;

        const auto& style = ImGui::GetStyle();
        const float spacing = style.ItemSpacing.x;
        const float framed_ellipsis_width = CalcFramedTextSize("…").x;
        const float min_item_width = framed_ellipsis_width;
        float total_width = ImGui::GetContentRegionAvail().x;
        float prev_total_width = total_width;

        float cur_x = 0;
        float last_line_right = 0;
        std::vector<FramedItemExt> line;
        std::size_t idx;
        bool need_less = false;

        for (idx = 0; idx < items.size(); ++idx) {
            auto& item = items[idx];
            auto& [text, tooltip, spec] = item;
            float width = CalcFramedTextSize(item).x;
            float right = cur_x + width;

            line.emplace_back(text,
                              tooltip,
                              (holds_alternative<std::monostate>(spec.id)
                               ? static_cast<int>(idx)
                               : spec.id),
                              width,
                              cur_x);

            cur_x += width + spacing;

            // If last item extends beyond total_width, remove it, unless it's the only item.
            if (right > total_width) {
                if (line.size() > 1) {
                    line.pop_back();
                    --idx;
                } else // if single item, just shrink it
                    line.back().width = std::fmax(min_item_width, total_width);

                // Show this line.
                last_line_right = line.back().x + line.back().width;
                show_one_framed_line(line);
                line.clear();
                cur_x = 0;
                prev_total_width = total_width;
                total_width = ImGui::GetContentRegionAvail().x;

                // If there are more lines after this, we need the "less" button.
                if (idx + 1 < items.size())
                    need_less = true;
            }
        }

        // We may have an incomplete line to show.
        if (!line.empty()) {
            last_line_right = line.back().x + line.back().width;
            show_one_framed_line(line);
            line.clear();
        }

        if (need_less) {
            // If we can fit the "less" button, put it on the same line.
            if (last_line_right + spacing + less_width < prev_total_width)
                ImGui::SameLine();
            bool result = ImGui::Button(less_label);
            ImGui::SetItemTooltip("Show less.");
            return result;
        } else
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


    ImVec2
    max(const ImVec2& a,
        const ImVec2& b)
    {
        return {
            std::fmax(a.x, b.x),
            std::fmax(a.y, b.y)
        };
    }


    float
    max_width(std::initializer_list<std::string> labels)
    {
        float result = 0;
        for (auto label : labels)
            result = std::fmax(result, ImGui::CalcTextSize(label).x);
        return result;
    }


    void
    SmoothScroll(float target_x,
                 float target_y)
    {
        SmoothScroll(ImVec2{target_x, target_y});
    }


    void
    SmoothScroll(const ImVec2& target,
                 float duration)
    {
        // TraceFunction tf{"UI"sv};

        ImVec2 start_pos = { ImGui::GetScrollX(), ImGui::GetScrollY() };
        ImVec2 final_pos = {
            target.x < 0 ? start_pos.x : target.x,
            target.y < 0 ? start_pos.y : target.y
        };
        double start_time = ImGui::GetTime();

        auto storage = ImGui::GetStateStorage();

        storage->SetFloat(ImGui::GetID(key_smooth_scroll_start_pos_x), start_pos.x);
        storage->SetFloat(ImGui::GetID(key_smooth_scroll_start_pos_y), start_pos.y);

        storage->SetFloat(ImGui::GetID(key_smooth_scroll_final_pos_x), final_pos.x);
        storage->SetFloat(ImGui::GetID(key_smooth_scroll_final_pos_y), final_pos.y);

        storage->SetFloat(ImGui::GetID(key_smooth_scroll_start_time), start_time);
        storage->SetFloat(ImGui::GetID(key_smooth_scroll_duration), duration);
    }


    void
    SmoothScrollItem(float duration)
    {
        // TraceFunction tf{"UI"sv};

        ImVec2 item_min = ScreenToLocal(ImGui::GetItemRectMin());
        ImVec2 item_max = ScreenToLocal(ImGui::GetItemRectMax());

        ImVec2 item_size = item_max - item_min;
        ImVec2 scroll = { ImGui::GetScrollX(), ImGui::GetScrollY() };

        ImVec2 win_padding = ImGui::GetCursorStartPos() + scroll;
        ImVec2 win_size = ImGui::GetWindowSize();

        const auto& style = ImGui::GetStyle();
        ImVec2 scrollbar_size = {
            ImGui::GetScrollMaxY() > 0 ? style.ScrollbarSize : 0.0f,
            ImGui::GetScrollMaxX() > 0 ? style.ScrollbarSize : 0.0f
        };
        ImVec2 visible_size = win_size - 2 *  win_padding - scrollbar_size;

        ImVec2 new_scroll = scroll;

        if (item_size.x > visible_size.x) {
            // Item is too large to fit.
            // Make sure there's no empty space on the left.
            if (item_min.x > scroll.x)
                new_scroll.x = item_min.x;

            // Make sure there's no empty space on the right.
            if (item_max.x < scroll.x + visible_size.x)
                new_scroll.x = item_max.x - visible_size.x;
        } else {
            // If left boundary is out of view.
            if (item_min.x < scroll.x)
                new_scroll.x = item_min.x;

            // If right boundary is out of view.
            if (item_max.x > scroll.x + visible_size.x)
                new_scroll.x = item_max.x - visible_size.x;
        }

        if (item_size.y > visible_size.y) {
            // Item is too large to fit.
            // Make sure there's no empty space above.
            if (item_min.y > scroll.y)
                new_scroll.y = item_min.y;

            // Make sure there's no empty space below.
            if (item_max.y < scroll.y + visible_size.y)
                new_scroll.y = item_max.y - visible_size.y;
        } else {
            // If top boundary is out of view
            if (item_min.y < scroll.y)
                new_scroll.y = item_min.y;

            // If bottom boundary is out of view.
            if (item_max.y > scroll.y + visible_size.y)
                new_scroll.y = item_max.y - visible_size.y;
        }

        if (new_scroll != scroll)
            SmoothScroll(new_scroll, duration);
    }


    void
    DoSmoothScroll()
    {
        // TraceFunction tf{"UI"sv};

        auto storage = ImGui::GetStateStorage();
        auto id_duration = ImGui::GetID(key_smooth_scroll_duration);

        if (std::ranges::binary_search(storage->Data,
                                       id_duration,
                                       {},
                                       &ImGuiStoragePair::key)) {

            auto id_start_pos_x = ImGui::GetID(key_smooth_scroll_start_pos_x);
            auto id_start_pos_y = ImGui::GetID(key_smooth_scroll_start_pos_y);
            auto id_final_pos_x = ImGui::GetID(key_smooth_scroll_final_pos_x);
            auto id_final_pos_y = ImGui::GetID(key_smooth_scroll_final_pos_y);
            auto id_start_time  = ImGui::GetID(key_smooth_scroll_start_time);

            ImVec2 start_pos;
            start_pos.x = storage->GetFloat(id_start_pos_x);
            start_pos.y = storage->GetFloat(id_start_pos_y);

            ImVec2 final_pos;
            final_pos.x = storage->GetFloat(id_final_pos_x);
            final_pos.y = storage->GetFloat(id_final_pos_y);

            double start_time = storage->GetFloat(id_start_time);
            float duration = storage->GetFloat(id_duration);

            double now = ImGui::GetTime();
            float dt = (now - start_time) / duration;

            auto pos = cubic_hermite_spline(start_pos,
                                            final_pos,
                                            ImVec2{0, 0},
                                            ImVec2{0, 0},
                                            dt);

            ImGui::SetScrollX(pos.x);
            ImGui::SetScrollY(pos.y);

            if (dt >= 1) {
                erase_from_storage(storage, id_start_pos_x);
                erase_from_storage(storage, id_start_pos_y);
                erase_from_storage(storage, id_final_pos_x);
                erase_from_storage(storage, id_final_pos_y);
                erase_from_storage(storage, id_start_time);
                erase_from_storage(storage, id_duration);
            }
        }
    }


    ImVec2
    ScreenToLocal(const ImVec2& screen_pos)
    {
        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 scroll = { ImGui::GetScrollX(), ImGui::GetScrollY() };
        return screen_pos - win_pos + scroll;
    }


    void
    Title(const std::string& text)
    {
        using namespace ImGui::RAII;

        Font bigger{nullptr, 0, 1.2f};
        ImGui::TextAligned(0.5f, -1, text);
        ImGui::Separator();
    }

} // namespace UI
