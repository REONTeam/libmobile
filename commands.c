#include "commands.h"

#include "mobile.h"

struct mobile_packet *mobile_process_packet(struct mobile_packet *packet)
{
    switch (packet->command) {
    case MOBILE_COMMAND_BEGIN_SESSION:
    case MOBILE_COMMAND_END_SESSION:
    default:
        // Just echo the same thing back
        break;

    case MOBILE_COMMAND_TELEPHONE_STATUS:
        packet->length = 1;
        packet->data[0] = 0x00;  // TODO: 0x05 if busy
    }

    return packet;
}
