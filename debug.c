// SPDX-License-Identifier: LGPL-3.0-or-later
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mobile_data.h"
#include "compat.h"
#include "callback.h"

void mobile_debug_init(struct mobile_adapter *adapter)
{
    adapter->debug.current = 0;
}

#define debug_write(data, size) mobile_debug_write(adapter, (const char *)data, size)
#define debug_print(fmt, ...) mobile_debug_print(adapter, PSTR(fmt), ##__VA_ARGS__)
#define debug_endl() mobile_debug_endl(adapter)

void mobile_debug_write(struct mobile_adapter *adapter, const char *data, size_t size)
{
    struct mobile_adapter_debug *s = &adapter->debug;

    int remaining = MOBILE_DEBUG_BUFFER_SIZE - s->current;
    if (remaining <= 1) return;
    int written = (int)size;
    if (written > remaining - 1) written = remaining - 1;
    memcpy(s->buffer + s->current, data, written);
    s->buffer[s->current + written] = 0;
    s->current += written;
}

void mobile_debug_print(struct mobile_adapter *adapter, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    struct mobile_adapter_debug *s = &adapter->debug;

    int remaining = MOBILE_DEBUG_BUFFER_SIZE - s->current;
    if (remaining <= 1) return;
    int written = vsnprintf_P(s->buffer + s->current, remaining, fmt, ap);
    if (written <= 0) return;
    s->current += written;

    // Remove the terminator 0 from the s->current size
    if (s->current >= MOBILE_DEBUG_BUFFER_SIZE) {
        s->current = MOBILE_DEBUG_BUFFER_SIZE - 1;
    }
}

void mobile_debug_print_hex(struct mobile_adapter *adapter, const void *data, size_t size)
{
    const unsigned char *d = data;
    while (size--) debug_print("%02X ", *d++);
}

void mobile_debug_print_addr(struct mobile_adapter *adapter, const struct mobile_addr *addr)
{
    unsigned port;
    if (addr->type == MOBILE_ADDRTYPE_IPV4) {
        const struct mobile_addr4 *addr4 = (struct mobile_addr4 *)addr;
        debug_print("%u.%u.%u.%u",
            addr4->host[0], addr4->host[1], addr4->host[2], addr4->host[3]);
        port = addr4->port;
    } else if (addr->type == MOBILE_ADDRTYPE_IPV6) {
        const struct mobile_addr6 *addr6 = (struct mobile_addr6 *)addr;
        // Stubbed, IPv6 addresses can get huge!
        debug_print("<IPv6 address>");
        port = addr6->port;
    } else {
        return;
    }
    debug_print(":%u", port);
}

void mobile_debug_endl(struct mobile_adapter *adapter)
{
    struct mobile_adapter_debug *s = &adapter->debug;

    // Write the current line out
    mobile_cb_debug_log(adapter, s->current ? s->buffer : "");
    s->current = 0;
}

static void dump_hex(struct mobile_adapter *adapter, const unsigned char *buf, size_t len)
{
    debug_endl();
    for (unsigned i = 0; i < len; i += 0x10) {
        debug_print("    ");
        size_t x = i + 0x10 > len ? len - i : 0x10;
        mobile_debug_print_hex(adapter, buf + i, x);
        debug_endl();
    }
}

static void dump(struct mobile_adapter *adapter, const unsigned char *buf, size_t len)
{
    if (!len) {
        debug_endl();
        return;
    }

    // If not everything is ASCII, dump hex instead
    unsigned i;
    for (i = 0; i < len; i++) {
        if (buf[i] >= 0x80) break;
        if (buf[i] < 0x20 &&
                buf[i] != '\r' &&
                buf[i] != '\n') {
            break;
        }
    }
    if (i < len) {
        dump_hex(adapter, buf, len);
        return;
    }

    debug_endl();
    debug_write(buf, len);
    debug_endl();
}

static void packet_end(struct mobile_adapter *adapter, const struct mobile_packet *packet, size_t length)
{
    if (packet->length > length) {
        debug_print(" !!parsing failed!!");
        dump_hex(adapter, packet->data + length, packet->length - length);
    } else {
        debug_endl();
    }
}

