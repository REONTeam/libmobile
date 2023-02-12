// SPDX-License-Identifier: LGPL-3.0-or-later
#include "serial.h"

#include "mobile_data.h"

void mobile_serial_init(struct mobile_adapter *adapter)
{
    adapter->serial.state = MOBILE_SERIAL_WAITING;
    adapter->serial.current = 0;
    adapter->serial.mode_32bit = false;
    adapter->serial.active = false;
}

unsigned char mobile_serial_transfer(struct mobile_adapter *adapter, unsigned char c)
{
    struct mobile_adapter_serial *s = &adapter->serial;

    enum mobile_serial_state state = s->state;  // Workaround for atomic load in clang...
    switch (state) {
    case MOBILE_SERIAL_WAITING:
        // Wait for the bytes that indicate a packet will be sent.
        if (c == 0x99) {
            s->current = 1;
        } else if (c == 0x66 && s->current == 1) {
            // Initialize transfer state
            s->data_size = 0;
            s->checksum = 0;
            s->error = 0;

            s->current = 0;
            s->state = MOBILE_SERIAL_DATA;
        } else {
            s->current = 0;
        }
        break;

    case MOBILE_SERIAL_DATA:
        // Receive the header and data. Calculate the checksum while at it.
        s->buffer[s->current++] = c;
        s->checksum += c;
        if (s->current == 4) {
            // Done receiving the header, read content size.
            s->data_size = s->buffer[3];
            if (s->mode_32bit && s->data_size % 4 != 0) {
                s->data_size += 4 - (s->data_size % 4);
            }

            // Data size is a u16be, but it may not be bigger than 0xff...
            if (s->buffer[2] != 0) {
                s->current = 0;
                s->state = MOBILE_SERIAL_WAITING;
            }

            if (!adapter->commands.session_begun) {
                // If we haven't begun a session, this is as good as any place to
                //   stop parsing, as we shouldn't react to this.
                // TODO: Re-verify this behavior on hardware.
                if (s->buffer[0] != MOBILE_COMMAND_BEGIN_SESSION) {
                    s->current = 0;
                    s->state = MOBILE_SERIAL_WAITING;
                }

                // Update device type
                unsigned char d = adapter->config.device;
                s->device = d & ~MOBILE_CONFIG_DEVICE_UNMETERED;
                s->device_unmetered = d & MOBILE_CONFIG_DEVICE_UNMETERED;
            }

            // If the command doesn't exist, set the error...
            if (!mobile_commands_exists(s->buffer[0])) {
                s->error = MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND;
            }
        } else if (s->current >= s->data_size + 4) {
            s->state = MOBILE_SERIAL_CHECKSUM;
        }
        break;

    case MOBILE_SERIAL_CHECKSUM:
        // Receive the checksum, verify it when done.
        s->buffer[s->current++] = c;
        if (s->current >= s->data_size + 6) {
            uint16_t in_checksum = s->buffer[s->current - 2] << 8 |
                                   s->buffer[s->current - 1];
            if (s->checksum != in_checksum) {
                s->error = MOBILE_SERIAL_ERROR_CHECKSUM;
            }
            s->current = 0;
            s->state = MOBILE_SERIAL_ACKNOWLEDGE;
            return s->device | 0x80;
        }
        break;

    case MOBILE_SERIAL_ACKNOWLEDGE:
        // Receive the acknowledgement byte, send error if applicable.

        // Perform requested alignment when in 32bit mode
        if (s->current > 0) {
            if (s->current++ == 2) {
                s->current = 0;
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
            s->current++;
        } else {
            s->state = MOBILE_SERIAL_IDLE_CHECK;
        }
        if (s->error) return s->error;
        return s->buffer[0] ^ 0x80;

    case MOBILE_SERIAL_IDLE_CHECK:
        // Delay one byte
        if (s->current++ < 1) {
            break;
        }
        s->current = 0;

        // If an error was raised or the empty command was sent, reset here.
        if (s->buffer[0] == MOBILE_COMMAND_EMPTY || s->error) {
            s->state = MOBILE_SERIAL_WAITING;
            if (c == 0x99) s->current = 1;
            break;
        }

        // If an idle byte isn't received, reset here.
        if (c != 0x4B) {
            s->state = MOBILE_SERIAL_WAITING;
            if (c == 0x99) s->current = 1;
            break;
        }

        // Otherwise, start processing
        s->state = MOBILE_SERIAL_RESPONSE_WAITING;
        break;

    case MOBILE_SERIAL_RESPONSE_WAITING:
        // Wait while processing the received packet and crafting a response.
        break;

    case MOBILE_SERIAL_RESPONSE_START:
        // Start sending the response.
        if (s->current++ == 0) {
            return 0x99;
        } else {
            s->data_size = s->buffer[3];
            if (s->mode_32bit && s->data_size % 4 != 0) {
                s->data_size += 4 - (s->data_size % 4);
            }

            s->current = 0;
            s->state = MOBILE_SERIAL_RESPONSE_DATA;
            return 0x66;
        }

    case MOBILE_SERIAL_RESPONSE_DATA:
        // Send all that's in the response buffer.
        // This includes the header, content and the checksum.
        c = s->buffer[s->current++];
        if (s->current >= s->data_size + 6) {
            s->current = 0;
            s->state = MOBILE_SERIAL_RESPONSE_ACKNOWLEDGE;
        }
        return c;

    case MOBILE_SERIAL_RESPONSE_ACKNOWLEDGE:
        if (s->current == 0) {
            s->current++;
            return s->device | 0x80;
        } else if (s->current == 1) {
            // There's nothing we can do with the received device ID.
            // In fact, the real adapter doesn't care for this value, either.
            s->current++;
            return 0;
        } else if (s->current == 2) {
            // Catch the error
            s->error = c;
        }

        if (s->mode_32bit && s->current < 4) {
            s->current++;
            return 0;
        }

        s->current = 0;
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
