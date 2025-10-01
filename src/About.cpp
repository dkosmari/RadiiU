#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <curl/curl.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <jansson.h>
#include <mpg123.h>

#include <SDL_version.h>

#include "About.hpp"

#include "constants.hpp"
#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "ui.hpp"
#include "utils.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;


using constants::label_color;


namespace About {

    namespace {

        std::string
        get_sdl_version_str()
        {
                SDL_version sdl_ver;
                SDL_GetVersion(&sdl_ver);
                char buf[32];
                std::snprintf(buf, sizeof buf,
                              "%u.%u.%u",
                              sdl_ver.major,
                              sdl_ver.minor,
                              sdl_ver.patch);
                return buf;
        }


        std::string
        get_mpg_decoders()
        {
            std::string result;
            const char** mpg_dec = mpg123_decoders();
            for (std::size_t i = 0; mpg_dec[i]; ++i) {
                if (i > 0)
                    result += ", ";
                result += mpg_dec[i];
            }
            return result;
        }


        struct RoleName {
            std::string role;
            std::string name;
        };

        std::vector<RoleName>
        get_credits()
        {
            std::vector<RoleName> result;

            try {
                std::ifstream input{ utils::get_content_path() / "CREDITS" };
                std::string line;
                while (getline(input, line)) {
                    if (!line.empty() && line.front() == '#')
                        continue;
                    line = utils::trimmed(line);
                    if (line.empty())
                        continue;
                    auto tokens = utils::split(line, ":", false, 2);
                    if (tokens.size() != 2) {
                        cout << "Error in CREDITS file: split into "
                             << tokens.size()
                             << " tokens: \""
                             << line << "\""
                             << endl;
                        continue;
                    }
                    result.emplace_back(std::move(tokens[0]), std::move(tokens[1]));
                }
            }
            catch (std::exception& e) {
                cout << "Error loading CREDITS file: " << e.what() << endl;
            }

            return result;
        }

    } // namespace


    void
    process_ui()
    {
        // Note: flat navigation doesn't work well on child windows that scroll.
        if (ImGui::BeginChild("about")) {

            auto radiiu_icon_tex = IconManager::get("ui/radiiu-icon.png");
            ImGui::Image(*radiiu_icon_tex, sdl::vec2{128, 128});
            ImGui::SameLine();

            if (ImGui::BeginTable("app-details", 2)) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                ui::show_link_row("Homepage", PACKAGE_URL);
                ui::show_link_row("Bugs", PACKAGE_BUGREPORT);
                ui::show_info_row("User Agent", utils::get_user_agent());

                ImGui::EndTable();
            }

            // ImGui::SeparatorTextColored(label_color, "Credits");
            static const auto credits = get_credits();
            ImGui::SeparatorText("Credits");
            if (ImGui::BeginTable("credits", 2)) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                for (const auto& [role, name] : credits)
                    ui::show_info_row(role, name);

                ImGui::EndTable();
            }

            // ImGui::SeparatorTextColored(label_color, "Components");
            ImGui::SeparatorText("Components");
            if (ImGui::BeginTable("components", 2)) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                static const std::string sdl_version = get_sdl_version_str();
                ui::show_info_row("SDL", sdl_version);

                ui::show_info_row("ImGui", IMGUI_VERSION);

                ui::show_info_row("libCURL", curl_version());

                ui::show_info_row("JANSSON", jansson_version_str());

                static const std::string mpg_decoders = get_mpg_decoders();
                ui::show_info_row("mpg123 decoders", mpg_decoders);

                ImGui::EndTable();
            }
        }

        ImGui::HandleDragScroll();
        ImGui::EndChild();
    }

} // namespace About
