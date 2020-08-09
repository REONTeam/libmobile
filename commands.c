#include "commands.h"

#include <stdbool.h>
#include <string.h>

#include "mobile.h"
#include "dns.h"

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
// - How many connections can the adapter make at the same time?
// - Is connection ID 0xFF reserved for calls or can it be returned?
//     Is this ID ever even checked during a call?

// UNKERR is used for errors of which we don't really know if they exist, and
//   if so what error code they return, but have been implemented just in case.
// NEWERR is used to indicate an error code that we made up ourselves to
//   indicate something that couldn't happen with the real adapter.

static struct mobile_packet *error_packet(struct mobile_packet *packet, const unsigned char error)
{
    enum mobile_command command = packet->command;
    packet->command = MOBILE_COMMAND_ERROR;
    packet->length = 2;
    packet->data[0] = command;
    packet->data[1] = error;
    return packet;
}

static bool parse_address(unsigned char *address, char *data)
{
    // Converts a string of 12 characters to a binary representation for an IPv4
    //   address. It also checks for the validity of the address while doing so.
    // The output will be a buffer of 4 bytes, representing the address.

    char *cur_data = data;
    unsigned char *cur_addr = address;
    for (unsigned y = 0; y < 4; y++) {
        unsigned cur_num = 0;
        for (unsigned x = 0; x < 3; x++) {
            if (*cur_data < '0' || *cur_data > '9') return false;
            cur_num *= 10;
            cur_num += *cur_data++ - '0';
        }
        if (cur_num > 255) return false;
        *cur_addr++ = cur_num;
    }
    return true;
}

static bool transfer_data(struct mobile_adapter *adapter, unsigned conn, unsigned char *data, unsigned *size)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    // Pokémon Crystal expects communications to be "synchronized".
    // For this, we only try to receive packets when we've sent one.
    // TODO: Check other games with peer to peer functionality.

    int recv_size = 0;
    if (s->state == MOBILE_CONNECTION_CALL) {
        // Call mode
        if (!mobile_board_tcp_send(_u, conn, data, *size)) return false;
        if (*size > 0) s->call_packets_sent++;
        if (s->call_packets_sent) {
            recv_size = mobile_board_tcp_recv(_u, conn, data, MOBILE_MAX_DATA_SIZE - 1);
            if (recv_size != 0) s->call_packets_sent--;
        } else {
            // Check if the connection is alive
            recv_size = mobile_board_tcp_recv(_u, conn, NULL, 0);
        }
    } else {
        // Internet mode
        if (!mobile_board_tcp_send(_u, conn, data, *size)) return false;
        recv_size = mobile_board_tcp_recv(_u, conn, data, MOBILE_MAX_DATA_SIZE - 1);
    }
    if (recv_size == -10) return true;  // Allow echoing the packet (weak_defs.c)
    if (recv_size < 0) return false;

    *size = (unsigned)recv_size;
    return true;
}

