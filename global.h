// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Exposes functions and data defined/used in mobile.c

#include <stdbool.h>

#include "commands.h"

#define MOBILE_MAX_DATA_SIZE 0xFF

struct mobile_packet {
    enum mobile_command command;
    unsigned char length;
    unsigned char data[MOBILE_MAX_DATA_SIZE];
};

struct mobile_adapter_global {
    // Whether the adapter is turned on or not
    bool start: 1;

    // Whether the adapter is currently awake or not
    // Used to reset everything after a bit of inactivity
    bool active: 1;

    // Whether the <packet> field has been initialized
    bool packet_parsed: 1;

    // Whether the relay connection is currently open
    bool number_fetch_active: 1;

    // Remaining retries for initializing the relay number
    unsigned char number_fetch_retries;

    // Packet data to pass between "serial" and "commands"
    struct mobile_packet packet;
};

void mobile_number_fetch_cancel(struct mobile_adapter *adapter);
void mobile_number_fetch_reset(struct mobile_adapter *adapter);
