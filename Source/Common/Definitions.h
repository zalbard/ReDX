#pragma once

// Fixed width integer types for storage.
#include <cstdint>

// Fast integer types for compute.
using byte_t = unsigned char;
using size_t = size_t;
using sign_t = ptrdiff_t;

// Declares -default- Dtor, copy/move Ctors and assignment Otors.
#define RULE_OF_ZERO(T) T(const T&)                = default; \
                        T& operator=(const T&)     = default; \
                        T(T&&) noexcept            = default; \
                        T& operator=(T&&) noexcept = default; \
                        ~T() noexcept              = default

// Declares -explicit- Dtor, copy/move Ctors and assignment Otors.
#define RULE_OF_FIVE(T) T(const T&);                \
                        T& operator=(const T&);     \
                        T(T&&) noexcept;            \
                        T& operator=(T&&) noexcept; \
                        ~T() noexcept

// Declares -default- Dtor, a move Ctor and an assignment Otor.
// Forbids copy construction and copy assignment.
#define RULE_OF_ZERO_MOVE_ONLY(T) T(const T&)                = delete;  \
                                  T& operator=(const T&)     = delete;  \
                                  T(T&&) noexcept            = default; \
                                  T& operator=(T&&) noexcept = default; \
                                  ~T() noexcept              = default

// Declares -explicit- Dtor, a move Ctor and an assignment Otor.
// Forbids copy construction and copy assignment.
#define RULE_OF_FIVE_MOVE_ONLY(T) T(const T&)            = delete; \
                                  T& operator=(const T&) = delete; \
                                  T(T&&) noexcept;                 \
                                  T& operator=(T&&) noexcept;      \
                                  ~T() noexcept

// Declares a static class which cannot be
// constructed, destroyed, copied, moved or assigned.
#define STATIC_CLASS(T) T()                        = delete; \
                        T(const T&)                = delete; \
                        T& operator=(const T&)     = delete; \
                        T(T&&) noexcept            = delete; \
                        T& operator=(T&&) noexcept = delete; \
                        ~T() noexcept              = delete

// Clean up Windows header includes.
// These are already defined for the entire project!
// #define NOMINMAX
// #define STRICT
// #define WIN32_LEAN_AND_MEAN
