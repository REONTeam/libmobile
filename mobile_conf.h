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
// This define makes the library define mobile_impl_* functions as weak
// symbols, allowing any library user to implement these directly, instead of
// passing the callbacks as pointers into the mobile_def_* functions.
//
// If this option is set, and weak symbols aren't detected as supported by the
// toolchain (see compat.h), all of the mobile_impl_* functions will need to
// be defined by the user, and the library will only work as a static library.
//
//#define MOBILE_LIBCONF_IMPL_WEAK
