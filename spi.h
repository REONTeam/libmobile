#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "commands.h"

enum mobile_spi_state {
    MOBILE_SPI_WAITING,
    MOBILE_SPI_DATA,
    MOBILE_SPI_CHECKSUM,
    MOBILE_SPI_ACKNOWLEDGE,
    MOBILE_SPI_RESPONSE_WAITING,
    MOBILE_SPI_RESPONSE_START,
    MOBILE_SPI_RESPONSE_DATA,
    MOBILE_SPI_RESPONSE_ACKNOWLEDGE
};

extern unsigned char mobile_spi_buffer[4 + MOBILE_MAX_DATA_SIZE + 2];
extern volatile enum mobile_spi_state mobile_spi_state;
extern volatile unsigned mobile_spi_current;

unsigned char mobile_transfer(unsigned char c);

#ifdef __cplusplus
}
#endif
