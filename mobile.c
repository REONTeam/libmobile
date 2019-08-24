#include "mobile.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "commands.h"

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

// This is used to indicate whether we want to listen for commands other than
//   MOBILE_COMMAND_BEGIN_SESSION. Its state is modified in commands.c
bool mobile_session_begun;

static const enum mobile_adapter adapter = MOBILE_ADAPTER_BLUE;

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
static volatile unsigned index;
static unsigned char buffer[4 + MOBILE_MAX_DATA_LENGTH + 2];

unsigned char mobile_transfer(unsigned char c)
{
    static unsigned data_length;
    static uint16_t checksum;
    static enum mobile_error error;
    static int send_retry;

    mobile_board_time_latch();

    switch (state) {
    case STATE_WAITING:
        if (c == 0x99) {
            index = 1;
        } else if (c == 0x66 && index == 1) {
            index = 0;
            data_length = 0;
            checksum = 0;
            error = 0;
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
            if (!mobile_session_begun && buffer[0] != MOBILE_COMMAND_BEGIN_SESSION) {
                index = 0;
                state = STATE_WAITING;
            }
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
        index = 0;
        state = STATE_WAITING;
        if (error) return error;
        if (c != 0x80) return MOBILE_ERROR_UNKNOWN;

        send_retry = 0;
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
    for (unsigned i = 0; i < packet->length + 4; i++) {
        checksum += buffer[i];
    }
    buffer[packet->length + 4] = (checksum >> 8) & 0xFF;
    buffer[packet->length + 5] = checksum & 0xFF;
}

void mobile_loop(void)
{
    static struct mobile_packet packet;

    if (state == STATE_RESPONSE_WAITING) {
        parse_packet(&packet, buffer);
        mobile_board_debug_cmd(0, &packet);

        struct mobile_packet *send = mobile_process_packet(&packet);
        mobile_board_debug_cmd(1, send);
        create_packet(buffer, send);

        state = STATE_RESPONSE_START;
    } else if ((state != STATE_WAITING && mobile_board_time_check_ms(500)) ||
            (mobile_session_begun && mobile_board_time_check_ms(2000))) {
        // If the adapter is stuck waiting, with no signal from the game,
        //   put it out of its misery.
        mobile_board_disable_spi();
        mobile_session_begun = false;
        index = 0;
        state = STATE_WAITING;
        mobile_board_enable_spi();

        // Notify the debugger somehow.
        packet.command = MOBILE_COMMAND_END_SESSION;
        packet.length = 0;
        mobile_board_debug_cmd(1, &packet);
    } else if (state == STATE_WAITING && !mobile_session_begun &&
            mobile_board_time_check_ms(500)) {
        mobile_board_disable_spi();
        mobile_board_time_latch();
        mobile_board_enable_spi();
    }
}

#if MOBILE_CONFIG_DATA_SIZE >= MOBILE_MAX_DATA_LENGTH
#error
#endif
static void config_clear(void)
{
    memset(buffer, 0, MOBILE_CONFIG_DATA_SIZE);
    mobile_board_config_write(buffer, 0, MOBILE_CONFIG_DATA_SIZE);
}

static bool config_verify(void)
{
    mobile_board_config_read(buffer, 0, MOBILE_CONFIG_DATA_SIZE);
    if (buffer[0] != 'M' || buffer[1] != 'A' || buffer[2] != 0x81) return false;

    uint16_t checksum = 0;
    for (unsigned i = 0; i < MOBILE_CONFIG_DATA_SIZE - 2; i++) {
        checksum += buffer[i];
    }
    uint16_t config_checksum = buffer[MOBILE_CONFIG_DATA_SIZE - 2] << 8 |
                               buffer[MOBILE_CONFIG_DATA_SIZE - 1]; 
    return checksum == config_checksum;
}

void mobile_init(void)
{
    if (!config_verify()) config_clear();

    mobile_board_disable_spi();
    mobile_session_begun = false;
    index = 0;
    state = STATE_WAITING;
    mobile_board_enable_spi();
}
