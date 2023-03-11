// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stdbool.h>

#include "mobile.h"
#include "atomic.h"

enum mobile_command {
    MOBILE_COMMAND_NULL = 0xF,
    MOBILE_COMMAND_START,
    MOBILE_COMMAND_END,
    MOBILE_COMMAND_TEL,
    MOBILE_COMMAND_OFFLINE,
    MOBILE_COMMAND_WAIT_CALL,
    MOBILE_COMMAND_DATA,
    MOBILE_COMMAND_REINIT,
    MOBILE_COMMAND_CHECK_STATUS,
    MOBILE_COMMAND_CHANGE_CLOCK,
    MOBILE_COMMAND_EEPROM_READ,
    MOBILE_COMMAND_EEPROM_WRITE,
    MOBILE_COMMAND_DATA_END = 0x1F,
    MOBILE_COMMAND_PPP_CONNECT = 0x21,
    MOBILE_COMMAND_PPP_DISCONNECT,
    MOBILE_COMMAND_TCP_CONNECT,
    MOBILE_COMMAND_TCP_DISCONNECT,
    MOBILE_COMMAND_UDP_CONNECT,
    MOBILE_COMMAND_UDP_DISCONNECT,
    MOBILE_COMMAND_DNS_REQUEST = 0x28,
    MOBILE_COMMAND_TEST_MODE = 0x3F,
    MOBILE_COMMAND_ERROR = 0x6E,
};

enum mobile_connection_state {
    MOBILE_CONNECTION_DISCONNECTED,
    MOBILE_CONNECTION_WAIT,
    MOBILE_CONNECTION_WAIT_RELAY,
    MOBILE_CONNECTION_WAIT_TIMEOUT,
    MOBILE_CONNECTION_CALL,
    MOBILE_CONNECTION_CALL_RECV,
    MOBILE_CONNECTION_CALL_ISP,
    MOBILE_CONNECTION_INTERNET,
};

struct mobile_adapter_commands {
    _Atomic volatile bool session_started;
    _Atomic volatile bool mode_32bit;

    // Asynchronous state for command processing
    unsigned char processing;  // Set to 0 every time a command is parsed
    unsigned char processing_data[4];
    struct mobile_addr processing_addr;

    enum mobile_connection_state state;
    bool connections[MOBILE_MAX_CONNECTIONS];
    bool dns2_use;
    unsigned char call_packets_sent;
    struct mobile_addr4 dns1;
    struct mobile_addr4 dns2;
};

void mobile_commands_init(struct mobile_adapter *adapter);
void mobile_commands_reset(struct mobile_adapter *adapter);
struct mobile_packet *mobile_commands_process(struct mobile_adapter *adapter, struct mobile_packet *packet);
bool mobile_commands_exists(enum mobile_command command);

#undef _Atomic  // "atomic.h"
