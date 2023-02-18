// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stdbool.h>

struct mobile_adapter;
struct mobile_addr;

// TODO: Share buffer with dns.c to save memory?

#define MOBILE_RELAY_PACKET_SIZE 0x20

enum mobile_relay_command {
    MOBILE_RELAY_COMMAND_CALL,
    MOBILE_RELAY_COMMAND_WAIT,
    MOBILE_RELAY_COMMAND_GET_NUMBER
};

enum mobile_relay_state {
    // Connection states
    MOBILE_RELAY_DISCONNECTED,
    MOBILE_RELAY_CONNECTED,
    MOBILE_RELAY_LINKED,

    // Waiting for command states
    MOBILE_RELAY_RECV_CONNECT,
    MOBILE_RELAY_RECV_HANDSHAKE,
    MOBILE_RELAY_RECV_CALL,
    MOBILE_RELAY_RECV_WAIT,
    MOBILE_RELAY_RECV_GET_NUMBER
};

enum mobile_relay_call_result {
    MOBILE_RELAY_CALL_RESULT_FAILURE = -1,
    MOBILE_RELAY_CALL_RESULT_PROCESSING = 0,
    MOBILE_RELAY_CALL_RESULT_ACCEPTED,  // Call established
    MOBILE_RELAY_CALL_RESULT_INTERNAL,  // Internal error
    MOBILE_RELAY_CALL_RESULT_BUSY,  // Number is busy
    MOBILE_RELAY_CALL_RESULT_UNAVAILABLE,  // Number not available
    MOBILE_RELAY_MAX_CALL_RESULT
};

enum mobile_relay_wait_result {
    MOBILE_RELAY_WAIT_RESULT_FAILURE = -1,
    MOBILE_RELAY_WAIT_RESULT_PROCESSING = 0,
    MOBILE_RELAY_WAIT_RESULT_ACCEPTED,  // Call established
    MOBILE_RELAY_WAIT_RESULT_INTERNAL,  // Internal error
    MOBILE_RELAY_MAX_WAIT_RESULT
};

struct mobile_adapter_relay {
    enum mobile_relay_state state;
    unsigned char processing;

    unsigned buffer_len;
    unsigned char buffer[MOBILE_RELAY_PACKET_SIZE];
};

void mobile_relay_init(struct mobile_adapter *adapter);
int mobile_relay_connect(struct mobile_adapter *adapter, unsigned char conn, const struct mobile_addr *server);
int mobile_relay_call(struct mobile_adapter *adapter, unsigned char conn, const char *number, unsigned number_len);
int mobile_relay_wait(struct mobile_adapter *adapter, unsigned char conn, char *number, unsigned *number_len);
int mobile_relay_get_number(struct mobile_adapter *adapter, unsigned char conn, char *number, unsigned *number_len);
int mobile_relay_proc_call(struct mobile_adapter *adapter, unsigned char conn, const struct mobile_addr *server, const char *number, unsigned number_len);
int mobile_relay_proc_wait(struct mobile_adapter *adapter, unsigned char conn, const struct mobile_addr *server);