void mobile_debug_command(struct mobile_adapter *adapter, const struct mobile_packet *packet, bool send)
{
    if (!send) debug_print(">>> ");
    else debug_print("<<< ");

    debug_print("%02X ", packet->command);

    switch(packet->command) {
    case MOBILE_COMMAND_BEGIN_SESSION:
        debug_print("Begin session: ");
        debug_write(packet->data, packet->length);
        debug_endl();
        break;

    case MOBILE_COMMAND_END_SESSION:
        debug_print("End session");
        packet_end(adapter, packet, 0);
        if (send) debug_endl();
        break;

    case MOBILE_COMMAND_DIAL_TELEPHONE:
        debug_print("Dial telephone");
        if (!send) {
            if (packet->length < 2) {
                packet_end(adapter, packet, 0);
                break;
            }
            debug_print(" (prot %d): ", packet->data[0]);
            debug_write(packet->data + 1, packet->length - 1);
            debug_endl();
        } else {
            packet_end(adapter, packet, 0);
        }
        break;

    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
        debug_print("Hang up telephone");
        packet_end(adapter, packet, 0);
        break;

    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
        debug_print("Wait for telephone call");
        packet_end(adapter, packet, 0);
        break;

    case MOBILE_COMMAND_TRANSFER_DATA:
        debug_print("Transfer data");
        if (packet->length < 1) break;

        if (packet->data[0] == 0xff) {
            debug_print(" (p2p)");
        } else {
            debug_print(" (conn %u)", packet->data[0]);
        }
        dump(adapter, packet->data + 1, packet->length - 1);
        break;

    case MOBILE_COMMAND_RESET:
        debug_print("Reset");
        packet_end(adapter, packet, 0);
        break;

    case MOBILE_COMMAND_TELEPHONE_STATUS:
        debug_print("Telephone status");
        if (!send) {
            packet_end(adapter, packet, 0);
        } else {
            if (packet->length < 3) {
                packet_end(adapter, packet, 0);
                break;
            }
            debug_print(": %02X %02X %02X",
                    packet->data[0], packet->data[1], packet->data[2]);
            packet_end(adapter, packet, 3);
        }
        break;

    case MOBILE_COMMAND_SIO32_MODE:
        debug_print("Serial 32-bit mode");
        if (!send) {
            if (packet->length < 1) break;
            if (packet->data[0] != 0) {
                debug_print(": On");
            } else {
                debug_print(": Off");
            }
            packet_end(adapter, packet, 1);
        } else {
            packet_end(adapter, packet, 0);
        }
        break;

    case MOBILE_COMMAND_READ_CONFIGURATION_DATA:
        debug_print("Read configuration data");
        if (!send) {
            if (packet->length < 2) {
                packet_end(adapter, packet, 0);
                break;
            }
            debug_print(" (offset: %02X; size: %02X)", packet->data[0],
                packet->data[1]);
            packet_end(adapter, packet, 2);
        } else {
            if (packet->length < 1) break;
            debug_print(" (offset: %02X)", packet->data[0]);
            dump_hex(adapter, packet->data + 1, packet->length - 1);
        }
        break;

    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
        debug_print("Write configuration data");
        if (!send) {
            if (packet->length < 1) break;
            debug_print(" (offset: %02X)", packet->data[0]);
            dump_hex(adapter, packet->data + 1, packet->length - 1);
        } else {
            if (packet->length < 2) break;
            debug_print(" (offset: %02X; size: %02X)", packet->data[0],
                packet->data[1]);
            packet_end(adapter, packet, 2);
        }
        break;

    case MOBILE_COMMAND_TRANSFER_DATA_END:
        debug_print("Transfer data end");
        if (packet->length < 1) break;
        debug_print(" (conn %u)", packet->data[0]);
        packet_end(adapter, packet, 1);
        break;

    case MOBILE_COMMAND_ISP_LOGIN:
        debug_print("ISP login");
        if (!send) {
            if (packet->length < 1) break;

            const unsigned char *data = packet->data;
            if (packet->data + packet->length < data + 1 + data[0]) {
                packet_end(adapter, packet, data - packet->data);
                break;
            }
            debug_print(" (id: ");
            debug_write(data + 1, data[0]);
            data += 1 + data[0];

            if (packet->data + packet->length < data + 1 + data[0] + 8) {
                debug_print(")");
                packet_end(adapter, packet, data - packet->data);
                break;
            }
            data += 1 + data[0];

            debug_print("; dns1: %u.%u.%u.%u; dns2: %u.%u.%u.%u)",
                    data[0], data[1], data[2], data[3],
                    data[4], data[5], data[6], data[7]);
            data += 8;
            packet_end(adapter, packet, data - packet->data);
        } else {
            if (packet->length < 4 * 3) {
                packet_end(adapter, packet, 0);
                break;
            }
            debug_print(" (ip: %u.%u.%u.%u; dns1: %u.%u.%u.%u; dns2: %u.%u.%u.%u)",
                    packet->data[0], packet->data[1],
                    packet->data[2], packet->data[3],
                    packet->data[4], packet->data[5],
                    packet->data[6], packet->data[7],
                    packet->data[8], packet->data[9],
                    packet->data[10], packet->data[11]);
            packet_end(adapter, packet, 4 * 3);
        }
        break;

    case MOBILE_COMMAND_ISP_LOGOUT:
        debug_print("ISP logout");
        packet_end(adapter, packet, 0);
        break;

    case MOBILE_COMMAND_OPEN_TCP_CONNECTION:
    case MOBILE_COMMAND_OPEN_UDP_CONNECTION:
        if (packet->command == MOBILE_COMMAND_OPEN_TCP_CONNECTION) {
            debug_print("Open TCP connection");
        }
        if (packet->command == MOBILE_COMMAND_OPEN_UDP_CONNECTION) {
            debug_print("Open UDP connection");
        }
        if (!send) {
            if (packet->length < 6) {
                packet_end(adapter, packet, 0);
                break;
            }
            debug_print(": %u.%u.%u.%u:%u",
                    packet->data[0], packet->data[1],
                    packet->data[2], packet->data[3],
                    packet->data[4] << 8 | packet->data[5]);
            packet_end(adapter, packet, 6);
        } else {
            if (packet->length < 1) break;
            debug_print(" (conn %u)", packet->data[0]);
            packet_end(adapter, packet, 1);
        }
        break;

    case MOBILE_COMMAND_CLOSE_TCP_CONNECTION:
    case MOBILE_COMMAND_CLOSE_UDP_CONNECTION:
        if (packet->command == MOBILE_COMMAND_CLOSE_TCP_CONNECTION) {
            debug_print("Close TCP connection");
        }
        if (packet->command == MOBILE_COMMAND_CLOSE_UDP_CONNECTION) {
            debug_print("Close UDP connection");
        }
        if (packet->length < 1) break;
        debug_print(" (conn %u)", packet->data[0]);
        packet_end(adapter, packet, 1);
        break;

    case MOBILE_COMMAND_DNS_QUERY:
        debug_print("DNS query");
        if (!send) {
            debug_print(": ");
            debug_write(packet->data, packet->length);
            debug_endl();
        } else {
            if (packet->length < 4) {
                packet_end(adapter, packet, 0);
                break;
            }
            debug_print(": %u.%u.%u.%u",
                packet->data[0], packet->data[1],
                packet->data[2], packet->data[3]);
            packet_end(adapter, packet, 4);
        }
        break;

    case MOBILE_COMMAND_ERROR:
        debug_print("Error");
        if (packet->length < 2) {
            packet_end(adapter, packet, 0);
            break;
        }
        debug_print(": %02X", packet->data[1]);
        packet_end(adapter, packet, 2);
        break;

    default:
        debug_print("Unknown");
        dump_hex(adapter, packet->data, packet->length);
    }
}
