#include "mobile.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "spi.h"
#include "commands.h"

static void packet_parse(struct mobile_packet *packet, const unsigned char *buffer)
{
    packet->command = buffer[0];
    packet->length = buffer[3];
    memcpy(packet->data, buffer + 4, packet->length);
}

static void packet_create(unsigned char *buffer, const struct mobile_packet *packet)
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

void mobile_loop(struct mobile_adapter *adapter)
{
    struct mobile_packet packet;

    enum mobile_spi_state state =
        ((volatile struct mobile_adapter *)adapter)->spi.state;

    if (state == MOBILE_SPI_RESPONSE_WAITING) {
        packet_parse(&packet, adapter->spi.buffer);
        mobile_board_debug_cmd(0, &packet);

        struct mobile_packet *send = mobile_packet_process(adapter, &packet);
        mobile_board_debug_cmd(1, send);
        packet_create(adapter->spi.buffer, send);

        adapter->spi.state = MOBILE_SPI_RESPONSE_START;
    } else if ((state != MOBILE_SPI_WAITING &&
                mobile_board_time_check_ms(500)) ||
            (adapter->commands.session_begun &&
             mobile_board_time_check_ms(2000))) {
        // If the adapter is stuck waiting, with no signal from the game,
        //   put it out of its misery.
        mobile_board_disable_spi();
        adapter->commands.session_begun = false;
        mobile_spi_reset(adapter);

        // "Emulate" a regular end session.
        packet.command = MOBILE_COMMAND_END_SESSION;
        packet.length = 0;
        struct mobile_packet *send = mobile_packet_process(adapter, &packet);
        mobile_board_debug_cmd(1, send);
        mobile_board_enable_spi();
    } else if (state == MOBILE_SPI_WAITING &&
            !adapter->commands.session_begun &&
            mobile_board_time_check_ms(500)) {
        // Reset the SPI state every few if we haven't established a
        //   connection yet. This fixes connectivity issues on hardware.
        mobile_board_disable_spi();
        mobile_board_time_latch();
        mobile_board_enable_spi();
    }
}

static void config_clear(void)
{
    char buffer[MOBILE_CONFIG_SIZE] = {0};
    mobile_board_config_write(buffer, 0, MOBILE_CONFIG_SIZE);
}

static bool config_verify(void)
{
    char buffer[MOBILE_CONFIG_SIZE];
    mobile_board_config_read(buffer, 0, MOBILE_CONFIG_SIZE);
    if (buffer[0] != 'M' || buffer[1] != 'A') {
        return false;
    }

    uint16_t checksum = 0;
    for (unsigned i = 0; i < MOBILE_CONFIG_SIZE - 2; i++) {
        checksum += buffer[i];
    }
    uint16_t config_checksum = buffer[MOBILE_CONFIG_SIZE - 2] << 8 |
                               buffer[MOBILE_CONFIG_SIZE - 1]; 
    return checksum == config_checksum;
}

void mobile_init(struct mobile_adapter *adapter)
{
    if (!config_verify()) config_clear();

    mobile_board_disable_spi();
    adapter->device = MOBILE_ADAPTER_BLUE;
    adapter->commands.session_begun = false;
    mobile_spi_reset(adapter);
    mobile_board_enable_spi();
}
