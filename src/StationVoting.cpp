/*
 * RadiiU - an internet radio player for the Wii U.
 *
 * Copyright (C) 2026  Daniel K. O. <dkosmari>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>


#include "StationVoting.hpp"

#include "humanize.hpp"
#include "IconsFontAwesome4.h"
#include "LogManager.hpp"
#include "RadioBrowserAPI.hpp"
#include "Settings.hpp"
#include "tracer.hpp"
#include "UI.hpp"


using namespace std::literals;

using Settings::cfg;


namespace StationVoting {

    namespace {

        using clock = std::chrono::system_clock;


        /*-------*/
        /* Types */
        /*-------*/

        struct VoteRecord {
            clock::time_point when;
            RadioBrowserAPI::VoteResult result;
        };


        /*-----------*/
        /* Constants */
        /*-----------*/

        const clock::duration vote_duration = 10min;


        /*-----------*/
        /* Variables */
        /*-----------*/

        std::unordered_map<std::string, VoteRecord> votes_cast;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        cast_vote(const ConstStationPtr& station);

        void
        handle_vote_error(const ConstStationPtr& station,
                          const std::exception& e);

        void
        handle_vote_result(const ConstStationPtr& station,
                           RadioBrowserAPI::VoteResult result);

        void
        handle_vote_update(const ConstStationPtr& station,
                           RadioBrowserAPI::Station rb_station);


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        cast_vote(const ConstStationPtr& station)
        {
            TRACE_FUNC;

            if (!station || station->stationuuid.empty())
                return;

            RadioBrowserAPI::send_vote(station->stationuuid,
                                       std::bind_front(handle_vote_result, station),
                                       std::bind_front(handle_vote_error, station));
        }


        void
        handle_vote_error(const ConstStationPtr& station,
                          const std::exception& e)
        {
            LOG_ERROR("Failed to cast vote for {:?}: {}",
                      station->name,
                      e.what());
            votes_cast[station->stationuuid] = {
                .when = clock::now(),
                .result{
                    .ok = false,
                    .message = e.what()
                }
            };
        }


        void
        handle_vote_result(const ConstStationPtr& station,
                           RadioBrowserAPI::VoteResult result)
        {
            LOG_DEBUG("Result of vote: {}", result.ok);

            if (!result.message.empty())
                LOG_DEBUG("{}", result.message);

            if (result.ok) {
                // Fetch new vote count
                RadioBrowserAPI::get_station(
                    station->stationuuid,
                    std::bind_front(handle_vote_update, station),
                    std::bind_front(handle_vote_error, station)
                );
            }

            votes_cast[station->stationuuid] = {
                .when = clock::now(),
                .result = std::move(result)
            };
        }


        void
        handle_vote_update(const ConstStationPtr& station,
                           RadioBrowserAPI::Station rb_station)
        {
            LOG_DEBUG("Updating station votes: {} -> {}",
                      station->votes.value_or(0),
                      rb_station.votes);
            station->votes = rb_station.votes;
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    process_logic()
    {
        std::vector<std::string> expired;

        auto now = clock::now();
        for (const auto& [uuid, record] : votes_cast)
            if (now - record.when > vote_duration)
                expired.push_back(uuid);

        for (const auto& uuid : expired)
            votes_cast.erase(uuid);
    }


    void
    Button(ConstStationPtr station)
    {
        using namespace ImGui::RAII;

        auto vote_record = votes_cast.find(station->stationuuid);
        const bool voted = vote_record != votes_cast.end();
        const bool ok = voted ? vote_record->second.result.ok : false;

        std::string vote_label;
        if (voted) {
            if (ok)
                vote_label = ICON_FA_THUMBS_UP;
            else
                vote_label = ICON_FA_FROWN_O;
        } else {
            vote_label = ICON_FA_THUMBS_O_UP;
        }

        std::string value_label;
        if (station->votes) {
            value_label = humanize::value(*station->votes);
            vote_label += " " + value_label;
        }

        {
            ImVec2 size {
                UI::play_button_size.x,
                ImGui::GetFrameHeight()
            };
            std::optional<Font> smaller_font;
            if (value_label.size() >= 3)
                smaller_font.emplace(nullptr, 24);
            Disabled if_cant_vote{
                station->stationuuid.empty() ||
                !cfg.send_clicks ||
                voted
            };
            if (ImGui::Button(vote_label, size))
                cast_vote(station);
        }
        if (voted)
            ImGui::FormatSetItemTooltip("{} votes.\n{}",
                                        value_label,
                                        vote_record->second.result.message);
        else
            ImGui::FormatSetItemTooltip("{} votes.\nClick to vote for this station.",
                                        value_label);

    }


} // namespace StationVoting
