// SPDX-License-Identifier: LGPL-3.0-or-later
#include "serial.h"

#include "mobile_data.h"

void mobile_serial_init(struct mobile_adapter *adapter)
{
    adapter->serial.state = MOBILE_SERIAL_INIT;
    adapter->serial.mode_32bit = false;
    adapter->serial.active = false;
}

unsigned char mobile_serial_transfer(struct mobile_adapter *adapter, unsigned char c)
{
    struct mobile_adapter_serial *s = &adapter->serial;
    struct mobile_buffer_serial *b = &adapter->buffer.serial;

    // Workaround for atomic load in clang...
    enum mobile_serial_state state = s->state;

    switch (state) {
    case MOBILE_SERIAL_INIT:
        b->current = 0;
        s->state = MOBILE_SERIAL_WAITING;
        // fallthrough

    case MOBILE_SERIAL_WAITING:
        // Wait for the bytes that indicate a packet will be sent.
        if (c == 0x99) {
            b->current = 1;
        } else if (c == 0x66 && b->current == 1) {
            // Initialize transfer state
            b->data_size = 0;
            b->checksum = 0;
            s->error = 0;

            b->current = 0;
            s->state = MOBILE_SERIAL_HEADER;
        } else {
            b->current = 0;
        }
        break;

    case MOBILE_SERIAL_HEADER:
        // Receive the header.
        b->header[b->current++] = c;
        b->checksum += c;

        if (b->current < 4) break;

        // Done receiving the header, read content size.
        b->data_size = b->header[3];
        if (s->mode_32bit && b->data_size % 4 != 0) {
            b->data_size += 4 - (b->data_size % 4);
        }

        // Data size is a u16be, but it may not be bigger than 0xff...
        if (b->header[2] != 0) {
            b->current = 0;
            s->state = MOBILE_SERIAL_WAITING;
        }

        if (!adapter->commands.session_started) {
            // If we haven't begun a session, this is as good as any place
            //   to stop parsing, as we shouldn't react to this.
            // TODO: Re-verify this behavior on hardware.
            if (b->header[0] != MOBILE_COMMAND_START) {
                b->current = 0;
                s->state = MOBILE_SERIAL_WAITING;
            }

            // Update device type
            unsigned char d = adapter->config.device;
            s->device = d & ~MOBILE_CONFIG_DEVICE_UNMETERED;
            s->device_unmetered = d & MOBILE_CONFIG_DEVICE_UNMETERED;
        }

        // If the command doesn't exist, set the error...
        if (!mobile_commands_exists(b->header[0])) {
            s->error = MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND;
        }

        b->current = 0;
        s->state = MOBILE_SERIAL_DATA;
        break;

    case MOBILE_SERIAL_DATA:
        // Receive the header and data.
        b->buffer[b->current++] = c;
        b->checksum += c;
        if (b->current >= b->data_size) {
            s->state = MOBILE_SERIAL_CHECKSUM;
        }
        break;

    case MOBILE_SERIAL_CHECKSUM:
        // Receive the checksum, verify it when done.
        b->buffer[b->current++] = c;
        if (b->current >= b->data_size + 2) {
            uint16_t in_checksum = b->buffer[b->current - 2] << 8 |
                                   b->buffer[b->current - 1];
            if (b->checksum != in_checksum) {
                s->error = MOBILE_SERIAL_ERROR_CHECKSUM;
            }
            b->current = 0;
            s->state = MOBILE_SERIAL_ACKNOWLEDGE;
            return s->device | 0x80;
        }
        break;

    case MOBILE_SERIAL_ACKNOWLEDGE:
        // Receive the acknowledgement byte, send error if applicable.

        // Perform requested alignment when in 32bit mode
        if (b->current > 0) {
            if (b->current++ == 2) {
                b->current = 0;
                s->state = MOBILE_SERIAL_IDLE_CHECK;
            }
            return 0;
        }

        // The blue adapter doesn't check the device ID apparently,
        //   the other adapters don't check it in 32bit mode.
        if (s->device != MOBILE_ADAPTER_BLUE &&
                !s->mode_32bit &&
                c != (MOBILE_ADAPTER_GAMEBOY | 0x80) &&
                c != (MOBILE_ADAPTER_GAMEBOY_ADVANCE | 0x80)) {
            s->state = MOBILE_SERIAL_WAITING;
            // Yellow/Red adapters are probably bugged to return the
            //   device ID again, instead of the idle byte.
            break;
        }

        if (s->mode_32bit) {
            // We need to add two extra 0 bytes to the transmission
            b->current++;
        } else {
            s->state = MOBILE_SERIAL_IDLE_CHECK;
        }
        if (s->error) return s->error;
        return b->header[0] ^ 0x80;

    case MOBILE_SERIAL_IDLE_CHECK:
        // Delay one byte
        if (b->current++ < 1) {
            break;
        }
        b->current = 0;

        // If an error was raised or the empty command was sent, reset here.
        if (b->buffer[0] == MOBILE_COMMAND_NULL || s->error) {
            s->state = MOBILE_SERIAL_WAITING;
            if (c == 0x99) b->current = 1;
            break;
        }

        // If an idle byte isn't received, reset here.
        if (c != 0x4B) {
            s->state = MOBILE_SERIAL_WAITING;
            if (c == 0x99) b->current = 1;
            break;
        }

        // Otherwise, start processing
        s->state = MOBILE_SERIAL_RESPONSE_WAITING;
        break;

    case MOBILE_SERIAL_RESPONSE_WAITING:
        // Wait while processing the received packet and crafting a response.
        break;

    case MOBILE_SERIAL_RESPONSE_INIT:
        b->current = 0;
        s->state = MOBILE_SERIAL_RESPONSE_START;
        // fallthrough

    case MOBILE_SERIAL_RESPONSE_START:
        // Start sending the response.
        if (b->current++ == 0) {
            return 0x99;
        } else {
            b->data_size = b->header[3];
            if (s->mode_32bit && b->data_size % 4 != 0) {
                b->data_size += 4 - (b->data_size % 4);
            }

            b->current = 0;
            s->state = MOBILE_SERIAL_RESPONSE_HEADER;
            return 0x66;
        }

    case MOBILE_SERIAL_RESPONSE_HEADER:
        c = b->header[b->current++];
        if (b->current >= 4) {
            b->current = 0;
            s->state = MOBILE_SERIAL_RESPONSE_DATA;
        }
        return c;

    case MOBILE_SERIAL_RESPONSE_DATA:
        // Send all that's in the response buffer.
        // This includes the header, content and the checksum.
        c = b->buffer[b->current++];
        if (b->current >= b->data_size + 2) {
            b->current = 0;
            s->state = MOBILE_SERIAL_RESPONSE_ACKNOWLEDGE;
        }
        return c;

    case MOBILE_SERIAL_RESPONSE_ACKNOWLEDGE:
        if (b->current == 0) {
            b->current++;
            return s->device | 0x80;
        } else if (b->current == 1) {
            // There's nothing we can do with the received device ID.
            // In fact, the real adapter doesn't care for this value, either.
            b->current++;
            return 0;
        } else if (b->current == 2) {
            // Catch the error
            s->error = c;
        }

        if (s->mode_32bit && b->current < 4) {
            b->current++;
            return 0;
        }

        b->current = 0;
        // If an error happened, retry.
        if (s->error == MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND ||
                s->error == MOBILE_SERIAL_ERROR_CHECKSUM ||
                s->error == MOBILE_SERIAL_ERROR_INTERNAL) {
            s->state = MOBILE_SERIAL_RESPONSE_START;
            break;
        }
        // Start over after this
        s->state = MOBILE_SERIAL_WAITING;
        break;
    }

    return 0xD2;
}
