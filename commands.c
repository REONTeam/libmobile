#include "commands.h"

#include "mobile.h"

__attribute__((unused))
static void make_wait_packet(struct mobile_packet *packet)
{
    // Causes the game to wait indefinitely if sent regularly enough.
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
        int offset = packet->data[0];
        int size = packet->data[1];
        packet->length = size + 1;
        packet->data[0] = offset;
        mobile_board_config_read(packet->data + 1, offset, size);
        return packet;
    }

    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
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
