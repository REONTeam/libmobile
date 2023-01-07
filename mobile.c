// SPDX-License-Identifier: LGPL-3.0-or-later
#include "mobile.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "compat.h"

static void mobile_global_init(struct mobile_adapter *adapter)
{
    adapter->global.active = false;
    adapter->global.packet_parsed = false;
}

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
    struct mobile_adapter_global *s = &adapter->global;

    // If the packet hasn't been parsed yet, parse and store it
    if (!s->packet_parsed) {
        packet_parse(&s->packet, adapter->serial.buffer);
        mobile_debug_command(adapter, &s->packet, false);
        adapter->commands.processing = 0;
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

    // If the serial has been active at all, latch the timer
    if (adapter->serial.active) {
        mobile_board_time_latch(_u, MOBILE_TIMER_SERIAL);
        adapter->global.active = true;
        adapter->serial.active = false;
    }

    // If the adapter is stuck waiting, with no signal from the game,
    //   put it out of its misery.
    // Timeout has been verified on hardware.
    if (adapter->commands.session_begun &&
            mobile_board_time_check_ms(_u, MOBILE_TIMER_SERIAL, 3000)) {
        return MOBILE_ACTION_DROP_CONNECTION;
    }

    // If the serial stops receiving data after a while since the session was
    //   ended, perform a reset.
    if (adapter->global.active &&
            !adapter->commands.session_begun &&
            mobile_board_time_check_ms(_u, MOBILE_TIMER_SERIAL, 3000)) {
        return MOBILE_ACTION_RESET;
    }

    // Process a packet if one is waiting
    if (adapter->serial.state == MOBILE_SERIAL_RESPONSE_WAITING) {
        return MOBILE_ACTION_PROCESS_COMMAND;
    }

    // If the mode_32bit should be changed, change it.
    if (adapter->serial.state == MOBILE_SERIAL_WAITING &&
            adapter->commands.mode_32bit != adapter->serial.mode_32bit) {
        return MOBILE_ACTION_CHANGE_32BIT_MODE;
    }

    // If nothing else is being triggered, reset the serial periodically,
    //   in an attempt to synchronize.
    if (!adapter->global.active &&
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
    // After the client has finished sending data, process a command
    case MOBILE_ACTION_PROCESS_COMMAND:
        if (adapter->serial.state != MOBILE_SERIAL_RESPONSE_WAITING) break;

        if (command_handle(adapter)) {
            adapter->serial.state = MOBILE_SERIAL_RESPONSE_START;
        }
        break;

    // Once the exchange has finished, switch the 32bit mode flag
    case MOBILE_ACTION_CHANGE_32BIT_MODE:
        if (adapter->serial.state != MOBILE_SERIAL_WAITING) break;

        mobile_board_serial_disable(_u);
        mode_32bit_change(adapter);
        mobile_board_serial_enable(_u);
        break;

    // End the session and reset everything
    case MOBILE_ACTION_DROP_CONNECTION:
        if (!adapter->commands.session_begun) break;

        mobile_board_serial_disable(_u);

        // End the session and reset
        mobile_commands_reset(adapter);
        mode_32bit_change(adapter);
        mobile_serial_init(adapter);
        mobile_global_init(adapter);

        mobile_debug_print(adapter, PSTR("<<< 11 End session (timeout)"));
        mobile_debug_endl(adapter);
        mobile_debug_endl(adapter);

        mobile_board_time_latch(_u, MOBILE_TIMER_SERIAL);
        mobile_board_serial_enable(_u);
        break;

    // Resets everything when the session has ended
    case MOBILE_ACTION_RESET:
        if (adapter->commands.session_begun) break;

        mobile_board_serial_disable(_u);

        adapter->commands.mode_32bit = false;
        mode_32bit_change(adapter);
        mobile_global_init(adapter);

        mobile_board_time_latch(_u, MOBILE_TIMER_SERIAL);
        mobile_board_serial_enable(_u);
        break;

    // Reset the serial's current bit state in an attempt to synchronize
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

void mobile_init(struct mobile_adapter *adapter, void *user)
{
    adapter->user = user;

    if (!config_verify(user)) config_clear(user);

    mobile_global_init(adapter);
    mobile_config_init(adapter);
    mobile_debug_init(adapter);
    mobile_commands_init(adapter);
    mobile_serial_init(adapter);
    mobile_dns_init(adapter);
    mobile_relay_init(adapter);

    mobile_board_time_latch(user, MOBILE_TIMER_SERIAL);
    mobile_board_serial_enable(user);
}
