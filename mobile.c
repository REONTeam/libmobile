// SPDX-License-Identifier: LGPL-3.0-or-later
#include "global.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "mobile_data.h"
#include "compat.h"

#ifdef MOBILE_LIBCONF_USE
#include <mobile_config.h>
#endif

static const int number_fetch_conn = 0;

static void mobile_global_init(struct mobile_adapter *adapter)
{
    adapter->global.start = false;
    adapter->global.active = false;
    adapter->global.packet_parsed = false;
    adapter->global.number_fetch_active = false;
    adapter->global.number_fetch_retries = 3;
}

static void debug_prefix(struct mobile_adapter *adapter)
{
    mobile_debug_print(adapter, PSTR("<GLOBAL> "));
}

static void mode_32bit_change(struct mobile_adapter *adapter)
{
    adapter->serial.mode_32bit = adapter->commands.mode_32bit;
}

static void mobile_reset(struct mobile_adapter *adapter)
{
    // End the session and reset
    mobile_commands_reset(adapter);
    mode_32bit_change(adapter);
    mobile_serial_init(adapter);
    adapter->global.active = false;
    adapter->global.packet_parsed = false;
}

static struct mobile_packet packet_parse(struct mobile_adapter *adapter)
{
    struct mobile_adapter_serial *s = &adapter->serial;
    struct mobile_buffer_serial *b = &adapter->buffer.serial;

    struct mobile_packet packet = {
        .command = b->header[0],
        .length = b->header[3],
        .data = s->buffer
    };

    return packet;
}

static void packet_create(struct mobile_adapter *adapter, struct mobile_packet packet)
{
    struct mobile_adapter_serial *s = &adapter->serial;
    struct mobile_buffer_serial *b = &adapter->buffer.serial;

    b->header[0] = packet.command | 0x80;
    b->header[1] = 0;
    b->header[2] = 0;
    b->header[3] = packet.length;
    memmove(s->buffer, packet.data, packet.length);

    unsigned checksum = 0;
    for (unsigned i = 0; i < sizeof(b->header); i++) checksum += b->header[i];
    for (unsigned i = 0; i < packet.length; i++) checksum += s->buffer[i];
    b->footer[0] = checksum >> 8;
    b->footer[1] = checksum;
}

static bool command_handle(struct mobile_adapter *adapter)
{
    struct mobile_adapter_global *s = &adapter->global;

    struct mobile_packet *packet = &adapter->buffer.commands.packet;

    // If the packet hasn't been parsed yet, parse and store it
    if (!s->packet_parsed) {
        *packet = packet_parse(adapter);
        mobile_debug_command(adapter, packet, false);
        adapter->buffer.commands.processing = 0;
        s->packet_parsed = true;
    }

    struct mobile_packet *send = mobile_commands_process(adapter, packet);

    // If there's a packet to be sent, write it out and return true
    if (send) {
        mobile_debug_command(adapter, send, true);
        packet_create(adapter, *send);
        s->packet_parsed = false;
        return true;
    }

    return false;
}

void mobile_number_fetch_cancel(struct mobile_adapter *adapter)
{
    if (adapter->global.number_fetch_active) {
        mobile_cb_sock_close(adapter, number_fetch_conn);
        adapter->global.number_fetch_active = false;
    }
}

void mobile_number_fetch_reset(struct mobile_adapter *adapter)
{
    mobile_number_fetch_cancel(adapter);

    if (!adapter->global.number_fetch_retries) {
        mobile_cb_update_number(adapter, MOBILE_NUMBER_USER, NULL);
    }

    adapter->global.number_fetch_retries = 3;
}

static void number_fetch_handle(struct mobile_adapter *adapter)
{
    if (!adapter->global.number_fetch_active) {
        debug_prefix(adapter);
        mobile_debug_print(adapter, "Checking mobile number...");
        mobile_debug_endl(adapter);

        if (adapter->global.number_fetch_retries) {
            adapter->global.number_fetch_retries--;
        }
        mobile_relay_init(adapter);
        mobile_cb_time_latch(adapter, MOBILE_TIMER_COMMAND);
        mobile_cb_sock_open(adapter, number_fetch_conn, MOBILE_SOCKTYPE_TCP,
            adapter->config.relay.type, 0);
        adapter->global.number_fetch_active = true;
    } else if (mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND, 3000)) {
        debug_prefix(adapter);
        mobile_debug_print(adapter, PSTR("Timeout"));
        mobile_debug_endl(adapter);

        mobile_cb_sock_close(adapter, number_fetch_conn);
        adapter->global.number_fetch_active = false;
        return;
    }

    if (mobile_relay_proc_init_number(adapter, number_fetch_conn,
            &adapter->config.relay) != 0) {
        mobile_cb_sock_close(adapter, number_fetch_conn);
        adapter->global.number_fetch_active = false;
    }
}

