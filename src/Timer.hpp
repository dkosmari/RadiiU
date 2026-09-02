#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
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


    void
    report();

}; // struct Timer


struct TimerReporter {

    Timer timer;

    TimerReporter(const std::string& name)
        noexcept:
        timer{name}
    {
        timer.start();
    }


    ~TimerReporter()
    {
        timer.stop();
        timer.report();
    }

}; // struct TimerReporter

#endif
