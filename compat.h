// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// This header provides macros for compatibility of certain features across
//   multiple compilers.

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

// AVR program space macros
// Since AVR has separate address spaces for program and data, it needs
//   different functions to access these spaces. Constant data is better stored
//   in the program address space, to avoid wasting precious RAM.
// Usually a no-op on platforms with a single linear address space.
#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#define PROGMEM
#define PSTR(...) __VA_ARGS__
#define pgm_read_ptr(x) (*x)
#define memcmp_P(...) memcmp(__VA_ARGS__)
#define memcpy_P(...) memcpy(__VA_ARGS__)
#define strlen_P(...) strlen(__VA_ARGS__)
#define vsnprintf_P(...) vsnprintf(__VA_ARGS__)
#endif
