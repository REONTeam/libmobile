// SPDX-License-Identifier: LGPL-3.0-or-later
#include "callback.h"

#include <string.h>

#include "data.h"
#include "compat.h"

// Optional implementations of the callback functions.
// Library internals will always call `mobile_impl_` functions, which will be
//   dispatched using pointers or similar depending on the Library
//   configuration.

#ifdef MOBILE_LIBCONF_IMPL_WEAK
#ifdef A_WEAK
#define IMPL A_WEAK
#endif  // IMPL undefined if weak symbols aren't supported
#else
#define IMPL
#endif

// Default implementations of the various callback functions
#ifdef IMPL
IMPL void mobile_impl_debug_log(A_UNUSED void *user, A_UNUSED const char *line)
{
    return;
}

IMPL void mobile_impl_serial_disable(A_UNUSED void *user)
{
    return;
}

IMPL void mobile_impl_serial_enable(A_UNUSED void *user)
{
    return;
}

IMPL bool mobile_impl_config_read(A_UNUSED void *user, void *dest, A_UNUSED uintptr_t offset, size_t size)
{
    memset(dest, 0xFF, size);
    return true;
}

IMPL bool mobile_impl_config_write(A_UNUSED void *user, A_UNUSED const void *src, A_UNUSED uintptr_t offset, A_UNUSED size_t size)
{
    return true;
}

IMPL void mobile_impl_time_latch(A_UNUSED void *user, A_UNUSED enum mobile_timers timer)
{
    return;
}

IMPL bool mobile_impl_time_check_ms(A_UNUSED void *user, A_UNUSED enum mobile_timers timer, A_UNUSED unsigned ms)
{
    return false;
}

IMPL bool mobile_impl_sock_open(A_UNUSED void *user, A_UNUSED unsigned conn, A_UNUSED enum mobile_socktype type, A_UNUSED enum mobile_addrtype addrtype, A_UNUSED unsigned bindport)
{
    return true;
}

IMPL void mobile_impl_sock_close(A_UNUSED void *user, A_UNUSED unsigned conn)
{
    return;
}

IMPL int mobile_impl_sock_connect(A_UNUSED void *user, A_UNUSED unsigned conn, A_UNUSED const struct mobile_addr *addr)
{
    return 1;
}

IMPL bool mobile_impl_sock_listen(A_UNUSED void *user, A_UNUSED unsigned conn)
{
    return true;
}

IMPL bool mobile_impl_sock_accept(A_UNUSED void *user, A_UNUSED unsigned conn) 
{
    return true;
}

IMPL int mobile_impl_sock_send(A_UNUSED void *user, A_UNUSED unsigned conn, A_UNUSED const void *data, A_UNUSED unsigned size, A_UNUSED const struct mobile_addr *addr)
{
    return size;
}

IMPL int mobile_impl_sock_recv(A_UNUSED void *user, A_UNUSED unsigned conn, A_UNUSED void *data, A_UNUSED unsigned size, A_UNUSED struct mobile_addr *addr)
{
    return -10;
}

IMPL void mobile_impl_update_number(A_UNUSED void *user, A_UNUSED enum mobile_number type, A_UNUSED const char *number)
{
    return;
}
#endif

void mobile_callback_init(struct mobile_adapter *adapter)
{
    (void)adapter;
#ifndef MOBILE_LIBCONF_IMPL_WEAK
    adapter->callback.debug_log = mobile_impl_debug_log;
    adapter->callback.serial_disable = mobile_impl_serial_disable;
    adapter->callback.serial_enable = mobile_impl_serial_enable;
    adapter->callback.config_read = mobile_impl_config_read;
    adapter->callback.config_write = mobile_impl_config_write;
    adapter->callback.time_latch = mobile_impl_time_latch;
    adapter->callback.time_check_ms = mobile_impl_time_check_ms;
    adapter->callback.sock_open = mobile_impl_sock_open;
    adapter->callback.sock_close = mobile_impl_sock_close;
    adapter->callback.sock_connect = mobile_impl_sock_connect;
    adapter->callback.sock_listen = mobile_impl_sock_listen;
    adapter->callback.sock_accept = mobile_impl_sock_accept;
    adapter->callback.sock_recv = mobile_impl_sock_recv;
    adapter->callback.update_number = mobile_impl_update_number;
#endif
}

void mobile_cb_debug_log(struct mobile_adapter *adapter, const char *line)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    mobile_impl_debug_log(adapter->user, line);
#else
    adapter->callback.debug_log(adapter->user, line);
#endif
}

void mobile_cb_serial_disable(struct mobile_adapter *adapter)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    mobile_impl_serial_disable(adapter->user);
#else
    adapter->callback.serial_disable(adapter->user);
#endif
}

void mobile_cb_serial_enable(struct mobile_adapter *adapter)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    mobile_impl_serial_enable(adapter->user);
#else
    adapter->callback.serial_enable(adapter->user);
#endif
}

bool mobile_cb_config_read(struct mobile_adapter *adapter, void *dest, uintptr_t offset, size_t size)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_config_read(adapter->user, dest, offset, size);
#else
    return adapter->callback.config_read(adapter->user, dest, offset, size);
#endif
}

