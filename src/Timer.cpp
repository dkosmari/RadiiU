#include "Timer.hpp"

#include "PerfWindow.hpp"


using flt_seconds = std::chrono::duration<float>;


Timer::Timer(const std::string& name) :
    name{name}
{}


void
Timer::report()
{
    float dt = duration_cast<flt_seconds>(elapsed()).count();
    PerfWindow::record_time(name, dt);
}
