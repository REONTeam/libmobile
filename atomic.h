// SPDX-License-Identifier: LGPL-3.0-or-later

// This header helps with using C11 atomic in cases where it isn't (properly)
//   supported. This header is separate from compat.h, to avoid clobbering the
//   namespace of library users.

// C++ is only ever used when the headers are included.
// In this case, the atomic variables shouldn't be touched,
//   so it's not necessary to have the atomic instrumentation.
#if defined(__cplusplus)
#define _Atomic

// MSVC doesn't like modern C features in general.
// Currently, _Atomic is only used for 32-bit values or smaller, and as per
//   https://docs.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
//   reading and writing from these variables is already atomic.
// So, _Atomic will not be used with MSVC, at least for now.
#elif defined(_MSC_VER)
#define _Atomic

#endif

// When including this header, remember to unset _Atomic afterwards:
//#undef _Atomic
