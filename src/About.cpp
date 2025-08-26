#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <curl/curl.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <jansson.h>
#include <mpg123.h>

#include <SDL_version.h>

#include "About.hpp"

#include "IconManager.hpp"
#include "imgui_extras.hpp"
#include "utils.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


using std::cout;
using std::endl;


namespace About {

    namespace {

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

        std::vector<std::string>
        get_authors()
        {
            std::vector<std::string> result;

            try {
                std::ifstream input{ utils::get_content_path() / "AUTHORS" };
                std::string line;
                while (getline(input, line))
                    result.push_back(line);
            }
            catch (std::exception& e) {
                cout << "Error loading AUTHORS file: " << e.what() << endl;
            }

            return result;
        }

    } // namespace


    void
    process_ui()
    {
        static const ImVec4 label_color = {1.0, 1.0, 0.25, 1.0};

        if (ImGui::BeginChild("about")) {

            auto radiiu_icon_tex = IconManager::get("ui/radiiu-icon.png");
            ImGui::Image(*radiiu_icon_tex, sdl::vec2{128, 128});
            ImGui::SameLine();

            if (ImGui::BeginTable("app-details", 2)) {

                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, label_color);
                ImGui::TextAligned(1.0, -FLT_MIN, "Homepage");
                ImGui::PopStyleColor();
                ImGui::TableNextColumn();
                ImGui::TextLink(PACKAGE_URL);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleColor(ImGuiCol_Text, label_color);
                ImGui::TextAligned(1.0, -FLT_MIN, "Bugs");
                ImGui::PopStyleColor();
                ImGui::TableNextColumn();
                ImGui::TextLink(PACKAGE_BUGREPORT);

                ImGui::EndTable();
            }

            ImGui::Separator();

            ImGui::TextColored(label_color, "Components:");

            SDL_version sdl_ver;
            SDL_GetVersion(&sdl_ver);
            ImGui::BulletText("SDL: %u.%u.%u", sdl_ver.major, sdl_ver.minor, sdl_ver.patch);

            ImGui::BulletText("ImGui: %s", IMGUI_VERSION);

            ImGui::Bullet();
            ImGui::TextWrapped("CURL: %s", curl_version());

            ImGui::Bullet();
            ImGui::TextWrapped("User Agent: %s", utils::get_user_agent().data());

            ImGui::Bullet();
            ImGui::TextWrapped("JANSSON: %s", jansson_version_str());

            static const std::string mpg_decoders = get_mpg_decoders();
            ImGui::Bullet();
            ImGui::TextWrapped("MPG123 decoders: %s", mpg_decoders.data());

            ImGui::Separator();

            ImGui::TextColored(label_color, "Authors:");

            static const auto authors = get_authors();
            for (auto& author : authors) {
                ImGui::Bullet();
                ImGui::TextUnformatted(author);
            }

            // WORKAROUND: ImGui is cutting the end of child windows
            ImGui::Spacing();

        }

        ImGui::HandleDragScroll();
        ImGui::EndChild();
    }

} // namespace About
