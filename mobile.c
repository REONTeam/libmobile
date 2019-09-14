#include "mobile.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "serial.h"
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
    void *_u = adapter->user;
    struct mobile_packet packet;

    if (adapter->serial.state == MOBILE_SERIAL_RESPONSE_WAITING) {
        packet_parse(&packet, adapter->serial.buffer);
        mobile_board_debug_cmd(_u, 0, &packet);

        struct mobile_packet *send = mobile_packet_process(adapter, &packet);
        mobile_board_debug_cmd(_u, 1, send);
        packet_create(adapter->serial.buffer, send);

        adapter->serial.state = MOBILE_SERIAL_RESPONSE_START;
    } else if ((adapter->serial.state != MOBILE_SERIAL_WAITING &&
                mobile_board_time_check_ms(_u, 500)) ||
            (adapter->commands.session_begun &&
             mobile_board_time_check_ms(_u, 2000))) {
        // If the adapter is stuck waiting, with no signal from the game,
        //   put it out of its misery.
        mobile_board_serial_disable(_u);
        adapter->commands.session_begun = false;
        mobile_serial_reset(adapter);

        // "Emulate" a regular end session.
        packet.command = MOBILE_COMMAND_END_SESSION;
        packet.length = 0;
        struct mobile_packet *send = mobile_packet_process(adapter, &packet);
        mobile_board_debug_cmd(_u, 1, send);
        mobile_board_serial_enable(_u);
    } else if (adapter->serial.state == MOBILE_SERIAL_WAITING &&
            !adapter->commands.session_begun &&
            mobile_board_time_check_ms(_u, 500)) {
        // Reset the serial state every few if we haven't established a
        //   connection yet. This fixes connectivity issues on hardware.
        mobile_board_serial_disable(_u);
        mobile_board_time_latch(_u);
        mobile_board_serial_enable(_u);
    }
}

static void config_clear(void *user)
{
    unsigned char buffer[MOBILE_CONFIG_SIZE] = {0};
    mobile_board_config_write(user, buffer, 0, MOBILE_CONFIG_SIZE);
}

static bool config_verify(void *user)
{
    unsigned char buffer[MOBILE_CONFIG_SIZE];
    mobile_board_config_read(user, buffer, 0, MOBILE_CONFIG_SIZE);
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

void mobile_init(struct mobile_adapter *adapter, void *user)
{
    if (!config_verify(user)) config_clear(user);

    adapter->user = user;
    adapter->device = MOBILE_ADAPTER_BLUE;
    adapter->commands.session_begun = false;
    mobile_serial_reset(adapter);
    mobile_board_serial_enable(user);
}
