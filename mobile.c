#include "mobile.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "spi.h"
#include "commands.h"

static void parse_packet(struct mobile_packet *packet, const unsigned char *buffer)
{
    packet->command = buffer[0];
    packet->length = buffer[3];
    memcpy(packet->data, buffer + 4, packet->length);
}

static void create_packet(unsigned char *buffer, const struct mobile_packet *packet)
{
    buffer[0] = packet->command ^ 0x80;
    buffer[1] = 0;
    buffer[2] = 0;
    buffer[3] = packet->length;
    memcpy(buffer + 4, packet->data, packet->length);

    uint16_t checksum = 0;
    for (unsigned i = 0; i < packet->length + 4; i++) {
        checksum += buffer[i];
    }
    buffer[packet->length + 4] = (checksum >> 8) & 0xFF;
    buffer[packet->length + 5] = checksum & 0xFF;
}

void mobile_loop(void)
{
    static struct mobile_packet packet;

    if (mobile_spi_state == MOBILE_SPI_RESPONSE_WAITING) {
        parse_packet(&packet, mobile_spi_buffer);
        mobile_board_debug_cmd(0, &packet);

        struct mobile_packet *send = mobile_process_packet(&packet);
        mobile_board_debug_cmd(1, send);
        create_packet(mobile_spi_buffer, send);

        mobile_spi_state = MOBILE_SPI_RESPONSE_START;
    } else if ((mobile_spi_state != MOBILE_SPI_WAITING &&
                mobile_board_time_check_ms(500)) ||
            (mobile_session_begun &&
             mobile_board_time_check_ms(2000))) {
        // If the adapter is stuck waiting, with no signal from the game,
        //   put it out of its misery.
        mobile_board_disable_spi();
        mobile_session_begun = false;
        mobile_spi_current = 0;
        mobile_spi_state = MOBILE_SPI_WAITING;

        // "Emulate" a regular end session.
        packet.command = MOBILE_COMMAND_END_SESSION;
        packet.length = 0;
        struct mobile_packet *send = mobile_process_packet(&packet);
        mobile_board_debug_cmd(1, send);
        mobile_board_enable_spi();
    } else if (mobile_spi_state == MOBILE_SPI_WAITING &&
            !mobile_session_begun &&
            mobile_board_time_check_ms(500)) {
        mobile_board_disable_spi();
        mobile_board_time_latch();
        mobile_board_enable_spi();
    }
}

#if MOBILE_CONFIG_SIZE >= MOBILE_MAX_DATA_SIZE
#error
#endif
static void config_clear(void)
{
    memset(mobile_spi_buffer, 0, MOBILE_CONFIG_SIZE);
    mobile_board_config_write(mobile_spi_buffer, 0, MOBILE_CONFIG_SIZE);
}

static bool config_verify(void)
{
    mobile_board_config_read(mobile_spi_buffer, 0, MOBILE_CONFIG_SIZE);
    if (mobile_spi_buffer[0] != 'M' || mobile_spi_buffer[1] != 'A') {
        return false;
    }

    uint16_t checksum = 0;
    for (unsigned i = 0; i < MOBILE_CONFIG_SIZE - 2; i++) {
        checksum += mobile_spi_buffer[i];
    }
    uint16_t config_checksum = mobile_spi_buffer[MOBILE_CONFIG_SIZE - 2] << 8 |
                               mobile_spi_buffer[MOBILE_CONFIG_SIZE - 1]; 
    return checksum == config_checksum;
}

void mobile_init(void)
{
    if (!config_verify()) config_clear();

    mobile_board_disable_spi();
    mobile_session_begun = false;
    mobile_spi_current = 0;
    mobile_spi_state = MOBILE_SPI_WAITING;
    mobile_board_enable_spi();
}
