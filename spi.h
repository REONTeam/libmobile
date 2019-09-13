#pragma once

#include <stdint.h>

#include "commands.h"
struct mobile_adapter;

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

enum mobile_spi_error {
    MOBILE_SPI_ERROR_UNKNOWN = 0xF0,
    MOBILE_SPI_ERROR_CHECKSUM,
};

struct mobile_adapter_spi {
    enum mobile_spi_state state;
    unsigned char buffer[4 + MOBILE_MAX_DATA_SIZE + 2];  // header, content, checksum
    unsigned current;
    unsigned data_size;
    uint16_t checksum;
    enum mobile_spi_error error;
    unsigned retries;
};

void mobile_spi_reset(struct mobile_adapter *a);
unsigned char mobile_transfer(struct mobile_adapter *a, unsigned char c);
