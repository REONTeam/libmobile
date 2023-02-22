// SPDX-License-Identifier: LGPL-3.0-or-later
#include "callback.h"

#include <string.h>

#include "mobile_data.h"
#include "compat.h"

// Optional implementations of the callback functions.
// Library internals will always call `mobile_impl_` functions, which will be
//   dispatched using pointers or similar depending on the Library
//   configuration.

static_assert(_MOBILE_MAX_TIMERS == MOBILE_MAX_TIMERS,
        "The MOBILE_MAX_TIMERS definition is out of sync");

// Use weak implementations if requested
#ifdef MOBILE_ENABLE_IMPL_WEAK
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

IMPL void mobile_impl_time_latch(A_UNUSED void *user, A_UNUSED unsigned timer)
{
    return;
}

IMPL bool mobile_impl_time_check_ms(A_UNUSED void *user, A_UNUSED unsigned timer, A_UNUSED unsigned ms)
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
#ifndef MOBILE_ENABLE_IMPL_WEAK
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
    adapter->callback.sock_send = mobile_impl_sock_send;
    adapter->callback.sock_recv = mobile_impl_sock_recv;
    adapter->callback.update_number = mobile_impl_update_number;
#endif
}

#ifndef MOBILE_ENABLE_IMPL_WEAK
#define def(name) \
void mobile_def_ ## name(struct mobile_adapter *adapter, mobile_func_ ## name func) \
{ \
    adapter->callback.name = func; \
}

def(debug_log)
def(serial_disable)
def(serial_enable)
def(config_read)
def(config_write)
def(time_latch)
def(time_check_ms)
def(sock_open)
def(sock_close)
def(sock_connect)
def(sock_listen)
def(sock_accept)
def(sock_send)
def(sock_recv)
def(update_number)
#endif
