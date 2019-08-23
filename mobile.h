#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "commands.h"

// Board-specific function prototypes (make sure these are defined elsewhere!)
void mobile_board_reset_spi(void);
void mobile_board_debug_cmd(const int send, const struct mobile_packet *packet);
void mobile_board_config_read(unsigned char *dest, const uintptr_t offset, const size_t size);
void mobile_board_config_write(const unsigned char *src, const uintptr_t offset, const size_t size);
void mobile_board_time_latch(void);
bool mobile_board_time_check_ms(unsigned int ms);

unsigned char mobile_transfer(unsigned char c);
void mobile_loop(void);
void mobile_init(void);

#ifdef __cplusplus
}
#endif
