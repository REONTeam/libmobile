// This file provides an example implementation of mobile_board_debug_cmd
//   that you can conditionally compile.

// TODO: On arduino, move the strings to progmem.

static void dump_hex(const unsigned char *buf, const unsigned len)
{
    for (unsigned i = 0; i < len; i += 0x10) {
        printf("\r\n    ");
        for (unsigned x = i; x < i + 0x10 && x < len; x++)  {
            printf("%02X ", buf[x]);
        }
    }
}

static void dump(const unsigned char *buf, const unsigned len)
{
    if (!len) return;

    unsigned i;
    for (i = 0; i < len; i++) {
        if (buf[i] >= 0x80) break;
        if (buf[i] < 0x20 &&
                buf[i] != '\r' &&
                buf[i] != '\n') {
            break;
        }
    }
    if (i < len) return dump_hex(buf, len);

    printf("\n");
    for (unsigned i = 0; i < len; i++) printf("%c", buf[i]);
}

static void packet_end(const struct mobile_packet *packet, unsigned length)
{
    if (packet->length > length) {
        printf(" !!parsing failed!!");
        dump_hex(packet->data + length, packet->length - length);
    }
}

void mobile_board_debug_cmd(
#ifdef __GNUC__
        __attribute__((unused))
#endif
        void *user, const int send, const struct mobile_packet *packet)
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
        packet_end(packet, 0);
        if (send) printf("\r\n");
        break;

    case MOBILE_COMMAND_DIAL_TELEPHONE:
        printf("Dial telephone");
        if (!send) {
            if (packet->length < 2) {
                packet_end(packet, 0);
                break;
            }
            printf(" (unkn %02X): ", packet->data[0]);
            for (unsigned i = 0; i < packet->length; i++) {
                printf("%c", packet->data[i]);
            }
        } else {
            packet_end(packet, 0);
        }
        break;

    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
        printf("Hang up telephone");
        packet_end(packet, 0);
        break;

    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
        printf("Wait for telephone call");
        packet_end(packet, 0);
        break;

    case MOBILE_COMMAND_TRANSFER_DATA:
        printf("Transfer data");
        if (packet->length < 1) break;

        if (packet->data[0] == 0xFF) {
            printf(" (p2p)");
        } else {
            printf(" (conn %u)", packet->data[0]);
        }
        dump(packet->data + 1, packet->length - 1);
        break;

    case MOBILE_COMMAND_TELEPHONE_STATUS:
        printf("Telephone status");
        if (!send) {
            packet_end(packet, 0);
        } else {
            if (packet->length < 3) {
                packet_end(packet, 0);
                break;
            }
            printf(": %02X %02X %02X",
                    packet->data[0], packet->data[1], packet->data[2]);
            packet_end(packet, 3);
        }
        break;

    case MOBILE_COMMAND_SIO32_MODE:
        printf("Serial 32-bit mode");
        if (!send) {
            if (packet->length < 1) break;
            printf(packet->data[0] != 0 ? ": On" : ": Off");
            packet_end(packet, 1);
        } else {
            packet_end(packet, 0);
        }
        break;

    case MOBILE_COMMAND_READ_CONFIGURATION_DATA:
        printf("Read configuration data");
        if (!send) {
            if (packet->length < 2) {
                packet_end(packet, 0);
                break;
            }
            printf(" (offset: %02X; size: %02X)", packet->data[0], packet->data[1]);
            packet_end(packet, 2);
        } else {
            if (packet->length < 1) break;
            printf(" (offset: %02X)", packet->data[0]);
            dump_hex(packet->data + 1, packet->length - 1);
        }
        break;

    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
        printf("Write configuration data");
        if (!send) {
            if (packet->length < 1) break;
            printf(" (offset: %02X; size: %02X)", packet->data[0], packet->length - 1);
            dump_hex(packet->data + 1, packet->length - 1);
        } else {
            packet_end(packet, 0);
        }
        break;

    case MOBILE_COMMAND_TRANSFER_DATA_END:
        printf("Transfer data end");
        if (packet->length < 1) break;
        printf(" (conn %u)", packet->data[0]);
        packet_end(packet, 1);
        break;

    case MOBILE_COMMAND_ISP_LOGIN:
        printf("ISP login");
        if (!send) {
            if (packet->length < 1) break;

            const unsigned char *data = packet->data;
            if (packet->data + packet->length < data + 1 + data[0]) {
                packet_end(packet, data - packet->data);
                break;
            }
            printf(" (id: ");
            for (unsigned i = 0; i < data[0]; i++) printf("%c", data[i + 1]);
            data += 1 + data[0];

            if (packet->data + packet->length < data + 1 + data[0] + 8) {
                printf(")");
                packet_end(packet, data - packet->data);
                break;
            }
            printf("; pass: ");
            for (unsigned i = 0; i < data[0]; i++) printf("*");  // Censor pass
            data += 1 + data[0];

            printf("; dns1: %u.%u.%u.%u; dns2: %u.%u.%u.%u)",
                    data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);
            data += 8;
            packet_end(packet, data - packet->data);
        } else {
            if (packet->length < 4) {
                packet_end(packet, 0);
                break;
            }
            printf(" (ip: %u.%u.%u.%u)",
                    packet->data[0], packet->data[1],
                    packet->data[2], packet->data[3]);
            packet_end(packet, 4);
        }
        break;

    case MOBILE_COMMAND_ISP_LOGOUT:
        printf("ISP logout");
        packet_end(packet, 0);
        break;

    case MOBILE_COMMAND_OPEN_TCP_CONNECTION:
        printf("Open TCP connection");
        if (!send) {
            if (packet->length < 6) {
                packet_end(packet, 0);
                break;
            }
            printf(": %u.%u.%u.%u:%u",
                    packet->data[0], packet->data[1],
                    packet->data[2], packet->data[3],
                    packet->data[4] << 8 | packet->data[5]);
            packet_end(packet, 6);
        } else {
            if (packet->length < 1) break;
            printf(" (conn %u)", packet->data[0]);
            packet_end(packet, 1);
        }
        break;

    case MOBILE_COMMAND_CLOSE_TCP_CONNECTION:
        printf("Close TCP connection");
        if (packet->length < 1) break;
        printf(" (conn %u)", packet->data[0]);
        packet_end(packet, 1);
        break;

    case MOBILE_COMMAND_DNS_QUERY:
        printf("DNS query");
        if (!send) {
            printf(": ");
            for (unsigned i = 0; i < packet->length; i++) {
                printf("%c", packet->data[i]);
            }
        } else {
            if (packet->length < 4) {
                packet_end(packet, 0);
                break;
            }
            printf(": %u.%u.%u.%u",
                    packet->data[0], packet->data[1],
                    packet->data[2], packet->data[3]);
            packet_end(packet, 4);
        }
        break;

    case MOBILE_COMMAND_ERROR:
        printf("Error");
        if (packet->length < 2) {
            packet_end(packet, 0);
            break;
        }
        printf(": %02X", packet->data[1]);
        packet_end(packet, 2);
        break;

    default:
        printf("Unknown");
        dump_hex(packet->data, packet->length);
    }

    printf("\r\n");
}
