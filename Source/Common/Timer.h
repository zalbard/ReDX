#pragma once

#include <chrono>
#include "Definitions.h"

// High resolution timer compatible with std::chrono::high_resolution_clock interface
// Accesses the processor's time stamp counter (TSC) via the QueryPerformanceCounter API
class HighResTimer {
public:
    SINGLETON(HighResTimer);
    using rep        = long long;
    using period     = std::micro;
    using duration   = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<HighResTimer>;
    // Mark it as steady
    static constexpr bool is_steady = true;
    // Retrieves the current value of the TSC
    static time_point now();
    // Converts the current value of the TSC to milliseconds
    // Effectively returns the number of milliseconds since the last TSC reset
    static uint milliseconds();
    // Converts the current value of the TSC to milliseconds
    // Effectively returns the number of milliseconds since the last TSC reset
    static uint64 microseconds();
};
