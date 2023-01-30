// SPDX-License-Identifier: LGPL-3.0-or-later
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MOBILE_INTERNAL
#include "mobile.h"
#include "compat.h"
struct mobile_adapter;
struct mobile_packet;

// This file contains weakly-linked definitions of the impl functions.
// They're meant to provide a working default for implementations that haven't
//   defined them yet.
// Of course, this is a compiler-specific feature.
// Right now, we only support GCC.

#ifdef A_WEAK

A_WEAK void mobile_impl_debug_log(A_UNUSED void *user, A_UNUSED const char *line) {}
A_WEAK void mobile_impl_serial_disable(A_UNUSED void *user) {}
A_WEAK void mobile_impl_serial_enable(A_UNUSED void *user) {}
A_WEAK bool mobile_impl_config_read(A_UNUSED void *user, void *dest, A_UNUSED uintptr_t offset, size_t size)
{
    memset(dest, 0xFF, size);
    return true;
}
A_WEAK bool mobile_impl_config_write(A_UNUSED void *user, A_UNUSED const void *src, A_UNUSED uintptr_t offset, A_UNUSED size_t size)
{
    return true;
}
A_WEAK void mobile_impl_time_latch(A_UNUSED void *user, A_UNUSED enum mobile_timers timer) {}
A_WEAK bool mobile_impl_time_check_ms(A_UNUSED void *user, A_UNUSED enum mobile_timers timer, A_UNUSED unsigned ms)
{
    return false;
}
A_WEAK bool mobile_impl_sock_open(A_UNUSED void *user, A_UNUSED unsigned conn, A_UNUSED enum mobile_socktype type, A_UNUSED enum mobile_addrtype addrtype, A_UNUSED unsigned bindport)
{
    return true;
}
A_WEAK void mobile_impl_sock_close(A_UNUSED void *user, A_UNUSED unsigned conn) {}
A_WEAK int mobile_impl_sock_connect(A_UNUSED void *user, A_UNUSED unsigned conn, A_UNUSED const struct mobile_addr *addr)
{
    return 1;
}
A_WEAK bool mobile_impl_sock_listen(A_UNUSED void *user, A_UNUSED unsigned conn)
{
    return true;
}
A_WEAK bool mobile_impl_sock_accept(A_UNUSED void *user, A_UNUSED unsigned conn) 
{
    return true;
}
A_WEAK int mobile_impl_sock_send(A_UNUSED void *user, A_UNUSED unsigned conn, A_UNUSED const void *data, A_UNUSED unsigned size, A_UNUSED const struct mobile_addr *addr)
{
    return size;
}
A_WEAK int mobile_impl_sock_recv(A_UNUSED void *user, A_UNUSED unsigned conn, A_UNUSED void *data, A_UNUSED unsigned size, A_UNUSED struct mobile_addr *addr)
{
    return -10;
}
A_WEAK void mobile_impl_update_number(A_UNUSED void *user, A_UNUSED enum mobile_number type, A_UNUSED const char *number) {}

#endif
