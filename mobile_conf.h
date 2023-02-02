// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Configuration file defining various settings and toggles that change the
// behavior of the library at compile time.
//
// To change these settings, you're allowed to either define them as
// preprocessor defines, or including a modified version of this header in the
// relevant (buildtime?) paths.
//
// Make sure that both the library and any files that include the library
// header are built with exactly the same options.

// MOBILE_LIBCONF_IMPL_WEAK - use weak implementation callbacks
//
// Makes the library define mobile_impl_* functions as weak symbols, allowing
// any library user to implement these directly, instead of passing the
// callbacks as pointers into the mobile_def_* functions.
//
// If this option is set, and weak symbols aren't detected as supported by the
// toolchain (see compat.h), all of the mobile_impl_* functions will need to
// be defined by the user, and the library will only work as a static library.
//
//#define MOBILE_LIBCONF_IMPL_WEAK

// MOBILE_LIBCONF_NOALLOC - disable functions for memory allocation
//
// Removes any function that calls malloc(). This is primarily useful for
// platforms where including a memory allocator is unwieldy, requiring too much
// code memory or ram, and as such are better off managing memory themselves.
//
// The library may never be used as a shared library in this fashion, though
// it's very unlikely that any platform where this is a problem can't use
// shared libraries regardless.
//
//#define MOBILE_LIBCONF_NOALLOC
