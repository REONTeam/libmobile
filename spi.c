#include "spi.h"

#include "mobile.h"
#include "commands.h"

enum mobile_error {
    MOBILE_ERROR_UNKNOWN = 0xF0,
    MOBILE_ERROR_CHECKSUM,
};

static const enum mobile_adapter adapter = MOBILE_ADAPTER_BLUE;

unsigned char mobile_spi_buffer[4 + MOBILE_MAX_DATA_SIZE + 2];
volatile enum mobile_spi_state mobile_spi_state;
volatile unsigned mobile_spi_current;

unsigned char mobile_transfer(unsigned char c)
{
    static unsigned data_length;
    static uint16_t checksum;
    static enum mobile_error error;
    static int send_retry;

    mobile_board_time_latch();

    switch (mobile_spi_state) {
    case MOBILE_SPI_WAITING:
        if (c == 0x99) {
            mobile_spi_current = 1;
        } else if (c == 0x66 && mobile_spi_current == 1) {
            mobile_spi_current = 0;
            data_length = 0;
            checksum = 0;
            error = 0;
            mobile_spi_state = MOBILE_SPI_DATA;
        } else {
            mobile_spi_current = 0;
        }
        break;

    case MOBILE_SPI_DATA:
        mobile_spi_buffer[mobile_spi_current++] = c;
        checksum += c;
        if (mobile_spi_current == 4) {
            data_length = mobile_spi_buffer[3];
            if (!mobile_session_begun &&
                    mobile_spi_buffer[0] != MOBILE_COMMAND_BEGIN_SESSION) {
                mobile_spi_current = 0;
                mobile_spi_state = MOBILE_SPI_WAITING;
            }
        } else if (mobile_spi_current >= data_length + 4) {
            mobile_spi_state = MOBILE_SPI_CHECKSUM;
        }
        break;

    case MOBILE_SPI_CHECKSUM:
        mobile_spi_buffer[mobile_spi_current++] = c;
        if (mobile_spi_current >= data_length + 6) {
            uint16_t in_checksum = mobile_spi_buffer[mobile_spi_current - 2] << 8 |
                                   mobile_spi_buffer[mobile_spi_current - 1];
            if (checksum != in_checksum) error = MOBILE_ERROR_CHECKSUM;
            mobile_spi_state = MOBILE_SPI_ACKNOWLEDGE;
            return adapter;
        }
        break;

    case MOBILE_SPI_ACKNOWLEDGE:
        mobile_spi_current = 0;
        mobile_spi_state = MOBILE_SPI_WAITING;
        if (error) return error;
        if (c != 0x80) return MOBILE_ERROR_UNKNOWN;

        send_retry = 0;
        mobile_spi_state = MOBILE_SPI_RESPONSE_WAITING;
        return mobile_spi_buffer[0] ^ 0x80;

    case MOBILE_SPI_RESPONSE_WAITING:
        // Don't do anything
        break;

    case MOBILE_SPI_RESPONSE_START:
        if (mobile_spi_current++ == 0) {
            return 0x99;
        } else {
            data_length = mobile_spi_buffer[3];
            mobile_spi_current = 0;
            mobile_spi_state = MOBILE_SPI_RESPONSE_DATA;
            return 0x66;
        }

    case MOBILE_SPI_RESPONSE_DATA:
        c = mobile_spi_buffer[mobile_spi_current++];
        if (mobile_spi_current > data_length + 6) {
            mobile_spi_current = 0;
            mobile_spi_state = MOBILE_SPI_RESPONSE_ACKNOWLEDGE;
            return adapter;
        }
        return c;

    case MOBILE_SPI_RESPONSE_ACKNOWLEDGE:
        if (mobile_spi_current++ == 0) {
            return 0;
        } else {
            // TODO: Actually parse the error code.
            if ((c ^ 0x80) != mobile_spi_buffer[0]) {
                if (++send_retry < 4) {
                    mobile_spi_current = 1;
                    mobile_spi_state = MOBILE_SPI_RESPONSE_START;
                    return 0x99;
                }
            }

            mobile_spi_current = 0;
            mobile_spi_state = MOBILE_SPI_WAITING;
        }
    }

    return 0xD2;
}
