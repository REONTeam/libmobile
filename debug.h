// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stdbool.h>
struct mobile_adapter;
struct mobile_packet;
struct mobile_addr;

#define MOBILE_DEBUG_BUFFER_SIZE 80

struct mobile_adapter_debug {
    unsigned current;
    char buffer[MOBILE_DEBUG_BUFFER_SIZE];
};

void mobile_debug_write(struct mobile_adapter *adapter, const char *data, unsigned size);
void mobile_debug_print(struct mobile_adapter *adapter, const char *fmt, ...);
void mobile_debug_print_hex(struct mobile_adapter *adapter, const void *data, unsigned size);
void mobile_debug_print_addr(struct mobile_adapter *adapter, const struct mobile_addr *addr);
void mobile_debug_endl(struct mobile_adapter *adapter);
void mobile_debug_init(struct mobile_adapter *adapter);
void mobile_debug_command(struct mobile_adapter *adapter, const struct mobile_packet *packet, bool send);
