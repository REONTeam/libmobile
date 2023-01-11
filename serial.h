// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MOBILE_INTERNAL
#include "mobile.h"
#include "atomic.h"
struct mobile_adapter;

#define MOBILE_MAX_DATA_SIZE 0xFF

enum mobile_serial_state {
    MOBILE_SERIAL_WAITING,
    MOBILE_SERIAL_DATA,
    MOBILE_SERIAL_CHECKSUM,
    MOBILE_SERIAL_ACKNOWLEDGE,
    MOBILE_SERIAL_IDLE_CHECK,
    MOBILE_SERIAL_RESPONSE_WAITING,
    MOBILE_SERIAL_RESPONSE_START,
    MOBILE_SERIAL_RESPONSE_DATA,
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

struct mobile_adapter_serial {
    _Atomic volatile enum mobile_serial_state state;
    _Atomic volatile bool active;
    enum mobile_serial_error error;

    bool mode_32bit;
    enum mobile_adapter_device device;
    bool device_unmetered;

    uint16_t checksum;
    unsigned current;
    unsigned char buffer[4 + MOBILE_MAX_DATA_SIZE + 2 + 3];  // header, content, checksum + alignment to 4 bytes
    unsigned data_size;
};

void mobile_serial_init(struct mobile_adapter *adapter);
unsigned char mobile_serial_transfer(struct mobile_adapter *adapter, unsigned char c);

#undef _Atomic  // "atomic.h"
