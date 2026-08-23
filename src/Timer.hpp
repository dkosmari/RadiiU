#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <iosfwd>
#include <string>

struct Timer {

    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;
    using duration = clock_type::duration;


    std::string name = {};
    time_point start_time = {};
    time_point finish_time = {};


    constexpr
    Timer()
        noexcept = default;

    Timer(const std::string& name);

    inline
    void
    start()
        noexcept
    {
        start_time = clock_type::now();
    }

    inline
    duration
    stop()
        noexcept
    {
        finish_time = clock_type::now();
        return elapsed();
    }

    [[nodiscard]]
    inline
    duration
    elapsed()
        const noexcept
    {
        return finish_time - start_time;
    }

    std::ostream&
    print(std::ostream& out);

}; // struct Timer


struct TimerReporter {

    std::ostream& out;
    Timer timer;
    Timer::duration threshold;


    TimerReporter(std::ostream& out,
                  Timer::duration threshold = Timer::duration{0})
        noexcept :
        out(out),
        threshold{threshold}
    {
        timer.start();
    }


    TimerReporter(std::ostream& out,
                  const std::string& name,
                  Timer::duration threshold = Timer::duration{0})
        noexcept:
        out(out),
        timer{name},
        threshold{threshold}
    {
        timer.start();
    }


    ~TimerReporter();

}; // struct TimerReporter

#endif
