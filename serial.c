// SPDX-License-Identifier: LGPL-3.0-or-later
#include "serial.h"

#include "mobile_data.h"

void mobile_serial_init(struct mobile_adapter *adapter)
{
    adapter->serial.state = MOBILE_SERIAL_INIT;
    adapter->serial.mode_32bit = false;
    adapter->serial.active = false;
}

uint8_t mobile_serial_transfer(struct mobile_adapter *adapter, uint8_t c)
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
            b->error = 0;

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

        if (b->current < sizeof(b->header)) break;

        // Done receiving the header, read content size.
        b->data_size = b->header[3];

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
            b->error = MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND;
        }

        b->current = 0;
        if (b->data_size) {
            s->state = MOBILE_SERIAL_DATA;
        } else {
            s->state = MOBILE_SERIAL_CHECKSUM;
        }
        break;

    case MOBILE_SERIAL_DATA:
        // Receive the data
        s->buffer[b->current++] = c;
        b->checksum += c;
        if (b->current >= b->data_size) {
            if (s->mode_32bit && b->current % 4) {
                b->current = 4 - (b->current % 4);
                s->state = MOBILE_SERIAL_DATA_PAD;
            } else {
                b->current = 0;
                s->state = MOBILE_SERIAL_CHECKSUM;
            }
        }
        break;

    case MOBILE_SERIAL_DATA_PAD:
        // In 32bit mode, we must add some extra padding
        if (!--b->current) s->state = MOBILE_SERIAL_CHECKSUM;
        break;

    case MOBILE_SERIAL_CHECKSUM:
        // Receive the checksum, verify it when done.
        b->footer[b->current++] = c;
        if (b->current >= sizeof(b->footer)) {
            uint16_t in_checksum = b->footer[0] << 8 | b->footer[1];
            if (b->checksum != in_checksum) {
                b->error = MOBILE_SERIAL_ERROR_CHECKSUM;
            }
            b->current = 0;
            s->state = MOBILE_SERIAL_ACKNOWLEDGE;
            return s->device | 0x80;
        }
        break;

    case MOBILE_SERIAL_ACKNOWLEDGE:
        // Receive the acknowledgement byte, send error if applicable.

        // The blue adapter doesn't check the device ID apparently,
        //   the other adapters don't check it in 32bit mode.
        if (s->device != MOBILE_ADAPTER_BLUE &&
                c != (MOBILE_ADAPTER_GAMEBOY | 0x80) &&
                c != (MOBILE_ADAPTER_GAMEBOY_ADVANCE | 0x80)) {
            s->state = MOBILE_SERIAL_WAITING;
            // Yellow/Red adapters are probably bugged to return the
            //   device ID again, instead of the idle byte.
            break;
        }

        b->current = 1;
        s->state = MOBILE_SERIAL_IDLE_CHECK;
        return b->error ? b->error : b->header[0] ^ 0x80;

    case MOBILE_SERIAL_IDLE_CHECK:
        // Skip at least one byte
        if (b->current--) break;

        // If an error was raised or the empty command was sent, reset here.
        if (b->header[0] == MOBILE_COMMAND_NULL || b->error) {
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
            b->error = 0;
            b->current = 0;
            s->state = MOBILE_SERIAL_RESPONSE_HEADER;
            return 0x66;
        }

    case MOBILE_SERIAL_RESPONSE_HEADER:
        c = b->header[b->current++];
        if (b->current >= sizeof(b->header)) {
            b->current = 0;
            if (b->data_size) {
                s->state = MOBILE_SERIAL_RESPONSE_DATA;
            } else {
                s->state = MOBILE_SERIAL_RESPONSE_CHECKSUM;
            }
        }
        return c;

    case MOBILE_SERIAL_RESPONSE_DATA:
        // Send all that's in the response buffer.
        // This includes the header, content and the checksum.
        c = s->buffer[b->current++];
        if (b->current >= b->data_size) {
            if (s->mode_32bit && b->current % 4) {
                b->current = 4 - (b->current % 4);
                s->state = MOBILE_SERIAL_RESPONSE_DATA_PAD;
            } else {
                b->current = 0;
                s->state = MOBILE_SERIAL_RESPONSE_CHECKSUM;
            }
        }
        return c;

    case MOBILE_SERIAL_RESPONSE_DATA_PAD:
        // In 32bit mode, we must add some extra padding
        if (!--b->current) s->state = MOBILE_SERIAL_RESPONSE_CHECKSUM;
        return 0;

    case MOBILE_SERIAL_RESPONSE_CHECKSUM:
        c = b->footer[b->current++];
        if (b->current >= sizeof(b->footer)) {
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
            b->error = c;
        }

        if (s->mode_32bit && b->current < 4) {
            b->current++;
            return 0;
        }

        b->current = 0;
        // If an error happened, retry.
        if (b->error == MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND ||
                b->error == MOBILE_SERIAL_ERROR_CHECKSUM ||
                b->error == MOBILE_SERIAL_ERROR_INTERNAL) {
            s->state = MOBILE_SERIAL_RESPONSE_START;
            break;
        }
        // Start over after this
        s->state = MOBILE_SERIAL_WAITING;
        break;
    }

    return MOBILE_SERIAL_IDLE_BYTE;
}

uint32_t mobile_serial_transfer_32bit(struct mobile_adapter *adapter, uint32_t c)
{
    struct mobile_adapter_serial *s = &adapter->serial;
    struct mobile_buffer_serial *b = &adapter->buffer.serial;

    // Unpack the data
    uint8_t d[4] = {c >> 24, c >> 16, c >> 8, c >> 0};

    // Use the 8-bit logic for most things
    for (unsigned i = 0; i < 4; i++) {
        d[i] = mobile_serial_transfer(adapter, d[i]);
    }

    // Handle acknowledgement footer separately
    // This is the entire reason the functions are split at all, the checksum
    //   isn't available in the 8bit function by the time the error byte has to
    //   be sent.
    // For this same reason, the received device byte can't be verified either.
    if (s->state == MOBILE_SERIAL_ACKNOWLEDGE) {
        d[0] = s->device | 0x80;
        d[1] = b->error ? b->error : b->header[0] ^ 0x80;
        d[2] = 0;
        d[3] = 0;

        // Ignore the next packet, we can't do anything with it.
        b->current = 4;
        s->state = MOBILE_SERIAL_IDLE_CHECK;
    }

    // Repack the data
    return d[0] << 24 | d[1] << 16 | d[2] << 8 | d[3] << 0;
}
