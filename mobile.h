#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Board-specific function prototypes (make sure these are defined elsewhere!)
void mobile_board_reset_spi(void);

unsigned char mobile_transfer(unsigned char c);
void mobile_loop(void);
void mobile_init(void);

#ifdef __cplusplus
}
#endif
