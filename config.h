// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#define MOBILE_INTERNAL
#include "mobile.h"
#include "atomic.h"

// Signals Pokémon Crystal (jp) that the connection isn't metered,
//   removing the time limit in mobile battles.
// We have no idea of the effects of this in other games.
#define MOBILE_CONFIG_DEVICE_UNMETERED 0x80

struct mobile_adapter_config {
    // What device to emulate
    _Atomic unsigned char device;  // Read by serial thread

    // DNS servers to override the gameboy's chosen servers with.
    // Only overridden if their type isn't MOBILE_ADDRTYPE_NONE.
    struct mobile_addr dns1;
    struct mobile_addr dns2;

    // What port to use for direct TCP connections
    unsigned p2p_port;

    // If p2p_relay.type isn't MOBILE_ADDRTYPE_NONE, use this relay server
    //   for p2p communication, instead of direct TCP connections
    struct mobile_addr relay;
};

void mobile_config_init(struct mobile_adapter *adapter);

#undef _Atomic  // "atomic.h"
