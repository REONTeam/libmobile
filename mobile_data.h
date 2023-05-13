// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Internal mobile adapter data structures

// When the library is compiled alongside the code using it, including this
// header may provide for an easy method to statically reserve memory for the
// mobile_adapter structure. Every member of the structure is considered
// private to the library, and as such must not be modified by user code.

// Extra care must be taken to use -DMOBILE_LIBCONF_USE in the project
// including this file, whenever it's used to build the library. This flag is
// used to build the library when libmobile is compiled using any of its
// included build systems.

#include "mobile.h"
#include "global.h"
#include "callback.h"
#include "config.h"
#include "debug.h"
#include "serial.h"
#include "commands.h"
#include "dns.h"
#include "relay.h"

struct mobile_adapter {
    void *user;
    struct mobile_adapter_global global;
    struct mobile_adapter_callback callback;
    struct mobile_adapter_config config;
    struct mobile_adapter_debug debug;
    struct mobile_adapter_serial serial;
    struct mobile_adapter_commands commands;
    struct mobile_adapter_dns dns;
    struct mobile_adapter_relay relay;

    // Memory shared across subsystems
    struct {
        union {
            struct mobile_buffer_dns dns;
            struct mobile_buffer_relay relay;
        };
        union {
            struct mobile_buffer_serial serial;
            struct mobile_buffer_commands commands;
        };
    } buffer;
};
