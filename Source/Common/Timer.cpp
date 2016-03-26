#include <Windows.h>
#include "Timer.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

HighResTimer::time_point HighResTimer::now() {
    LARGE_INTEGER n_ticks, ticks_per_second;
    QueryPerformanceCounter(&n_ticks);
    QueryPerformanceFrequency(&ticks_per_second);
    // Avoid the loss of precision.
    n_ticks.QuadPart *= static_cast<rep>(period::den);
    n_ticks.QuadPart /= ticks_per_second.QuadPart;
    // Return the number of microseconds.
    return time_point(duration(n_ticks.QuadPart));
}

uint HighResTimer::milliseconds() {
    const auto time_now = now().time_since_epoch();
    const auto ms_count = std::chrono::duration_cast<std::chrono::milliseconds>(time_now).count();
    return static_cast<uint>(ms_count);
}

uint64 HighResTimer::microseconds() {
    return now().time_since_epoch().count();
}
