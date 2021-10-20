// SPDX-License-Identifier: LGPL-3.0-or-later
#include "mobile.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "compat.h"

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
        memset(buffer + offset, 0, 4 - (offset % 4));
        offset += 4 - (offset % 4);
    }

    uint16_t checksum = 0;
    for (unsigned i = 0; i < offset; i++) {
        checksum += buffer[i];
    }
    buffer[offset + 0] = (checksum >> 8) & 0xFF;
    buffer[offset + 1] = checksum & 0xFF;
}

static bool command_handle(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // If the packet hasn't been parsed yet, parse and store it
    if (!s->packet_parsed) {
        packet_parse(&s->packet, adapter->serial.buffer);
        mobile_debug_command(adapter, &s->packet, false);
        s->processing = 0;
        s->packet_parsed = true;
    }

    struct mobile_packet *send = mobile_commands_process(adapter, &s->packet);

    // If there's a packet to be sent, write it out and return true
    if (send) {
        mobile_debug_command(adapter, send, true);
        packet_create(adapter->serial.buffer, send,
            adapter->serial.mode_32bit);
        s->packet_parsed = false;
        return true;
    }

    return false;
}

static void mode_32bit_change(struct mobile_adapter *adapter)
{
    adapter->serial.mode_32bit = adapter->commands.mode_32bit;
    // TODO: Signal hardware to change?
}

enum mobile_action mobile_action_get(struct mobile_adapter *adapter)
{
    void *_u = adapter->user;

    enum mobile_serial_state serial_state = adapter->serial.state;

    // If the adapter is stuck waiting, with no signal from the game,
    //   put it out of its misery.
    // Timeout has been verified on hardware.
    if (adapter->commands.session_begun &&
            mobile_board_time_check_ms(_u, MOBILE_TIMER_SERIAL, 3000)) {
        return MOBILE_ACTION_DROP_CONNECTION;
    }

    // If the serial was active recently, but nothing has been obtained since,
    //   reset it.
    if (adapter->serial.active &&
            !adapter->commands.session_begun &&
            mobile_board_time_check_ms(_u, MOBILE_TIMER_SERIAL, 3000)) {
        return MOBILE_ACTION_RESET;
    }

    // Process a packet if one is waiting
    if (serial_state == MOBILE_SERIAL_RESPONSE_WAITING) {
        return MOBILE_ACTION_PROCESS_COMMAND;
    }

    // If the mode_32bit should be changed, change it.
    if (serial_state == MOBILE_SERIAL_WAITING &&
            adapter->commands.mode_32bit != adapter->serial.mode_32bit) {
        return MOBILE_ACTION_CHANGE_32BIT_MODE;
    }

    // If nothing else is being triggered, reset the serial periodically,
    //   in an attempt to synchronize.
    if (!adapter->serial.active &&
            !adapter->commands.session_begun &&
            mobile_board_time_check_ms(_u, MOBILE_TIMER_SERIAL, 500)) {
        return MOBILE_ACTION_RESET_SERIAL;
    }

    return MOBILE_ACTION_NONE;
}

void mobile_action_process(struct mobile_adapter *adapter, enum mobile_action action)
{
    void *_u = adapter->user;

    switch (action) {
    case MOBILE_ACTION_PROCESS_COMMAND:
        // Pretty much everything's in a wonky state if this isn't the case...
        if (adapter->serial.state != MOBILE_SERIAL_RESPONSE_WAITING) break;

        if (command_handle(adapter)) {
            adapter->serial.state = MOBILE_SERIAL_RESPONSE_START;
        }
        break;

    case MOBILE_ACTION_CHANGE_32BIT_MODE:
        mobile_board_serial_disable(_u);
        mode_32bit_change(adapter);
        mobile_board_serial_enable(_u);
        break;

    case MOBILE_ACTION_DROP_CONNECTION:
        mobile_board_serial_disable(_u);

        // "Emulate" an end session.
        mobile_serial_init(adapter);
        mobile_commands_reset(adapter);
        mode_32bit_change(adapter);
        adapter->commands.packet_parsed = false;
        adapter->serial.active = false;
        mobile_debug_print(adapter, PSTR("<<< 11 End session (timeout)"));
        mobile_debug_endl(adapter);
        mobile_debug_endl(adapter);

        mobile_board_time_latch(_u, MOBILE_TIMER_SERIAL);
        mobile_board_serial_enable(_u);
        break;

    case MOBILE_ACTION_RESET:
        mobile_board_serial_disable(_u);

        adapter->commands.mode_32bit = false;
        mode_32bit_change(adapter);
        adapter->commands.packet_parsed = false;
        adapter->serial.active = false;

        mobile_board_time_latch(_u, MOBILE_TIMER_SERIAL);
        mobile_board_serial_enable(_u);
        break;

    case MOBILE_ACTION_RESET_SERIAL:
        mobile_board_serial_disable(_u);
        mobile_board_time_latch(_u, MOBILE_TIMER_SERIAL);
        mobile_board_serial_enable(_u);
        break;

    default:
        break;
    }
}

void mobile_loop(struct mobile_adapter *adapter) {
    mobile_action_process(adapter, mobile_action_get(adapter));
}

unsigned char mobile_transfer(struct mobile_adapter *adapter, unsigned char c) {
    mobile_board_time_latch(adapter->user, MOBILE_TIMER_SERIAL);
    adapter->serial.active = true;

    // Nothing should be done while switching the mode_32bit
    // This should be picked up by mobile_action_get/mobile_action_process
    if (adapter->serial.state == MOBILE_SERIAL_WAITING &&
            adapter->commands.mode_32bit != adapter->serial.mode_32bit) {
        return 0xD2;
    }

    return mobile_serial_transfer(adapter, c);
}

#define MOBILE_CONFIG_SIZE_INTERNAL 0xC0
#if MOBILE_CONFIG_SIZE_INTERNAL > MOBILE_CONFIG_SIZE
#error "MOBILE_CONFIG_SIZE isn't big enough!"
#endif

static void config_clear(void *user)
{
    unsigned char buffer[MOBILE_CONFIG_SIZE_INTERNAL] = {0};
    mobile_board_config_write(user, buffer, 0, MOBILE_CONFIG_SIZE_INTERNAL);
}

static bool config_verify(void *user)
{
    unsigned char buffer[MOBILE_CONFIG_SIZE_INTERNAL];
    mobile_board_config_read(user, buffer, 0, MOBILE_CONFIG_SIZE_INTERNAL);
    if (buffer[0] != 'M' || buffer[1] != 'A') {
        return false;
    }

    uint16_t checksum = 0;
    for (unsigned i = 0; i < MOBILE_CONFIG_SIZE_INTERNAL - 2; i++) {
        checksum += buffer[i];
    }
    uint16_t config_checksum = buffer[MOBILE_CONFIG_SIZE_INTERNAL - 2] << 8 |
                               buffer[MOBILE_CONFIG_SIZE_INTERNAL - 1];
    return checksum == config_checksum;
}

void mobile_init(struct mobile_adapter *adapter, void *user, const struct mobile_adapter_config *config)
{
    adapter->user = user;

    if (!config_verify(user)) config_clear(user);

    if (config) {
        adapter->config = *config;
    } else {
        adapter->config = MOBILE_ADAPTER_CONFIG_DEFAULT;
    }
    mobile_board_time_latch(user, MOBILE_TIMER_SERIAL);
    mobile_debug_init(adapter);
    mobile_commands_init(adapter);
    mobile_serial_init(adapter);
    mobile_dns_init(adapter);
    mobile_board_serial_enable(user);
}
