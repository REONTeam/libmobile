#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "mobile.h"

// This file contains weakly-linked definitions of the board-specific functions.
// They're meant to provide a working default for implementations that haven't
//   defined them yet.
// Of course, this is a compiler-specific feature.
// Right now, we only support GCC.

#ifdef __GNUC__
#define A_WEAK __attribute__((weak))
#define A_UNUSED __attribute__((unused))

A_WEAK void mobile_board_disable_spi(void) {}
A_WEAK void mobile_board_enable_spi(void) {}
A_WEAK void mobile_board_debug_cmd(A_UNUSED const int send, A_UNUSED const struct mobile_packet *packet) {}
A_WEAK bool mobile_board_config_read(unsigned char *dest, A_UNUSED const uintptr_t offset, const size_t size)
{
    memset(dest, 0xFF, size);
    return true;
}
A_WEAK bool mobile_board_config_write(A_UNUSED const unsigned char *src, A_UNUSED const uintptr_t offset, A_UNUSED const size_t size)
{
    return true;
}
A_WEAK void mobile_board_time_latch(void) {}
A_WEAK bool mobile_board_time_check_ms(A_UNUSED unsigned ms)
{
    return false;
}
A_WEAK bool mobile_board_tcp_connect(A_UNUSED const char *host, A_UNUSED unsigned port)
{
    return true;
}
bool mobile_board_tcp_listen(A_UNUSED unsigned port)
{
    return true;
}
void mobile_board_tcp_disconnect(void) {}
unsigned char *mobile_board_tcp_transfer(unsigned char *data, A_UNUSED unsigned *size)
{
    return data;
}

#endif
