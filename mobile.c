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

static void packet_create(unsigned char *buffer, const struct mobile_packet *packet, bool mode_32bit)
{
    buffer[0] = packet->command | 0x80;
    buffer[1] = 0;
    buffer[2] = 0;
    buffer[3] = packet->length;
    memcpy(buffer + 4, packet->data, packet->length);

    unsigned offset = packet->length + 4;

    // Align the offset in 32bit mode
    if (mode_32bit && offset % 4 != 0) {
        offset += 4 - (offset % 4);
    }

    uint16_t checksum = 0;
    for (unsigned i = 0; i < offset; i++) {
        checksum += buffer[i];
    }
    buffer[offset + 0] = (checksum >> 8) & 0xFF;
    buffer[offset + 1] = checksum & 0xFF;
}

enum mobile_action mobile_action_get(struct mobile_adapter *adapter)
{
    void *_u = adapter->user;

    // Process a packet if one is waiting
    if (adapter->serial.state == MOBILE_SERIAL_RESPONSE_WAITING) {
        return MOBILE_ACTION_PROCESS_PACKET;
    }

    // If the connection was interrupted mid-packet, drop it
    if (adapter->serial.state != MOBILE_SERIAL_WAITING &&
            mobile_board_time_check_ms(_u, 500)) {
        return MOBILE_ACTION_DROP_CONNECTION;
    }

    // If the adapter is stuck waiting, with no signal from the game,
    //   put it out of its misery.
    if (adapter->commands.session_begun &&
            mobile_board_time_check_ms(_u, 3000)) {  // Timeout verified on hardware
        return MOBILE_ACTION_DROP_CONNECTION;
    }

    // If no packet is being received, and no session has yet started,
    //   reset the serial periodically, in an attempt to synchronize.
    if (adapter->serial.state == MOBILE_SERIAL_WAITING &&
            !adapter->commands.session_begun &&
            mobile_board_time_check_ms(_u, 500)) {
        return MOBILE_ACTION_RESET_SERIAL;
    }

    return MOBILE_ACTION_NONE;
}

void mobile_action_process(struct mobile_adapter *adapter, enum mobile_action action)
{
    void *_u = adapter->user;

    switch (action) {
    case MOBILE_ACTION_PROCESS_PACKET:
        // Pretty much everything's in a wonky state if this isn't the case...
        if (adapter->serial.state != MOBILE_SERIAL_RESPONSE_WAITING) break;

        {
            bool mode_32bit = adapter->serial.mode_32bit;

            struct mobile_packet packet;
            packet_parse(&packet, adapter->serial.buffer);
            mobile_board_debug_cmd(_u, 0, &packet);

            struct mobile_packet *send = mobile_packet_process(adapter, &packet);
            mobile_board_debug_cmd(_u, 1, send);
            packet_create(adapter->serial.buffer, send, mode_32bit);

            adapter->serial.state = MOBILE_SERIAL_RESPONSE_START;
        }
        break;

    case MOBILE_ACTION_DROP_CONNECTION:
        mobile_board_serial_disable(_u);
        mobile_serial_reset(adapter);
        mobile_board_time_latch(_u);
        mobile_board_serial_enable(_u);

        {
            // "Emulate" a regular end session.
            struct mobile_packet packet;
            packet.command = MOBILE_COMMAND_END_SESSION;
            packet.length = 0;
            struct mobile_packet *send = mobile_packet_process(adapter, &packet);
            mobile_board_debug_cmd(_u, 1, send);
        }
        break;

    case MOBILE_ACTION_RESET_SERIAL:
        mobile_board_serial_disable(_u);
        mobile_board_time_latch(_u);
        mobile_board_serial_enable(_u);
        break;

    default:
        break;
    }
}

void mobile_loop(struct mobile_adapter *adapter) {
    mobile_action_process(adapter, mobile_action_get(adapter));
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

void mobile_init(struct mobile_adapter *adapter, void *user, struct mobile_adapter_config *config)
{
    if (!config_verify(user)) config_clear(user);

    if (config) {
        adapter->config = *config;
    } else {
        adapter->config = MOBILE_ADAPTER_CONFIG_DEFAULT;
    }
    adapter->user = user;
    adapter->commands.session_begun = false;
    mobile_serial_reset(adapter);
    mobile_board_serial_enable(user);
}
