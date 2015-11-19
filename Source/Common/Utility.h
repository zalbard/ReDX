#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <time.h>

// Computes the square of the value
template <typename T>
static inline T sq(const T v) {
    return v * v;
}

// Computes the inverse square of the value
template <typename T>
static inline float invSq(const T v) {
    return 1.0f / (v * v);
}

// For internal use only!
static inline void printInternal(FILE* const stream, const char* const fmt, const va_list& args) {
    // Print the timestamp
    time_t rawTime;
    time(&rawTime);
    struct tm timeInfo;
    localtime_s(&timeInfo, &rawTime);
    fprintf(stdout, "[%i:%i:%i] ", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    // Print the arguments
    vfprintf(stream, fmt, args);
}

// Prints information to stdout (printf syntax)
static inline void printInfo(const char* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printInternal(stdout, fmt, args);
    va_end(args);
    fputs("\n", stdout);
}

// Prints warnings/errors to stderr (printf syntax)
static inline void printError(const char* const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printInternal(stderr, fmt, args);
    va_end(args);
    fputs("\n", stderr);
}

// For internal use only!
[[noreturn]] static inline void panic(const char* const file, const int line) {
    fprintf(stderr, "Error location: %s : %i\n", file, line);
    abort();
}

// Prints the location of the fatal error and terminates the program
#define TERMINATE() panic(__FILE__, __LINE__)

// Prints the C string (errMsg) if the HRESULT (hr) fails
#define CHECK_CALL(hr, errMsg)     \
    if (FAILED(hr)) {              \
        printError(errMsg);        \
        panic(__FILE__, __LINE__); \
    }
