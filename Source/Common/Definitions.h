#pragma once

// Declares -default- Dtor, copy/move Ctors and assignment Ops
#define RULE_OF_ZERO(T) T(const T&)                = default; \
                        T& operator=(const T&)     = default; \
                        T(T&&) noexcept            = default; \
                        T& operator=(T&&) noexcept = default; \
                        ~T()                       = default

// Declares -explicit- Dtor, copy/move Ctors and assignment Ops
#define RULE_OF_FIVE(T) T(const T&);                \
                        T& operator=(const T&);     \
                        T(T&&) noexcept;            \
                        T& operator=(T&&) noexcept; \
                        ~T()

// Declares -default- Dtor, move Ctor and assignment Op
// Forbids copy construction and copy assignment
#define RULE_OF_ZERO_MOVE_ONLY(T) T(const T&)                = delete;  \
                                  T& operator=(const T&)     = delete;  \
                                  T(T&&) noexcept            = default; \
                                  T& operator=(T&&) noexcept = default; \
                                  ~T()                       = default

// Declares -explicit- Dtor, move Ctor and assignment Op
// Forbids copy construction and copy assignment
#define RULE_OF_FIVE_MOVE_ONLY(T) T(const T&)            = delete; \
                                  T& operator=(const T&) = delete; \
                                  T(T&&) noexcept;                 \
                                  T& operator=(T&&) noexcept;      \
                                  ~T()