enum mobile_action mobile_actions_get(struct mobile_adapter *adapter)
{
    if (!adapter->global.start) return MOBILE_ACTION_NONE;

    enum mobile_action actions = MOBILE_ACTION_NONE;

    // If the serial has been active at all, latch the timer
    if (adapter->serial.active) {
        // NOTE: Race condition possible, but not critical.
        adapter->serial.active = false;
        adapter->global.active = true;
        mobile_cb_time_latch(adapter, MOBILE_TIMER_SERIAL);
    }

    // If the adapter is stuck waiting, with no signal from the game,
    //   put it out of its misery.
    // Timeout has been verified on hardware.
    if (adapter->commands.session_started &&
            mobile_cb_time_check_ms(adapter, MOBILE_TIMER_SERIAL, 3000)) {
        actions |= MOBILE_ACTION_DROP_CONNECTION;
    }

    // If the serial stops receiving data after a while since the session was
    //   ended, perform a reset.
    if (adapter->global.active &&
            !adapter->commands.session_started &&
            mobile_cb_time_check_ms(adapter, MOBILE_TIMER_SERIAL, 3000)) {
        actions |= MOBILE_ACTION_RESET;
    }

    // Process a packet if one is waiting
    if (adapter->serial.state == MOBILE_SERIAL_RESPONSE_WAITING) {
        actions |= MOBILE_ACTION_PROCESS_COMMAND;
    }

    // If nothing else is being triggered, reset the serial periodically,
    //   in an attempt to synchronize.
    if (!adapter->global.active &&
            !adapter->commands.session_started &&
            mobile_cb_time_check_ms(adapter, MOBILE_TIMER_SERIAL, 500)) {
        actions |= MOBILE_ACTION_RESET_SERIAL;
    }

    // If the mode_32bit should be changed, change it.
    if (adapter->serial.state == MOBILE_SERIAL_WAITING &&
            adapter->commands.mode_32bit != adapter->serial.mode_32bit) {
        actions |= MOBILE_ACTION_CHANGE_32BIT_MODE;
    }

    // If the config is in need of updating, do that.
    if (adapter->config.dirty) {
        actions |= MOBILE_ACTION_WRITE_CONFIG;
    }

    // When we have time for it, attempt to fetch the user's number
    if (adapter->global.number_fetch_active || (
                !adapter->global.active &&
                adapter->global.number_fetch_retries &&
                adapter->config.relay.type != MOBILE_ADDRTYPE_NONE)) {
        actions |= MOBILE_ACTION_INIT_NUMBER;
    }

    return actions;
}

