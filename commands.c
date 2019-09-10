#include "commands.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "mobile.h"

#define MOBILE_P2P_PORT "2415"

extern bool mobile_session_begun;

// A bunch of details about the communication protocol are unknown,
//   and would be necessary to complete this implementation properly:
// - What is the effect of calling a number starting with a `#` sign?
//     (Dan Docs mentions it always starts with this character, however, when
//     calling someone with Pokémon Crystal, this character isn't included)
//     It seems to only be included for the 4-digit number '#9677'.
// - Under what circumstances is the line "busy"? Is it only when connecting,
//     or when connected, and does the difference between 0x04 and 0x05 have
//     anything to do with TCP vs Call?
// - What happens when calling ISP logout without being logged in?
//     What about hang up telephone and close tcp connection?
// - Before beginning a session, what does the adapter respond when being sent
//     a 99 66 99 66 10 ..., what about 99 66 17 99 66 10 ...?
//     (I want to know at which point the adapter starts rejecting a packet
//     that doesn't begin the session, when the session hasn't been begun)
// - What happens when reading/writing configuration data with a size of 0?
//     What if the requested address is outside of the config area?
// - Does the session "end" when an error is returned from, for example,
//     Dial Telephone? Try sending a session begin packet after such an error.

// UNKERR is used for errors of which we don't really know if they exist, and
//   if so what error code they return, but have been implemented just in case.
// NEWERR is used to indicate an error code that we made up ourselves to
//   indicate something that couldn't happen with the real adapter.

static enum {
    CONNECTION_DISCONNECTED,
    CONNECTION_LISTENING,
    CONNECTION_CONNECTING,
    CONNECTION_CONNECTED
} connection = CONNECTION_DISCONNECTED;
static unsigned connection_sent = 0;  // Amount of messages sent in synchronized mode.

static struct mobile_packet *error_packet(struct mobile_packet *packet, unsigned char error)
{
    enum mobile_command command = packet->command;
    packet->command = MOBILE_COMMAND_ERROR;
    packet->length = 2;
    packet->data[0] = command;
    packet->data[1] = error;
    return packet;
}

static bool parse_address(char *address, char *data)
{
    // Converts a 0-padded string of 12 characters to a '.'-separated IP address.
    // It also checks for the validity of the address while doing so.
    // The output string will be at most 17 bytes long including the terminator.

    char *cur_data = data;
    char *cur_addr = address;
    for (unsigned y = 0; y < 4; y++) {
        if (y) *cur_addr++ = '.';
        bool copy = false;
        unsigned cur_num = 0;
        for (unsigned x = 0; x < 3; x++) {
            if (*cur_data < '0' || *cur_data > '9') return false;
            if (!copy && *cur_data != '0') copy = true;
            if (copy) *cur_addr++ = *cur_data;
            cur_num *= 10;
            cur_num += *cur_data++ - '0';
        }
        if (cur_num > 255) return false;
        if (cur_num == 0) *cur_addr++ = '0';
    }
    *cur_addr = '\0';
    return true;
}

static bool transfer_data(unsigned char *data, unsigned *size)
{
    // Pokémon Crystal expects communications to be "synchronized".
    // For this, we only try to receive packets when we've sent one.
    // Maybe the first byte in the packet has something to do with it?
    // TODO: Check other games with peer to peer functionality.

    int recv_size = 0;
    if (data[0] == 0xFF) {
        // Synchronized mode
        if (!mobile_board_tcp_send(data + 1, *size - 1)) return false;
        if (*size > 1) connection_sent++;
        if (connection_sent) {
            recv_size = mobile_board_tcp_receive(data + 1);
            if (recv_size != 0) connection_sent--;
        }
    } else {
        // Normal mode
        if (!mobile_board_tcp_send(data + 1, *size - 1)) return false;
        recv_size = mobile_board_tcp_receive(data + 1);
    }
    if (recv_size == -10) return true;  // Allow echoing the packet (weak_defs.c)
    if (recv_size < 0) return false;

    // Echoing data[0]...
    *size = (unsigned)recv_size + 1;
    return true;
}

