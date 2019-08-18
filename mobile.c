#include "mobile.h"

#include "commands.h"

#include <stdint.h>
#include <string.h>

enum mobile_adapter {
    MOBILE_ADAPTER_BLUE = 0x88,
    MOBILE_ADAPTER_YELLOW,
    MOBILE_ADAPTER_GREEN,
    MOBILE_ADAPTER_RED
};

enum mobile_error {
    MOBILE_ERROR_UNKNOWN = 0xF0,
    MOBILE_ERROR_CHECKSUM,
};

static volatile enum {
    STATE_WAITING,
    STATE_DATA,
    STATE_CHECKSUM,
    STATE_ACKNOWLEDGE,
    STATE_RESPONSE_WAITING,
    STATE_RESPONSE_START,
    STATE_RESPONSE_DATA,
    STATE_RESPONSE_ACKNOWLEDGE
} state;

static enum mobile_adapter adapter = MOBILE_ADAPTER_BLUE;

static unsigned char buffer[4 + MOBILE_MAX_DATA_LENGTH + 2];
static unsigned index;
static unsigned data_length;

static uint16_t checksum;
static enum mobile_error error;
static int send_retry;

unsigned char mobile_transfer(unsigned char c)
{
    switch (state) {
    case STATE_WAITING:
        if (c == 0x99) {
            index = 1;
        } else if (c == 0x66 && index == 1) {
            index = 0;
            checksum = 0;
            state = STATE_DATA;
        } else {
            index = 0;
        }
        break;

    case STATE_DATA:
        buffer[index++] = c;
        checksum += c;
        if (index == 4) {
            data_length = buffer[3];
        } else if (index >= data_length + 4) {
            state = STATE_CHECKSUM;
        }
        break;

    case STATE_CHECKSUM:
        buffer[index++] = c;
        if (index >= data_length + 6) {
            uint16_t in_checksum = buffer[index - 2] << 8 | buffer[index - 1];
            if (checksum != in_checksum) error = MOBILE_ERROR_CHECKSUM;
            state = STATE_ACKNOWLEDGE;
            return adapter;
        }
        break;

    case STATE_ACKNOWLEDGE:
        if (error) {
            c = error;
            error = 0;
            index = 0;
            state = STATE_WAITING;
            return c;
        }
        if (c != 0x80) {
            index = 0;
            state = STATE_WAITING;
            return MOBILE_ERROR_UNKNOWN;
        }

        send_retry = 0;
        index = 0;
        state = STATE_RESPONSE_WAITING;
        return buffer[0] ^ 0x80;

    case STATE_RESPONSE_WAITING:
        // Don't do anything
        break;

    case STATE_RESPONSE_START:
        if (index++ == 0) {
            return 0x99;
        } else {
            data_length = buffer[3];
            index = 0;
            state = STATE_RESPONSE_DATA;
            return 0x66;
        }

    case STATE_RESPONSE_DATA:
        c = buffer[index++];
        if (index > data_length + 6) {
            index = 0;
            state = STATE_RESPONSE_ACKNOWLEDGE;
            return adapter;
        }
        return c;

    case STATE_RESPONSE_ACKNOWLEDGE:
        if (index++ == 0) {
            return 0;
        } else {
            // TODO: Actually parse the error code.
            if ((c ^ 0x80) != buffer[0]) {
                if (++send_retry < 4) {
                    index = 1;
                    state = STATE_RESPONSE_START;
                    return 0x99;
                }
            }

            index = 0;
            state = STATE_WAITING;
        }
    }

    return 0xD2;
}

static void parse_packet(struct mobile_packet *packet, const unsigned char *buffer)
{
    packet->command = buffer[0];
    packet->length = buffer[3];
    memcpy(packet->data, buffer + 4, packet->length);
}

static void create_packet(unsigned char *buffer, const struct mobile_packet *packet)
{
    buffer[0] = packet->command ^ 0x80;
    buffer[1] = 0;
    buffer[2] = 0;
    buffer[3] = packet->length;
    memcpy(buffer + 4, packet->data, packet->length);

    uint16_t checksum = 0;
    for (int i = 0; i < packet->length + 4; i++) {
        checksum += buffer[i];
    }
    buffer[packet->length + 4] = (checksum >> 8) & 0xFF;
    buffer[packet->length + 5] = checksum & 0xFF;
}

void mobile_loop(void)
{
    if (state != STATE_RESPONSE_WAITING) return;

    struct mobile_packet receive;
    parse_packet(&receive, buffer);

    struct mobile_packet *send = mobile_process_packet(&receive);
    create_packet(buffer, send);

    state = STATE_RESPONSE_START;
}

void mobile_init(void)
{
    mobile_board_reset_spi();
    error = 0;
    index = 0;
    state = STATE_WAITING;
}