void mobile_actions_process(struct mobile_adapter *adapter, enum mobile_action actions)
{
    // End the session and reset everything
    if (actions & MOBILE_ACTION_DROP_CONNECTION &&
            adapter->commands.session_started) {
        mobile_cb_serial_disable(adapter);

        mobile_debug_print(adapter, PSTR("!!! 11 End session (timeout)"));
        mobile_debug_endl(adapter);
        mobile_debug_endl(adapter);

        mobile_reset(adapter);
        mobile_cb_time_latch(adapter, MOBILE_TIMER_SERIAL);
        mobile_cb_serial_enable(adapter, adapter->serial.mode_32bit);
        return;
    }

    // Resets everything when the session has ended
    if (actions & MOBILE_ACTION_RESET &&
            !adapter->commands.session_started) {
        mobile_cb_serial_disable(adapter);

        // Assume the command subsystem is already reset
        // Avoid resetting the serial subsystem, and retain the parsed packet
        adapter->global.active = false;

        mobile_cb_time_latch(adapter, MOBILE_TIMER_SERIAL);
        mobile_cb_serial_enable(adapter, adapter->serial.mode_32bit);
        return;
    }

    // After the client has finished sending data, process a command
    if (actions & MOBILE_ACTION_PROCESS_COMMAND) {
        if (adapter->serial.state != MOBILE_SERIAL_RESPONSE_WAITING) return;

        if (command_handle(adapter)) {
            adapter->serial.state = MOBILE_SERIAL_RESPONSE_INIT;
        }
        return;
    }

    // Reset the serial's current bit state in an attempt to synchronize
    if (actions & MOBILE_ACTION_RESET_SERIAL) {
        mobile_cb_serial_disable(adapter);
        mobile_cb_time_latch(adapter, MOBILE_TIMER_SERIAL);
        mobile_cb_serial_enable(adapter, adapter->serial.mode_32bit);
        return;
    }

    // Once the exchange has finished, switch the 32bit mode flag
    if (actions & MOBILE_ACTION_CHANGE_32BIT_MODE) {
        if (adapter->serial.state != MOBILE_SERIAL_WAITING) return;

        mobile_cb_serial_disable(adapter);
        mode_32bit_change(adapter);
        mobile_cb_serial_enable(adapter, adapter->serial.mode_32bit);
        return;
    }

    // If the config is dirty, update it in one go
    if (actions & MOBILE_ACTION_WRITE_CONFIG) {
        mobile_config_save(adapter);
        return;
    }

    // Use free time to initialize the phone number
    if (actions & MOBILE_ACTION_INIT_NUMBER) {
        number_fetch_handle(adapter);
        return;
    }
}

void mobile_loop(struct mobile_adapter *adapter)
{
    mobile_actions_process(adapter, mobile_actions_get(adapter));
}

uint8_t mobile_transfer(struct mobile_adapter *adapter, uint8_t c)
{
    adapter->serial.active = true;

    // Nothing should be done while switching the mode_32bit
    // This should be picked up by mobile_actions_get/mobile_actions_process
    if (adapter->serial.state == MOBILE_SERIAL_WAITING &&
            adapter->serial.mode_32bit != adapter->commands.mode_32bit) {
        return MOBILE_SERIAL_IDLE_BYTE;
    }

    return mobile_serial_transfer(adapter, c);
}

uint32_t mobile_transfer_32bit(struct mobile_adapter *adapter, uint32_t c)
{
    adapter->serial.active = true;

    // Nothing should be done while switching the mode_32bit
    // This should be picked up by mobile_actions_get/mobile_actions_process
    if (adapter->serial.state == MOBILE_SERIAL_WAITING &&
            adapter->serial.mode_32bit != adapter->commands.mode_32bit) {
        return MOBILE_SERIAL_IDLE_WORD;
    }

    return mobile_serial_transfer_32bit(adapter, c);
}

void mobile_start(struct mobile_adapter *adapter)
{
    if (adapter->global.start) return;

    if (!adapter->config.loaded) mobile_config_load(adapter);

    adapter->global.start = true;
    mobile_cb_time_latch(adapter, MOBILE_TIMER_SERIAL);
    mobile_cb_serial_enable(adapter, adapter->serial.mode_32bit);
}

void mobile_stop(struct mobile_adapter *adapter)
{
    if (!adapter->global.start) return;

    mobile_cb_serial_disable(adapter);
    adapter->global.start = false;

    if (adapter->commands.session_started) {
        mobile_debug_print(adapter, PSTR("!!! 11 End session (forced)"));
        mobile_debug_endl(adapter);
        mobile_debug_endl(adapter);
    }

    mobile_reset(adapter);
    mobile_config_save(adapter);
}

void mobile_init(struct mobile_adapter *adapter, void *user)
{
    adapter->user = user;

    mobile_global_init(adapter);
    mobile_callback_init(adapter);
    mobile_config_init(adapter);
    mobile_debug_init(adapter);
    mobile_commands_init(adapter);
    mobile_serial_init(adapter);
    mobile_dns_init(adapter);
}

const size_t mobile_sizeof PROGMEM = sizeof(struct mobile_adapter);

#ifndef MOBILE_ENABLE_NOALLOC
#include <stdlib.h>
struct mobile_adapter *mobile_new(void *user)
{
    struct mobile_adapter *adapter = malloc(sizeof(struct mobile_adapter));
    mobile_init(adapter, user);
    return adapter;
}
#endif
