#pragma once

// This header provides macros for compatibility of certain features across
//   multiple compilers.

// Atomic

// C++ is only ever used when the headers are included.
// In this case, the atomic variables shouldn't be touched,
//   so it's not necessary to have the atomic instrumentation.
#if defined(__cplusplus)
#define atomic

// MSVC doesn't like modern C features in general.
// Currently, _Atomic is only used for 32-bit values or smaller, and as per
//   https://docs.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
//   reading and writing from these variables is already atomic.
// So, _Atomic will not be used with MSVC, at least for now.
#elif defined(_MSC_VER)
#define atomic

#else
#define atomic _Atomic
#endif

// Attribute packed
#if defined(__GNUC__)
#define A_PACKED(...) __attribute__((packed)) __VA_ARGS__
#elif defined(_MSC_VER)
#define A_PACKED(...) _Pragma(pack(push, 1)) __VA_ARGS__ _Pragma(pack(pop))
#endif

// Attribute weak
#ifdef __GNUC__
#define A_WEAK __attribute__((weak))
#endif

// Attribute unused
#ifdef __GNUC__
#define A_UNUSED __attribute__((unused))
#else
#define A_UNUSED
#endif