struct mobile_packet *mobile_process_packet(struct mobile_packet *packet)
{
    switch (packet->command) {
    case MOBILE_COMMAND_BEGIN_SESSION:
        // Errors:
        // 0 - ???
        // 1 - Invalid contents
        // 2 - Invalid use (Already begun a session)

        if (mobile_session_begun) return error_packet(packet, 2);
        if (packet->length != 8) return error_packet(packet, 1);
        if (memcmp(packet->data, "NINTENDO", 8) != 0) {
            return error_packet(packet, 1);
        }
        mobile_session_begun = true;
        connection = CONNECTION_DISCONNECTED;
        return packet;

    case MOBILE_COMMAND_END_SESSION:
        // TODO: What happens if the packet has a body? Probably nothing.
        mobile_session_begun = false;
        if (connection != CONNECTION_DISCONNECTED) {
            mobile_board_tcp_disconnect();
            connection = CONNECTION_DISCONNECTED;
        }
        return packet;

    case MOBILE_COMMAND_DIAL_TELEPHONE:
        // Errors:
        // 0 - (from crystal) Telephone line is busy
        // 1 - NEWERR: Invalid contents
        // 2 - (from crystal) Communication error
        // 3 - Phone is not connected

        // TODO: Should we even interpret the number here at all,
        //     or should we let the implementation handle it?
        //   That way the implementation can decide what port to use, where
        //     to connect to, etc.

        if (connection != CONNECTION_DISCONNECTED &&
                connection != CONNECTION_CONNECTING) {
            return error_packet(packet, 2);  // UNKERR
        }

        // Ignore the default phone numbers for now
        if (packet->length == 6 && memcmp(packet->data + 1, "#9677", 5) == 0) {
            //connection = CONNECTION_CONNECTED;
            //connection_sent = 0;
            packet->length = 0;
            return packet;
        }
        if (packet->length == 11 && memcmp(packet->data + 1, "0077487751", 10) == 0) {
            //connection = CONNECTION_CONNECTED;
            //connection_sent = 0;
            packet->length = 0;
            return packet;
        }

        // Interpret the number as an IP and connect to someone.
        if (packet->length == 3 * 4 + 1) {
            char address[17];
            if (!parse_address(address, (char *)packet->data + 1)) {
                return error_packet(packet, 1);  // NEWERR
            }
            if (mobile_board_tcp_connect(address, MOBILE_P2P_PORT)) {
                connection = CONNECTION_CONNECTED;
                connection_sent = 0;
                packet->length = 0;
                return packet;
            }
        }

        // TODO: What error is returned when the phone is connected but
        //       can't reach anything?
        // TODO: What happens if the packet has no body? Probably nothing.
        return error_packet(packet, 3);

    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
        // TODO: Actually implement
        //return error_packet(packet, 0);  // UNKERR
        // TODO: What happens if the packet has a body? Probably nothing.
        // TODO: What happens if hanging up a non-existing connection?
        if (connection != CONNECTION_DISCONNECTED) {
            mobile_board_tcp_disconnect();
            connection = CONNECTION_DISCONNECTED;
        }
        return packet;

    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
        // Errors:
        // 0 - Phone is not connected / No call received
        // 1 - ???
        // 2 - NEWERR: Invalid use (already connecting/connected)

        // TODO: What happens if the packet has a body? Probably nothing.

        if (connection != CONNECTION_DISCONNECTED &&
                connection != CONNECTION_LISTENING) {
            return error_packet(packet, 2);  // NEWERR
        }

        if (mobile_board_tcp_listen(MOBILE_P2P_PORT)) {
            connection = CONNECTION_CONNECTED;
            connection_sent = 0;
            return packet;
        }
        connection = CONNECTION_LISTENING;
        return error_packet(packet, 0);

    case MOBILE_COMMAND_TRANSFER_DATA:
        // Errors:
        // 0 - ???
        // 1 - Invalid use (Call was ended/never made)
        // 2 - UNKERR: Invalid contents

        if (packet->length < 1) return error_packet(packet, 2);  // UNKERR
        if (connection != CONNECTION_CONNECTED) return error_packet(packet, 1);

        if (!transfer_data(packet->data, &packet->length)) {
            mobile_board_tcp_disconnect();
            connection = CONNECTION_DISCONNECTED;
            return error_packet(packet, 1);
        }
        return packet;

    case MOBILE_COMMAND_TELEPHONE_STATUS:
        // TODO: Actually implement
        packet->length = 3;
        packet->data[0] = 0x00;  // 0xFF if phone is disconnected
        packet->data[1] = 0x4D;  // Blue adapter (0x48 for yellow, others unknown)
        packet->data[2] = 0x00;  // 0xF0 signals Crystal to bypass time limits.
        return packet;

    case MOBILE_COMMAND_READ_CONFIGURATION_DATA: {
        // Errors:
        // 0 - NEWERR: Internal error (Failed to read config)
        // 1 - UNKERR: Invalid contents
        // 2 - Invalid use (Tried to read outside of configuration area)

        if (packet->length != 2) return error_packet(packet, 1);  // UNKERR
        unsigned offset = packet->data[0];
        unsigned size = packet->data[1];
        if (offset + size > MOBILE_CONFIG_DATA_SIZE) {
            return error_packet(packet, 2);
        }
        packet->length = size + 1;
        packet->data[0] = offset;
        if (size) {
            if (!mobile_board_config_read(packet->data + 1, offset, size)) {
                return error_packet(packet, 0);  // NEWERR
            }
        }
        return packet;
    }

    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
        // Errors:
        // 0 - NEWERR: Internal error (Failed to write config)
        // 1 - UNKERR: Invalid contents
        // 2 - Invalid use (Tried to write outside of configuration area)

        if (packet->length < 2) return error_packet(packet, 1);  // UNKERR
        if (packet->data[0] + packet->length - 1 > MOBILE_CONFIG_DATA_SIZE) {
            return error_packet(packet, 2);
        }
        if (!mobile_board_config_write(packet->data + 1, packet->data[0],
                    packet->length - 1)) {
            return error_packet(packet, 0);  // NEWERR
        }
        packet->length = 0;
        return packet;

    case MOBILE_COMMAND_ISP_LOGIN:
        // TODO: Actually implement
        return error_packet(packet, 1);  // No phone is connected

    case MOBILE_COMMAND_ISP_LOGOUT:
        // TODO: Actually implement
        return error_packet(packet, 0);  // UNKERR

    case MOBILE_COMMAND_OPEN_TCP_CONNECTION:
        // TODO: Actually implement
        return error_packet(packet, 0);  // UNKERR

    case MOBILE_COMMAND_DNS_QUERY:
        // TODO: Actually implement
        return error_packet(packet, 0);  // UNKERR

    default:
        // Just echo the same thing back
        return packet;
    }
}
