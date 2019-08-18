#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "commands.h"

// Board-specific function prototypes (make sure these are defined elsewhere!)
void mobile_board_reset_spi(void);
void mobile_board_debug_cmd(int send, struct mobile_packet *packet);

unsigned char mobile_transfer(unsigned char c);
void mobile_loop(void);
void mobile_init(void);

#ifdef __cplusplus
}
#endif
