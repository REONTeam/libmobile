#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "commands.h"

// Board-specific function prototypes (make sure these are defined elsewhere!)
// TODO: Actually document these functions, with expectations and assumptions.
void mobile_board_disable_spi(void);
void mobile_board_enable_spi(void);
void mobile_board_debug_cmd(const int send, const struct mobile_packet *packet);
bool mobile_board_config_read(unsigned char *dest, const uintptr_t offset, const size_t size);
bool mobile_board_config_write(const unsigned char *src, const uintptr_t offset, const size_t size);
void mobile_board_time_latch(void);
bool mobile_board_time_check_ms(const unsigned ms);
bool mobile_board_tcp_connect(const char *host, const char *port);
bool mobile_board_tcp_listen(const char *port);
void mobile_board_tcp_disconnect(void);
bool mobile_board_tcp_send(const void *data, const unsigned size);
int mobile_board_tcp_receive(void *data);

unsigned char mobile_transfer(unsigned char c);
void mobile_loop(void);
void mobile_init(void);

#ifdef __cplusplus
}
#endif
