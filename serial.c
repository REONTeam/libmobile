#include "serial.h"

#include "mobile.h"
#include "commands.h"

void mobile_serial_reset(struct mobile_adapter *adapter)
{
    adapter->serial.state = MOBILE_SERIAL_WAITING;
    adapter->serial.mode_32bit = false;
    adapter->serial.current = 0;
}

unsigned char mobile_transfer(struct mobile_adapter *adapter, unsigned char c)
{
    struct mobile_adapter_serial *s = &adapter->serial;

    mobile_board_time_latch(adapter->user);

    // TODO: Set F0 in the acknowledgement byte if the command is unknown
    // Valid commands are:
    // 0xf-0x1a
    // 0x21-0x26
    // 0x28
    // 0x3f

    enum mobile_serial_state state = s->state;  // Workaround for atomic load in clang...
    switch (state) {
    case MOBILE_SERIAL_WAITING:
        // Wait for the bytes that indicate a packet will be sent.
        if (c == 0x99) {
            s->current = 1;
        } else if (c == 0x66 && s->current == 1) {
            // Initialize transfer state
            s->mode_32bit_cur = s->mode_32bit;
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
            if (s->mode_32bit_cur && s->data_size % 4 != 0) {
                s->data_size += 4 - (s->data_size % 4);
            }

            // Data size is a u16, but it may not be bigger than 0xff...
            if (s->buffer[2] != 0) {
                s->current = 0;
                s->state = MOBILE_SERIAL_WAITING;
            }

            // If we haven't begun a session, this is as good as any place to
            //   stop parsing, as we shouldn't react to this.
            if (!adapter->commands.session_begun &&
                    s->buffer[0] != MOBILE_COMMAND_BEGIN_SESSION) {
                s->current = 0;
                s->state = MOBILE_SERIAL_WAITING;
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
            return adapter->config.device | 0x80;
        }
        break;

    case MOBILE_SERIAL_ACKNOWLEDGE:
        // Receive the acknowledgement byte, send error if applicable.

        // Perform requested alignment when in 32-bit mode
        if (s->current > 0) {
            if (s->current++ == 2) {
                s->current = 0;
                s->state = MOBILE_SERIAL_RESPONSE_WAITING;
            }
            return 0;
        }

        // The blue adapter doesn't check the device ID apparently.
        if (adapter->config.device != MOBILE_ADAPTER_BLUE &&
                c != (MOBILE_ADAPTER_GAMEBOY | 0x80) &&
                c != (MOBILE_ADAPTER_GAMEBOY_ADVANCE | 0x80)) {
            s->state = MOBILE_SERIAL_WAITING;
            return 0xD2;  // TODO: What does it _actually_ return?
        }

        if (s->error) {
            s->state = MOBILE_SERIAL_WAITING;
            return s->error;
        }

        if (s->mode_32bit_cur) {
            // We need to add two extra 0 bytes to the transmission
            s->current++;
        } else {
            s->state = MOBILE_SERIAL_RESPONSE_WAITING;
        }
        return s->buffer[0] ^ 0x80;

    case MOBILE_SERIAL_RESPONSE_WAITING:
        // Wait while processing the received packet and crafting a response.
        // TODO: Check for 0x4b, reset otherwise.
        // TODO: Don't actually process the packet for command 0xf
        break;

    case MOBILE_SERIAL_RESPONSE_START:
        // Start sending the response.
        if (s->current++ == 0) {
            return 0x99;
        } else {
            s->data_size = s->buffer[3];
            if (s->mode_32bit_cur && s->data_size % 4 != 0) {
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
            return adapter->config.device | 0x80;
        } else if (s->current == 1) {
            // There's nothing we can do with the received device ID.
            // In fact, the real adapter doesn't care for this value, either.
            s->current++;
            return 0;
        } else if (s->current == 2) {
            // Catch the error
            s->error = c;
        }

        if (s->mode_32bit_cur && s->current < 4) {
            s->current++;
            return 0;
        }

        s->current = 0;
        // If an error happened, retry.
        if (s->error == MOBILE_SERIAL_ERROR_UNKNOWN_COMMAND ||
                s->error == MOBILE_SERIAL_ERROR_CHECKSUM ||
                s->error == MOBILE_SERIAL_ERROR_UNKNOWN) {
            s->state = MOBILE_SERIAL_RESPONSE_START;
            return 0xD2;
        }
        // Start over after this
        s->state = MOBILE_SERIAL_WAITING;
        break;
    }

    return 0xD2;
}
