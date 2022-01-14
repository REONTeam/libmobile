// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Internal mobile adapter data structures
// Exposed to the user of this library for the purpose of allocating memory,
//   but shouldn't be accessed independently.

#define MOBILE_INTERNAL
#include "mobile.h"
#include "commands.h"
#include "serial.h"
#include "dns.h"

struct mobile_adapter_global {
    bool active;

    bool packet_parsed;
    struct mobile_packet packet;
};

struct mobile_adapter {
    void *user;
    struct mobile_adapter_global global;
    struct mobile_adapter_config config;
    struct mobile_adapter_serial serial;
    struct mobile_adapter_commands commands;
    struct mobile_adapter_dns dns;
};
