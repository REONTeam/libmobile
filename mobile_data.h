// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Internal mobile adapter data structures

// When the library is compiled alongside the code using it, including this
// header may provide for an easy method to statically reserve memory for the
// mobile_adapter structure. Every member of the structure is considered
// private to the library.

#include "mobile.h"
#include "callback.h"
#include "config.h"
#include "debug.h"
#include "commands.h"
#include "serial.h"
#include "dns.h"
#include "relay.h"

struct mobile_adapter_global {
    // Whether the adapter is turned on or not
    bool start: 1;

    // Whether the adapter is currently awake or not
    // Used to reset everything after a bit of inactivity
    bool active: 1;

    // Packet data to pass between "serial" and "commands"
    bool packet_parsed: 1;
    struct mobile_packet packet;
};

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
};
