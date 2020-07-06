#pragma once

// This header provides compatibility with runtimes and compilers that don't
//   readily support C11 atomics for whatever reason.

// C++ is only ever used when the headers are included.
// In this case, the atomic variables shouldn't be touched,
//   so it's not necessary to have the atomic instrumentation.
#ifdef __cplusplus
#define _Atomic
#endif

// MSVC doesn't like modern C features in general.
// Currently, _Atomic is only used for 32-bit values or smaller, and as per
//   https://docs.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
//   reading and writing from these variables is already atomic.
// So, _Atomic will not be used with MSVC, at least for now.
#ifdef MSVC_VER
#define _Atomic
#endif
