// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define MOBILE_INTERNAL
#include "mobile.h"
#include "mobile_conf.h"

#ifndef MOBILE_LIBCONF_WEAK_IMPL
struct mobile_adapter_callback {
    mobile_func_debug_log debug_log;
    mobile_func_serial_disable serial_disable;
    mobile_func_serial_enable serial_enable;
    mobile_func_config_read config_read;
    mobile_func_config_write config_write;
    mobile_func_time_latch time_latch;
    mobile_func_time_check_ms time_check_ms;
    mobile_func_sock_open sock_open;
    mobile_func_sock_close sock_close;
    mobile_func_sock_connect sock_connect;
    mobile_func_sock_listen sock_listen;
    mobile_func_sock_accept sock_accept;
    mobile_func_sock_send sock_send;
    mobile_func_sock_recv sock_recv;
    mobile_func_update_number update_number;
};
#endif
void mobile_callback_init(struct mobile_adapter *adapter);

void mobile_cb_debug_log(struct mobile_adapter *adapter, const char *line);
void mobile_cb_serial_disable(struct mobile_adapter *adapter);
void mobile_cb_serial_enable(struct mobile_adapter *adapter);
bool mobile_cb_config_read(struct mobile_adapter *adapter, void *dest, uintptr_t offset, size_t size);
bool mobile_cb_config_write(struct mobile_adapter *adapter, const void *src, uintptr_t offset, size_t size);
void mobile_cb_time_latch(struct mobile_adapter *adapter, enum mobile_timers timer);
bool mobile_cb_time_check_ms(struct mobile_adapter *adapter, enum mobile_timers timer, unsigned ms);
bool mobile_cb_sock_open(struct mobile_adapter *adapter, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport);
void mobile_cb_sock_close(struct mobile_adapter *adapter, unsigned conn);
int mobile_cb_sock_connect(struct mobile_adapter *adapter, unsigned conn, const struct mobile_addr *addr);
bool mobile_cb_sock_listen(struct mobile_adapter *adapter, unsigned conn);
bool mobile_cb_sock_accept(struct mobile_adapter *adapter, unsigned conn);
int mobile_cb_sock_send(struct mobile_adapter *adapter, unsigned conn, const void *data, unsigned size, const struct mobile_addr *addr);
int mobile_cb_sock_recv(struct mobile_adapter *adapter, unsigned conn, void *data, unsigned size, struct mobile_addr *addr);
void mobile_cb_update_number(struct mobile_adapter *adapter, enum mobile_number type, const char *number);
