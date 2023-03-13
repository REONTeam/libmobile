// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "mobile.h"
#include "atomic.h"

#define MOBILE_MAX_DATA_SIZE 0xFF

enum mobile_serial_state {
    MOBILE_SERIAL_INIT,
    MOBILE_SERIAL_WAITING,
    MOBILE_SERIAL_HEADER,
    MOBILE_SERIAL_DATA,
    MOBILE_SERIAL_DATA_PAD,
    MOBILE_SERIAL_CHECKSUM,
    MOBILE_SERIAL_ACKNOWLEDGE,
    MOBILE_SERIAL_IDLE_CHECK,
    MOBILE_SERIAL_RESPONSE_WAITING,
    MOBILE_SERIAL_RESPONSE_INIT,
    MOBILE_SERIAL_RESPONSE_START,
    MOBILE_SERIAL_RESPONSE_HEADER,
    MOBILE_SERIAL_RESPONSE_DATA,
    MOBILE_SERIAL_RESPONSE_DATA_PAD,
    MOBILE_SERIAL_RESPONSE_CHECKSUM,
    MOBILE_SERIAL_RESPONSE_ACKNOWLEDGE
}
#if __GNUC__ && __AVR__
// Required for AVR _Atomic (it has no libatomic).
__attribute__((packed))
#endif
;

enum mobile_serial_error {
    MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND = 0xF0,
    MOBILE_SERIAL_ERROR_CHECKSUM,

    // Returned when:
    // - Transfer buffer is full and the transfer command is used
    // - Current command was canceled before sending a reply due to a serial
    //    timeout bigger than the command's timeout (>2s), but the device
    //    wasn't reset yet (>3s).
    MOBILE_SERIAL_ERROR_INTERNAL
};

struct mobile_buffer_serial {
    enum mobile_serial_error error;
    unsigned char current;
    unsigned char data_size;
    uint16_t checksum;
    unsigned char header[4];
    unsigned char footer[2];
};

struct mobile_adapter_serial {
    _Atomic volatile enum mobile_serial_state state;
    _Atomic volatile bool active;

    unsigned char buffer[MOBILE_MAX_DATA_SIZE];

    bool mode_32bit : 1;
    bool device_unmetered : 1;
    enum mobile_adapter_device device;
};

void mobile_serial_init(struct mobile_adapter *adapter);
unsigned char mobile_serial_transfer(struct mobile_adapter *adapter, unsigned char c);

#undef _Atomic  // "atomic.h"