bool mobile_cb_config_write(struct mobile_adapter *adapter, const void *src, uintptr_t offset, size_t size)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_config_write(adapter->user, src, offset, size);
#else
    return adapter->callback.config_write(adapter->user, src, offset, size);
#endif
}

void mobile_cb_time_latch(struct mobile_adapter *adapter, enum mobile_timers timer)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    mobile_impl_time_latch(adapter->user, timer);
#else
    adapter->callback.time_latch(adapter->user, timer);
#endif
}

bool mobile_cb_time_check_ms(struct mobile_adapter *adapter, enum mobile_timers timer, unsigned ms)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_time_check_ms(adapter->user, timer, ms);
#else
    return adapter->callback.time_check_ms(adapter->user, timer, ms);
#endif
}

bool mobile_cb_sock_open(struct mobile_adapter *adapter, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_sock_open(adapter->user, conn, type, addrtype, bindport);
#else
    return adapter->callback.sock_open(adapter->user, conn, type, addrtype, bindport);
#endif
}

void mobile_cb_sock_close(struct mobile_adapter *adapter, unsigned conn)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    mobile_impl_sock_close(adapter->user, conn);
#else
    adapter->callback.sock_close(adapter->user, conn);
#endif
}

int mobile_cb_sock_connect(struct mobile_adapter *adapter, unsigned conn, const struct mobile_addr *addr)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_sock_connect(adapter->user, conn, addr);
#else
    return adapter->callback.sock_connect(adapter->user, conn, addr);
#endif
}

bool mobile_cb_sock_listen(struct mobile_adapter *adapter, unsigned conn)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_sock_listen(adapter->user, conn);
#else
    return adapter->callback.sock_listen(adapter->user, conn);
#endif
}

bool mobile_cb_sock_accept(struct mobile_adapter *adapter, unsigned conn)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_sock_accept(adapter->user, conn);
#else
    return adapter->callback.sock_accept(adapter->user, conn);
#endif
}

int mobile_cb_sock_send(struct mobile_adapter *adapter, unsigned conn, const void *data, unsigned size, const struct mobile_addr *addr)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_sock_send(adapter->user, conn, data, size, addr);
#else
    return adapter->callback.sock_send(adapter->user, conn, data, size, addr);
#endif
}

int mobile_cb_sock_recv(struct mobile_adapter *adapter, unsigned conn, void *data, unsigned size, struct mobile_addr *addr)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    return mobile_impl_sock_recv(adapter->user, conn, data, size, addr);
#else
    return adapter->callback.sock_recv(adapter->user, conn, data, size, addr);
#endif
}

void mobile_cb_update_number(struct mobile_adapter *adapter, enum mobile_number type, const char *number)
{
#ifdef MOBILE_LIBCONF_IMPL_WEAK
    mobile_impl_update_number(adapter->user, type, number);
#else
    adapter->callback.update_number(adapter->user, type, number);
#endif
}

#ifndef MOBILE_LIBCONF_IMPL_WEAK
void mobile_def_debug_log(struct mobile_adapter *adapter, mobile_func_debug_log func)
{
    adapter->callback.debug_log = func;
}

void mobile_def_serial_disable(struct mobile_adapter *adapter, mobile_func_serial_disable func)
{
    adapter->callback.serial_disable = func;
}

void mobile_def_serial_enable(struct mobile_adapter *adapter, mobile_func_serial_enable func)
{
    adapter->callback.serial_enable = func;
}

void mobile_def_config_read(struct mobile_adapter *adapter, mobile_func_config_read func)
{
    adapter->callback.config_read = func;
}

void mobile_def_config_write(struct mobile_adapter *adapter, mobile_func_config_write func)
{
    adapter->callback.config_write = func;
}

void mobile_def_time_latch(struct mobile_adapter *adapter, mobile_func_time_latch func)
{
    adapter->callback.time_latch = func;
}

void mobile_def_time_check_ms(struct mobile_adapter *adapter, mobile_func_time_check_ms func)
{
    adapter->callback.time_check_ms = func;
}

void mobile_def_sock_open(struct mobile_adapter *adapter, mobile_func_sock_open func)
{
    adapter->callback.sock_open = func;
}

void mobile_def_sock_close(struct mobile_adapter *adapter, mobile_func_sock_close func)
{
    adapter->callback.sock_close = func;
}

void mobile_def_sock_connect(struct mobile_adapter *adapter, mobile_func_sock_connect func)
{
    adapter->callback.sock_connect = func;
}

void mobile_def_sock_listen(struct mobile_adapter *adapter, mobile_func_sock_listen func)
{
    adapter->callback.sock_listen = func;
}

void mobile_def_sock_accept(struct mobile_adapter *adapter, mobile_func_sock_accept func)
{
    adapter->callback.sock_accept = func;
}

void mobile_def_sock_send(struct mobile_adapter *adapter, mobile_func_sock_send func)
{
    adapter->callback.sock_send = func;
}

void mobile_def_sock_recv(struct mobile_adapter *adapter, mobile_func_sock_recv func)
{
    adapter->callback.sock_recv = func;
}

void mobile_def_update_number(struct mobile_adapter *adapter, mobile_func_update_number func)
{
    adapter->callback.update_number = func;
}
#endif
