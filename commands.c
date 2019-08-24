#include "commands.h"

#include <stdbool.h>
#include <string.h>

#include "mobile.h"

extern bool mobile_session_begun;

// A bunch of details about the communication protocol are unknown,
//   and would be necessary to complete this implementation properly:
// - What is the effect of calling a number starting with a `#` sign?
//     (Dan Docs mentions it always starts with this character, however, when
//     calling someone with Pokémon Crystal, this character isn't included)
//     It seems to only be included for the 4-digit numbers.
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

// UNKERR is used to indicate calls to error_packet for which I'm unsure What
//   the proper error value/behavior is.

static struct mobile_packet *error_packet(struct mobile_packet *packet, unsigned char error)
{
    enum mobile_command command = packet->command;
    packet->command = MOBILE_COMMAND_ERROR;
    packet->length = 2;
    packet->data[0] = command;
    packet->data[1] = error;
    return packet;
}

struct mobile_packet *mobile_process_packet(struct mobile_packet *packet)
{
    switch (packet->command) {
    case MOBILE_COMMAND_BEGIN_SESSION:
        if (mobile_session_begun) return error_packet(packet, 2);
        if (packet->length != 8) return error_packet(packet, 1);
        if (memcmp(packet->data, "NINTENDO", 8) != 0) {
            return error_packet(packet, 1);
        }
        mobile_session_begun = true;
        return packet;

    case MOBILE_COMMAND_END_SESSION:
        mobile_session_begun = false;
        return packet;

    case MOBILE_COMMAND_DIAL_TELEPHONE:
        // TODO: Actually implement
        return error_packet(packet, 3);  // No phone is connected
        //packet->length = 0;
        //return packet;

    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
        // TODO: Actually implement
        return error_packet(packet, 0);  // UNKERR
        //return packet;

    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
        // TODO: Actually implement
        return error_packet(packet, 0);  // No phone is connected
        //return packet;

    case MOBILE_COMMAND_TRANSFER_DATA:
        // TODO: Actually implement
        return error_packet(packet, 1);  // No transfer has been started
        //return packet;

    case MOBILE_COMMAND_TELEPHONE_STATUS:
        // TODO: Actually implement
        packet->length = 3;
        packet->data[0] = 0x00;  // 0xFF if phone is disconnected
        packet->data[1] = 0x4D;
        packet->data[2] = 0x00;
        return packet;

    case MOBILE_COMMAND_READ_CONFIGURATION_DATA: {
        if (packet->length != 2) return error_packet(packet, 0);  // UNKERR
        unsigned offset = packet->data[0];
        unsigned size = packet->data[1];
        if (offset + size > MOBILE_CONFIG_DATA_SIZE) {
            return error_packet(packet, 2);
        }
        packet->length = size + 1;
        packet->data[0] = offset;
        if (size) mobile_board_config_read(packet->data + 1, offset, size);
        return packet;
    }

    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
        if (packet->data[0] + packet->length > MOBILE_CONFIG_DATA_SIZE) {
            return error_packet(packet, 2);
        }
        if (packet->length < 2) return error_packet(packet, 1);  // UNKERR

        mobile_board_config_write(packet->data + 1, packet->data[0],
                                  packet->length - 1);
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
