#pragma once

#include <stdint.h>

#include "commands.h"
struct mobile_adapter;

#ifdef __cplusplus
#define _Atomic
#endif

enum mobile_serial_state {
    MOBILE_SERIAL_WAITING,
    MOBILE_SERIAL_DATA,
    MOBILE_SERIAL_CHECKSUM,
    MOBILE_SERIAL_ACKNOWLEDGE,
    MOBILE_SERIAL_RESPONSE_WAITING,
    MOBILE_SERIAL_RESPONSE_START,
    MOBILE_SERIAL_RESPONSE_DATA,
    MOBILE_SERIAL_RESPONSE_ACKNOWLEDGE
#if __GNUC__ && __AVR__
// Required for AVR _Atomic (it has no libatomic).
} __attribute__((packed));
#else
};
#endif

enum mobile_serial_error {
    MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND = 0xF0,
    MOBILE_SERIAL_ERROR_CHECKSUM,
};

struct mobile_adapter_serial {
    _Atomic enum mobile_serial_state state;
    unsigned current;
    unsigned char buffer[4 + MOBILE_MAX_DATA_SIZE + 2];  // header, content, checksum
    unsigned data_size;
    uint16_t checksum;
    enum mobile_serial_error error;
    unsigned retries;
    enum mobile_command last_command;
};

void mobile_serial_reset(struct mobile_adapter *adapter);
unsigned char mobile_transfer(struct mobile_adapter *adapter, unsigned char c);
