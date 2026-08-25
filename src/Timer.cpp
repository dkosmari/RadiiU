#include <ostream>

#include "Timer.hpp"


Timer::Timer(const std::string& name) :
    name{name}
{}


std::ostream&
Timer::print(std::ostream& out)
{
    out << "Timer";
    if (!name.empty())
        out << " \"" << name << "\"";
    out << ": " << duration_cast<std::chrono::milliseconds>(elapsed());
    return out;
}


TimerReporter::~TimerReporter()
{
    if (canceled)
        return;

    auto diff = timer.stop();
    if (diff > threshold)
        timer.print(out) << std::endl;
}


void
TimerReporter::cancel()
    noexcept
{
    canceled = true;
}
