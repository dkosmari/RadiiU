#ifndef TRACER_HPP
#define TRACER_HPP

#include <iostream>
#include <string>


struct Tracer {
    const std::string name;

    Tracer(const std::string& name) :
        name{name}
    {
        std::cout << "started: " << name << std::endl;
    }

    ~Tracer()
    {
        std::cout << "finished: " << name << std::endl;
    }
};

#define TRACE_MERGE(a, b) a##b

#define TRACE_FUNC Tracer TRACE_MERGE(tracer_, __COUNTER__){__PRETTY_FUNCTION__}

#define TRACE(x) Tracer TRACE_MERGE(tracer_, __COUNTER__){x}

#endif
