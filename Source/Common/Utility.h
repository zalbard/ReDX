#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "Definitions.h"

// For internal use only!
static inline void printInternal(FILE* stream, const char* prefix, const char* fmt,
                                 const va_list args) {
    // Print the time stamp.
    time_t rawTime;
    time(&rawTime);
    struct tm timeInfo;
    localtime_s(&timeInfo, &rawTime);
    fprintf(stream, "[%i:%i:%i] ", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    // Print the prefix if there is one.
    if (prefix) {
        fprintf(stream, "%s ", prefix);
    }
    // Print the arguments.
    vfprintf(stream, fmt, args);
}

// Prints information to stdout (printf syntax) and appends a newline at the end.
static inline void printInfo(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printInternal(stdout, nullptr, fmt, args);
    va_end(args);
    fputs("\n", stdout);
}

// Prints warnings to stdout (printf syntax) and appends a newline at the end.
static inline void printWarning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printInternal(stdout, "Warning:", fmt, args);
    va_end(args);
    fputs("\n", stdout);
}

// Prints fatal errors to stderr (printf syntax) and appends a newline at the end.
static inline void printError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printInternal(stderr, "Error:", fmt, args);
    va_end(args);
    fputs("\n", stderr);
}

// For internal use only!
[[noreturn]] static inline void panic(const char* file, const int line) {
    fprintf(stderr, "Error location: %s : %i\n", file, line);
    abort();
}

// Prints the location of the fatal error and terminates the program.
#define TERMINATE() panic(__FILE__, __LINE__)

// Prints the C string 'errMsg' if the HRESULT of 'call' signifies a failure.
#define CHECK_CALL(call, errMsg)          \
    do {                                  \
        volatile const HRESULT HR = call; \
        if (FAILED(HR)) {                 \
            printError(errMsg);           \
            DebugBreak();                 \
            panic(__FILE__, __LINE__);    \
        }                                 \
    } while (0)