struct mobile_packet *mobile_commands_process(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    switch (packet->command) {
    case MOBILE_COMMAND_BEGIN_SESSION:
        // Errors:
        // 1 - Invalid use (Already begun a session)
        // 2 - Invalid contents

        if (s->session_begun) return error_packet(packet, 1);
        if (adapter->config.device != MOBILE_ADAPTER_RED) {
            if (packet->length != 8) return error_packet(packet, 2);
        } else {
            if (packet->length < 8) return error_packet(packet, 2);
            packet->length = 8;
        }
        if (memcmp(packet->data, "NINTENDO", 8) != 0) {
            return error_packet(packet, 2);
        }

        s->session_begun = true;
        s->state = MOBILE_CONNECTION_DISCONNECTED;
        memset(s->connections, false, sizeof(s->connections));
        return packet;

    case MOBILE_COMMAND_END_SESSION:
        // Errors:
        // 2 - Still connected/failed to disconnect(?)

        // TODO: What happens if the packet has a body? Probably nothing.
        for (unsigned i = 0; i < MOBILE_MAX_CONNECTIONS; i++) {
            if (s->connections[i]) mobile_board_tcp_disconnect(_u, i);
        }
        s->session_begun = false;
        return packet;

    case MOBILE_COMMAND_DIAL_TELEPHONE:
        // Errors:
        // 0 - Telephone line is busy
        // 1 - Invalid use (already connected)
        // 2 - Invalid contents (first byte isn't correct)
        // 3 - Communication failed/phone is not connected
        // 4 - Call not established, redial

        // TODO: Max length: 0x1f
        // TODO: Acceptable characters: 0-9, # and *. Unacceptable characters are ignored.
        // TODO: First byte must be:
        //       0 for blue adapter
        //       1 for green or red adapter
        //       2 for yellow adapter
        //       might be 9 for some as well?

        if (s->state != MOBILE_CONNECTION_DISCONNECTED) {
            return error_packet(packet, 1);
        }

        // Close any connection created by "wait for telephone call"
        if (s->connections[0]) {
            mobile_board_tcp_disconnect(_u, 0);
            s->connections[0] = false;
        }

        // Ignore the ISP phone numbers for now
        if (packet->length == 6 &&
                (memcmp(packet->data + 1, "#9677", 5) == 0 ||
                 memcmp(packet->data + 1, "#9477", 5) == 0)) {
            s->state = MOBILE_CONNECTION_CALL;
            packet->length = 0;
            return packet;
        }
        if (packet->length == 11 &&
                memcmp(packet->data + 1, "0077487751", 10) == 0) {
            s->state = MOBILE_CONNECTION_CALL;
            packet->length = 0;
            return packet;
        }

        // Interpret the number as an IP and connect to someone.
        if (packet->length == 3 * 4 + 1) {
            unsigned char address[4];
            if (!parse_address(address, (char *)packet->data + 1)) {
                return error_packet(packet, 2);
            }
            if (mobile_board_tcp_connect(_u, 0, address,
                    adapter->config.p2p_port)) {
                s->state = MOBILE_CONNECTION_CALL;
                s->connections[0] = true;
                s->call_packets_sent = 0;
                packet->length = 0;
                return packet;
            }
        }

        // TODO: What error is returned when the phone is connected but
        //       can't reach anything?
        // TODO: What happens if the packet has no body? Probably nothing.
        return error_packet(packet, 3);

    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
        // Errors:
        // 0 - ???
        // 1 - Invalid use (already hung up/phone unplugged)
        // TODO: Actually implement
        //return error_packet(packet, 0);  // UNKERR
        // TODO: What happens if the packet has a body? Probably nothing.
        // TODO: What happens if hanging up a non-existing connection?
        if (s->connections[0]) {
            mobile_board_tcp_disconnect(_u, 0);
            s->connections[0] = false;
        }
        s->state = MOBILE_CONNECTION_DISCONNECTED;
        return packet;

    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
        // Errors:
        // 0 - No call received/phone not connected
        // 1 - Invalid use (already calling)
        // 3 - Internal error (ringing but picking up fails)

        // TODO: What happens if the packet has a body? Probably nothing.

        if (s->state != MOBILE_CONNECTION_DISCONNECTED) {
            packet->length = 0;
            return packet;
        }

        if (!s->connections[0]) {
            if (!mobile_board_tcp_listen(_u, 0, adapter->config.p2p_port)) {
                return error_packet(packet, 0);
            }
            s->connections[0] = true;
        }

        if (!mobile_board_tcp_accept(_u, 0)) {
            return error_packet(packet, 0);
        }

        s->state = MOBILE_CONNECTION_CALL;
        s->call_packets_sent = 0;
        packet->length = 0;
        return packet;

    case MOBILE_COMMAND_TRANSFER_DATA:
        // Errors:
        // 0 - Invalid connection (not connected)
        // 1 - Invalid use (Call was ended/never made)
        // 2 - UNKERR: Invalid contents

        if (packet->length < 1) return error_packet(packet, 2);  // UNKERR
        if (s->state == MOBILE_CONNECTION_DISCONNECTED) {
            return error_packet(packet, 1);
        }

        {
            unsigned conn = packet->data[0];

            // P2P connections use ID 0xFF
            if (s->state == MOBILE_CONNECTION_CALL) {
                if (conn != 0xFF) {
                    return error_packet(packet, 2);
                }
                conn = 0;
            }

            if (conn >= MOBILE_MAX_CONNECTIONS || !s->connections[conn]) {
                return error_packet(packet, 1);
            }

            unsigned char *data = packet->data + 1;
            unsigned size = packet->length - 1;
            if (transfer_data(adapter, conn, data, &size)) {
                packet->length = size + 1;
            } else {
                mobile_board_tcp_disconnect(_u, conn);
                s->connections[conn] = false;
                if (s->state == MOBILE_CONNECTION_CALL) {
                    //s->state = MOBILE_CONNECTION_DISCONNECTED;
                    //return error_packet(packet, 1);
                    // TODO: How do we detect a connection drop vs a disconnect?
                } else if (s->state == MOBILE_CONNECTION_INTERNET) {
                    packet->command = MOBILE_COMMAND_TRANSFER_DATA_END;
                    packet->length = 1;
                }
            }
        }
        return packet;

    case MOBILE_COMMAND_TELEPHONE_STATUS:
        // TODO: Actually implement
        // TODO: What happens if the packet has a body? Probably nothing.

        packet->length = 3;
        switch (s->state) {
            // 0xFF if phone is disconnected
            default:
            case MOBILE_CONNECTION_DISCONNECTED: packet->data[0] = 0; break;
            case MOBILE_CONNECTION_CALL: packet->data[0] = 4; break;
            case MOBILE_CONNECTION_INTERNET: packet->data[0] = 5; break;
        }
        switch (adapter->config.device) {
            default:
            case MOBILE_ADAPTER_BLUE: packet->data[1] = 0x4D; break;
            case MOBILE_ADAPTER_RED:
            case MOBILE_ADAPTER_YELLOW: packet->data[1] = 0x48; break;
        }
        packet->data[2] = adapter->config.unmetered ? 0xF0 : 0x00;
        return packet;

    case MOBILE_COMMAND_SIO32_MODE:
        // Errors:
        // 2 - Invalid contents

        if (packet->length < 1) {
            // TODO: Packet length is seemingly never properly checked,
            //       but that should be verified.
            return error_packet(packet, 2);
        }

        if (packet->data[0] != 0 && packet->data[0] != 1) {
            return error_packet(packet, 2);
        }

        adapter->serial.mode_32bit = packet->data[0] == 1;
        packet->length = 0;
        return packet;

    case MOBILE_COMMAND_READ_CONFIGURATION_DATA:
        // Errors:
        // 0 - NEWERR: Internal error (Failed to read config)
        // 1 - UNKERR: Invalid contents
        // 2 - Invalid use (Tried to read outside of configuration area)

        // TODO: Can't read chunks bigger than 0x80,
        //       Can read up to address 0x100

        if (packet->length != 2) return error_packet(packet, 1);  // UNKERR

        {
            unsigned offset = packet->data[0];
            unsigned size = packet->data[1];
            if (offset + size > MOBILE_CONFIG_SIZE) {
                return error_packet(packet, 2);
            }
            packet->length = size + 1;  // Preserve offset byte
            if (size) {
                if (!mobile_board_config_read(_u, packet->data + 1, offset,
                            size)) {
                    return error_packet(packet, 0);  // NEWERR
                }
            }
        }
        return packet;

    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
        // Errors:
        // 0 - NEWERR: Internal error (Failed to write config)
        // 1 - UNKERR: Invalid contents
        // 2 - Invalid use (Tried to write outside of configuration area)

        // TODO: Returns offset and size written

        if (packet->length < 2) return error_packet(packet, 1);  // UNKERR
        if (packet->data[0] + packet->length - 1 > MOBILE_CONFIG_SIZE) {
            return error_packet(packet, 2);
        }
        if (!mobile_board_config_write(_u, packet->data + 1, packet->data[0],
                    packet->length - 1)) {
            return error_packet(packet, 0);  // NEWERR
        }
        packet->length = 0;
        return packet;

    case MOBILE_COMMAND_ISP_LOGIN:
        // Errors:
        // 1 - Invalid use (Not in a call)
        // 2 - Unknown error (timeout)
        // 3 - Unknown error (timeout)

        if (s->state != MOBILE_CONNECTION_CALL) {
            return error_packet(packet, 1);
        }

        if (s->state == MOBILE_CONNECTION_INTERNET) {
            return error_packet(packet, 2);  // UNKERR
        }

        // TODO: Maybe use connection to actually log in?
        // Make sure we aren't connected to an actual phone
        if (s->connections[0]) return error_packet(packet, 1);

        // TODO: Actually implement?
        // TODO: Maximum username and password size: 0x20
        const unsigned char *data = packet->data;
        if (packet->data + packet->length < data + 1 + data[0]) {
            return error_packet(packet, 0);  // UNKERR
        }
        // ID = data + 1; ID_SIZE = data[0];
        data += 1 + data[0];
        if (packet->data + packet->length < data + 1 + data[0] + 8) {
            return error_packet(packet, 0);  // UNKERR
        }
        // PASS = data + 1; PASS_SIZE = data[0];
        data += 1 + data[0];

        memcpy(s->dns1, data + 0, 4);
        memcpy(s->dns2, data + 4, 4);

        // TODO: Returns 3 ips! (last two are DNS?)
        s->state = MOBILE_CONNECTION_INTERNET;
        packet->data[0] = 0;
        packet->data[1] = 0;
        packet->data[2] = 0;
        packet->data[3] = 0;
        packet->length = 4;
        return packet;

    case MOBILE_COMMAND_ISP_LOGOUT:
        // TODO: What happens if the packet has a body? Probably nothing.
        // Errors:
        // 0 - Not logged in
        // 1 - Invalid use (Not in a call)
        // 2 - Unknown error (timeout)

        if (s->state != MOBILE_CONNECTION_INTERNET) {
            return error_packet(packet, 0);  // UNKERR
        }

        // TODO: Actually implement?
        for (unsigned i = 0; i < MOBILE_MAX_CONNECTIONS; i++) {
            if (s->connections[i]) mobile_board_tcp_disconnect(_u, i);
        }
        s->state = MOBILE_CONNECTION_CALL;
        return packet;

    case MOBILE_COMMAND_OPEN_TCP_CONNECTION:
        // Errors:
        // 0 - Too many connections
        // 1 - Invalid use (not in a call/logged in)
        // 3 - Connection failed

        if (s->state != MOBILE_CONNECTION_INTERNET) {
            return error_packet(packet, 0);  // UNKERR
        }
        if (packet->length != 6) {
            return error_packet(packet, 1);  // UNKERR
        }

        {
            unsigned conn;
            for (conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
                if (!s->connections[conn]) break;
            }
            if (conn >= MOBILE_MAX_CONNECTIONS) {
                return error_packet(packet, 2);  // UNKERR
            }

            if (!mobile_board_tcp_connect(_u, conn, packet->data,
                        packet->data[4] << 8 | packet->data[5])) {
                return error_packet(packet, 0);  // UNKERR
            }
            s->connections[conn] = true;

            packet->length = 1;
            packet->data[0] = conn;
        }

        return packet;

    case MOBILE_COMMAND_CLOSE_TCP_CONNECTION:
        // TODO: What happens if the packet has extra data? Probably nothing.
        // Errors:
        // 0 - Invalid connection (not connected)
        // 1 - Invalid use (not in a call/logged in)
        // 2 - Unknown error

        if (packet->length != 1) {
            return error_packet(packet, 1);  // UNKERR
        }
        if (s->state != MOBILE_CONNECTION_INTERNET) {
            return error_packet(packet, 0);  // UNKERR
        }

        {
            unsigned conn = packet->data[0];
            if (conn >= MOBILE_MAX_CONNECTIONS || !s->connections[conn]) {
                return error_packet(packet, 0);  // UNKERR
            }
            mobile_board_tcp_disconnect(_u, conn);
            s->connections[conn] = false;
        }

        return packet;

    case MOBILE_COMMAND_DNS_QUERY:
        // Errors:
        // 0 - ???
        // 1 - Invalid use (not connected)
        // 2 - Invalid contents/lookup failed

        // TODO: Parse IPv4 dot-notation string if string has only numbers and dots up to the first 0 byte.
        //       If it can't be parsed return 255.255.255.255. If it's 0, return error 2.
        // TODO: Limit the hostname to 0x1f bytes.

        if (s->state != MOBILE_CONNECTION_INTERNET) {
            return error_packet(packet, 1);
        }

        {
            unsigned conn;
            for (conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
                if (!s->connections[conn]) break;
            }
            if (conn >= MOBILE_MAX_CONNECTIONS) {
                return error_packet(packet, 2);
            }

            unsigned char ip[4];
            s->connections[conn] = true;
            if (!mobile_dns_query(adapter, conn, ip,
                        (char *)packet->data, packet->length)) {
                return error_packet(packet, 2);
            }
            s->connections[conn] = false;
            memcpy(packet->data, ip, 4);
            packet->length = 4;
        }
        return packet;

    // TODO: Command 0x3F FIRMWARE_VERSION never returns anything and locks
    //         you out of all commands, except 0x16 and 0x11 (blue adapter only).

    default:
        // Nonexisting commands can't be used at any time
        return error_packet(packet, 1);
    }
}
