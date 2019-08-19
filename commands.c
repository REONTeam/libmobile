#include "commands.h"

#include <string.h>

#include "mobile.h"

// A lot of details about the communication protocol are unknown,
//   and would be necessary to complete this implementation properly:
// - What does the adapter respond if sending a start session packet with
//     contents _other_ than "NINTENDO"? Is it always echoed back?
// - What is the effect of calling a number starting with a `#` sign?
//     (Dan Docs mentions it always starts with this character, however, when
//     calling someone with Pokémon Crystal, this character isn't included)
//     It seems to only be included for the 4-digit numbers.
// - What does the adapter respond when starting a session without ending the
//     previous one?
// - Similarly, what does it respond when ending a ession without starting one?
// - What does the adapter respond to packets sent _before_ "beginning" a
//     session? (Try Telephone Status, Read Config Data, Wait For Call,
//     ISP Login, Transfer Data and End Session packets, at least).
// - What does the adapter respond to ISP Login and Dial Telephone when no
//     phone is connected/connection fails?
// - What does the adapter respond to Transfer Data if no connection has been
//     made?
// - Under what circumstances is the line "busy"? Is it only when connecting,
//     or when connected, and does the difference between 0x04 and 0x05 have
//     anything to do with TCP vs Call?
// - What does the adapter respond if we try to read/write beyond the 0xC0
//     configuration size boundary?

__attribute__((unused))
static void make_wait_packet(struct mobile_packet *packet)
{
    // Sent if the mobile adapter is waiting for the phone to respond.
    // This works as a sort of "keep alive" packet.
    enum mobile_command command = packet->command;
    packet->command = MOBILE_COMMAND_WAIT;
    packet->length = 2;
    packet->data[0] = command;
    packet->data[1] = 0;
}

struct mobile_packet *mobile_process_packet(struct mobile_packet *packet)
{
    switch (packet->command) {
    case MOBILE_COMMAND_BEGIN_SESSION:
        // TODO: Actually implement
        return packet;

    case MOBILE_COMMAND_END_SESSION:
        // TODO: Actually implement
        return packet;

    case MOBILE_COMMAND_TELEPHONE_STATUS:
        // TODO: Actually implement
        packet->length = 1;
        packet->data[0] = 0x00;
        return packet;

    case MOBILE_COMMAND_READ_CONFIGURATION_DATA: {
        unsigned offset = packet->data[0];
        unsigned size = packet->data[1];
        packet->data[0] = offset;
        packet->length = size + 1;
        // Make sure we don't read beyond the boundaries
        if (offset >= MOBILE_CONFIG_DATA_SIZE) {
            memset(packet->data + 1, 0xFF, size);
        } else if (offset + size > MOBILE_CONFIG_DATA_SIZE) {
            unsigned fitting = MOBILE_CONFIG_DATA_SIZE - offset;
            mobile_board_config_read(packet->data + 1, offset, fitting);
            memset(packet->data + 1 + fitting, 0xFF, size - fitting);
        } else {
            mobile_board_config_read(packet->data + 1, offset, size);
        }
        return packet;
    }

    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
        // Make sure we don't write beyond the boundaries
        if (packet->data[0] >= MOBILE_CONFIG_DATA_SIZE) {
            packet->length = 0;
            return packet;
        } else if (packet->data[0] + packet->length > MOBILE_CONFIG_DATA_SIZE) {
            packet->length = MOBILE_CONFIG_DATA_SIZE - packet->data[0];
        }
        mobile_board_config_write(packet->data + 1, packet->data[0], packet->length);
        packet->length = 0;
        return packet;

    case MOBILE_COMMAND_DIAL_TELEPHONE:
        // TODO: Actually implement
        packet->length = 0;
        return packet;

    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
        // TODO: Actually implement
        packet->length = 0;
        //make_wait_packet(packet);
        return packet;

    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
        // TODO: Actually implement
        return packet;

    case MOBILE_COMMAND_TRANSFER_DATA:
        // TODO: Actually implement
        return packet;

    default:
        // Just echo the same thing back
        return packet;
    }
}
