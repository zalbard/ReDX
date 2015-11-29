#pragma once

// Declares a -default- Dtor, copy/move Ctors and assignment Ops
#define RULE_OF_ZERO(T) T(const T&)                = default; \
                        T& operator=(const T&)     = default; \
                        T(T&&) noexcept            = default; \
                        T& operator=(T&&) noexcept = default; \
                        ~T() noexcept              = default

// Declares an -explicit- Dtor, copy/move Ctors and assignment Otors
#define RULE_OF_FIVE(T) T(const T&);                \
                        T& operator=(const T&);     \
                        T(T&&) noexcept;            \
                        T& operator=(T&&) noexcept; \
                        ~T() noexcept

// Declares a -default- Dtor, a move Ctor and an assignment Otor
// Forbids copy construction and copy assignment
#define RULE_OF_ZERO_MOVE_ONLY(T) T(const T&)                = delete;  \
                                  T& operator=(const T&)     = delete;  \
                                  T(T&&) noexcept            = default; \
                                  T& operator=(T&&) noexcept = default; \
                                  ~T() noexcept              = default

// Declares an -explicit- Dtor, a move Ctor and an assignment Otor
// Forbids copy construction and copy assignment
#define RULE_OF_FIVE_MOVE_ONLY(T) T(const T&)            = delete; \
                                  T& operator=(const T&) = delete; \
                                  T(T&&) noexcept;                 \
                                  T& operator=(T&&) noexcept;      \
                                  ~T() noexcept

// Declares a singleton class which cannot be
// constructed, destroyed, copied, moved or assigned
#define SINGLETON(T) T()                        = delete; \
                     T(const T&)                = delete; \
                     T& operator=(const T&)     = delete; \
                     T(T&&) noexcept            = delete; \
                     T& operator=(T&&) noexcept = delete; \
                     ~T() noexcept              = delete

using uint   = unsigned int;
using uint8  = unsigned short;
using uint64 = unsigned long long;
