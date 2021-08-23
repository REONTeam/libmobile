// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// This header provides macros for compatibility of certain features across
//   multiple compilers.

// Attribute packed
#if defined(__GNUC__)
#define A_PACKED(...) __VA_ARGS__ __attribute__((packed))
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
