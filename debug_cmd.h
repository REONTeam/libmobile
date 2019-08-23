// This file provides an example implementation of mobile_board_debug_cmd
//   that you can conditionally compile.

static void hex_dump(const unsigned char *buf, const unsigned len)
{
    for (unsigned i = 0; i < len; i += 0x10) {
        printf("\r\n    ");
        for (unsigned x = i; x < i + 0x10 && x < len; x++)  {
            printf("%02X ", buf[x]);
        }
    }
}

void mobile_board_debug_cmd(const int send, const struct mobile_packet *packet)
{
    if (!send) printf(">>> ");
    else printf("<<< ");

    printf("%02X ", packet->command);
    switch(packet->command) {
    case MOBILE_COMMAND_BEGIN_SESSION:
        printf("Begin session: ");
        for (unsigned i = 0; i < packet->length; i++){
            printf("%c", packet->data[i]);
        }
        break;

    case MOBILE_COMMAND_END_SESSION:
        printf("End session");
        if (send) printf("\r\n");
        break;

    case MOBILE_COMMAND_TELEPHONE_STATUS:
        printf("Telephone status");
        if (send && packet->length >= 1) printf(": %02X", packet->data[0]);
        break;

    case MOBILE_COMMAND_READ_CONFIGURATION_DATA:
        printf("Read configuration data");
        if (!send && packet->length >= 2) {
            printf(" (offset: %02X; size: %02X)", packet->data[0], packet->data[1]);
        } else if (packet->length >= 1) {
            hex_dump(packet->data + 1, packet->length - 1);
        }
        break;

    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
        printf("Write configuration data");
        if (!send && packet->length >= 1) {
            printf("(offset: %02X; size: %02X)", packet->data[0], packet->length - 1);
            hex_dump(packet->data + 1, packet->length - 1);
        }
        break;

    case MOBILE_COMMAND_DIAL_TELEPHONE:
        printf("Dial telephone");
        if (!send && packet->length >= 1) {
            printf(" (unkn %02X)", packet->data[0]);
        }
        if (!send && packet->length >= 2) {
            printf(" #");
            unsigned i = 1;
            while (packet->data[i] == '#') i++;
            for (; i < packet->length; i++) {
                printf("%c", packet->data[i]);
            }
        }
        break;

    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
        printf("Wait for telephone call");
        break;

    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
        printf("Hang up telephone");
        break;

    case MOBILE_COMMAND_TRANSFER_DATA:
        printf("Transfer data");
        hex_dump(packet->data, packet->length);
        break;

    case MOBILE_COMMAND_ERROR:
        printf("Error");
        if (packet->length >= 2) printf(" %02X", packet->data[1]);
        break;

    default:
        printf("Unknown");
        hex_dump(packet->data, packet->length);
    }

    printf("\r\n");
}
