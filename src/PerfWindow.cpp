#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <span>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "PerfWindow.hpp"

#include "App.hpp"
#include "LogManager.hpp"
#include "tracer.hpp"
#include "thread_safe.hpp"


using namespace std::literals;


namespace PerfWindow {

    namespace {

        /*-----------*/
        /* Constants */
        /*-----------*/

        constexpr std::size_t max_samples = 180;


        /*-------*/
        /* Types */
        /*-------*/

        struct TimeSeries {
            std::array<float, max_samples> data;
            std::size_t cur;

            TimeSeries();

            void
            add(float value);

            int
            get_first()
                const;

        }; // struct TimeSeries

        // using Collection = std::unordered_map<std::string, TimeSeries>;


        /*-----------*/
        /* Variables */
        /*-----------*/

        // thread_safe<Collection> safe_collection;
        TimeSeries frame_time_history;


        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/


        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        TimeSeries::TimeSeries()
        {
            data.fill(0);
            cur = 0;
        }


        void
        TimeSeries::add(float value)
        {
            data[cur++] = value;
            if (cur >= data.size())
                cur = 0;
        }


        int
        TimeSeries::get_first()
            const
        {
            return cur;
        }

    } // namespace


    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize()
    {
        TRACE_FUNC;
    }


    void
    finalize()
    {
        TRACE_FUNC;
    }


    void
    process_ui()
    {
        using namespace ImGui::RAII;

        auto& io = ImGui::GetIO();
        frame_time_history.add(1/io.DeltaTime);

        StyleColor transp_bg{ImGuiCol_WindowBg, {0.0f, 0.0f, 0.0f, 0.5f}};

        ImGui::SetNextWindowSize({680, 200}, ImGuiCond_Appearing);
        if (Window perf_window{"PerfWindow",
                               nullptr,
                               ImGuiWindowFlags_None}) {

            Font smaller{nullptr, 0, 0.75f};

            if (Child content{"content"}) {
                ImGui::PlotHistogram("FPS",
                                     frame_time_history.data.data(),
                                     frame_time_history.data.size(),
                                     frame_time_history.get_first(),
                                     nullptr,
                                     0,
                                     FLT_MAX,
                                     {max_samples * 3, 0});

            }

        } // perf_window
    }

} // namespace PerfWindow
