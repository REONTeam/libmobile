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

// Connection number to use for p2p comms
static const int p2p_conn = 0;

static const char *isp_numbers[] = {
    "#9677",  // DION PDC/CDMAONE - ISP login
    "#9477",  // DION PDC/CDMAONE - Service/configuration number
    "0077487751",  // DION DDI-POCKET - ISP login
    "0077487752",  // DION DDI-POCKET - Service/configuration number
    "0755311973",  // NINTENDO TEST
    NULL
};

static struct mobile_packet *error_packet(struct mobile_packet *packet, const unsigned char error)
{
    enum mobile_command command = packet->command;
    packet->command = MOBILE_COMMAND_ERROR;
    packet->data[0] = command;
    packet->data[1] = error;
    packet->length = 2;
    return packet;
}

static bool parse_phone_address(unsigned char *address, char *data)
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
        if (!mobile_board_sock_send(_u, conn, data, *size, NULL)) return false;
        if (*size > 0) s->call_packets_sent++;
        if (s->call_packets_sent) {
            recv_size = mobile_board_sock_recv(_u, conn, data,
                MOBILE_MAX_DATA_SIZE - 1, NULL);
            if (recv_size != 0) s->call_packets_sent--;
        } else {
            // Check if the connection is alive
            recv_size = mobile_board_sock_recv(_u, conn, NULL, 0, NULL);
        }
    } else {
        // Internet mode
        if (!mobile_board_sock_send(_u, conn, data, *size, NULL)) return false;
        // TODO: Wait up to 1s as long as nothing is received.
        recv_size = mobile_board_sock_recv(_u, conn, data,
            MOBILE_MAX_DATA_SIZE - 1, NULL);
    }
    if (recv_size == -10) return true;  // Allow echoing the packet (weak_defs.c)
    if (recv_size < 0) return false;

    *size = (unsigned)recv_size;
    return true;
}

static bool do_isp_logout(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    // Clean up internet connections if connected to the internet
    if (s->state != MOBILE_CONNECTION_INTERNET) return false;
    for (unsigned char conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
        if (s->connections[conn]) mobile_board_sock_close(_u, conn);
    }
    s->state = MOBILE_CONNECTION_CALL;
    return true;
}

static bool do_hang_up_telephone(struct mobile_adapter *adapter)
{
    do_isp_logout(adapter);

    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    // Clean up p2p connections if in a call
    if (s->state != MOBILE_CONNECTION_CALL) return false;
    if (s->connections[p2p_conn]) mobile_board_sock_close(_u, p2p_conn);
    s->state = MOBILE_CONNECTION_DISCONNECTED;
    return true;
}

// Errors:
// 1 - Invalid use (Already begun a session)
// 2 - Invalid contents
static struct mobile_packet *command_begin_session(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

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
}

// Errors:
// 2 - Still connected/failed to disconnect(?)
static struct mobile_packet *command_end_session(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    do_hang_up_telephone(adapter);

    // Clean up a possibly residual connection that wasn't established
    if (s->connections[p2p_conn]) mobile_board_sock_close(_u, p2p_conn);

    s->session_begun = false;

    packet->length = 0;
    return packet;
}

static struct mobile_packet *command_dial_telephone_begin(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    // TODO: Max length: 0x20
    // TODO: Acceptable characters: 0-9, # and *. Unacceptable characters are
    //         ignored.
    // TODO: First byte must be:
    //       0 for blue adapter
    //       1 for green or red adapter
    //       2 for yellow adapter
    //       red adapter also accepts 9 for some reason, yellow adapter doesn't
    //         parse this.

    if (s->state != MOBILE_CONNECTION_DISCONNECTED) {
        // TODO: There's a possibility the adapter allows calling a new number
        //         whilst connected to the internet...
        //       This disconnects it from the internet, of course.
        return error_packet(packet, 1);
    }
    if (packet->length < 1) return error_packet(packet, 2);

    // Close any connection created by "wait for telephone call"
    if (s->connections[p2p_conn]) {
        mobile_board_sock_close(_u, p2p_conn);
        s->connections[p2p_conn] = false;
    }

    // Ignore the ISP phone numbers for now
    for (const char **number = isp_numbers; *number; number++) {
        if (packet->length - 1 != strlen(*number)) continue;
        if (memcmp(packet->data + 1, *number, packet->length - 1) == 0) {
            s->state = MOBILE_CONNECTION_CALL;
            packet->length = 0;
            return packet;
        }
    }

    // Interpret the number as an IP and connect to someone
    if (packet->length == 1 + 3 * 4) {
        if (!mobile_board_sock_open(_u, p2p_conn, MOBILE_SOCKTYPE_TCP,
                MOBILE_ADDRTYPE_IPV4, 0)) {
            return error_packet(packet, 3);
        }
        s->connections[p2p_conn] = true;
        s->processing = 1;  // command_dial_telephone_ip
        return NULL;
    }

    return error_packet(packet, 3);
}

static struct mobile_packet *command_dial_telephone_ip(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    // Convert the numerical phone "ip address" into a real ipv4 address
    struct mobile_addr4 addr = {
        .type = MOBILE_ADDRTYPE_IPV4,
        .port = adapter->config.p2p_port,
    };
    if (!parse_phone_address(addr.host, (char *)packet->data + 1)) {
        return error_packet(packet, 3);
    }

    // Check if we're connected until it either errors or succeeds
    int rc = mobile_board_sock_connect(_u, p2p_conn, (struct mobile_addr *)&addr);
    if (rc == 0) return NULL;  // Not connected, no error; try again
    if (rc == -1) {
        s->connections[p2p_conn] = false;
        return error_packet(packet, 3);
    }

    s->state = MOBILE_CONNECTION_CALL;
    s->call_packets_sent = 0;

    packet->length = 0;
    return packet;
}

// Errors:
// 0 - Telephone line is busy
// 1 - Invalid use (already connected)
// 2 - Invalid contents (first byte isn't correct)
// 3 - Communication failed/phone is not connected
// 4 - Call not established, redial
static struct mobile_packet *command_dial_telephone(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    switch (s->processing) {
    case 0:
        mobile_board_time_latch(_u, MOBILE_TIMER_COMMAND);
        return command_dial_telephone_begin(adapter, packet);
    case 1:
        if (mobile_board_time_check_ms(_u, MOBILE_TIMER_COMMAND, 60000)) {
            mobile_board_sock_close(_u, p2p_conn);
            return error_packet(packet, 3);
        }
        return command_dial_telephone_ip(adapter, packet);
    default:
        return error_packet(packet, 3);
    }
}

// Errors:
// 1 - Invalid use (already hung up/phone unplugged)
static struct mobile_packet *command_hang_up_telephone(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    if (!do_hang_up_telephone(adapter)) return error_packet(packet, 1);
    packet->length = 0;
    return packet;
}

// Errors:
// 0 - No call received/phone not connected
// 1 - Invalid use (already calling)
// 3 - Internal error (ringing but picking up fails)
static struct mobile_packet *command_wait_for_telephone_call(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    if (s->state != MOBILE_CONNECTION_DISCONNECTED) {
        packet->length = 0;
        return packet;
    }

    if (!s->connections[p2p_conn]) {
        if (!mobile_board_sock_open(_u, 0, MOBILE_SOCKTYPE_TCP,
                MOBILE_ADDRTYPE_IPV4, adapter->config.p2p_port)) {
            return error_packet(packet, 0);
        }
        if (!mobile_board_sock_listen(_u, 0)) {
            return error_packet(packet, 0);
        }
        s->connections[p2p_conn] = true;
    }

    if (!mobile_board_sock_accept(_u, 0)) {
        return error_packet(packet, 0);
    }

    s->state = MOBILE_CONNECTION_CALL;
    s->call_packets_sent = 0;

    packet->length = 0;
    return packet;
}

// Errors:
// 0 - Invalid connection (not connected)
// 1 - Invalid use (Call was ended/never made)
// 2 - NEWERR: Invalid contents
static struct mobile_packet *command_transfer_data(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    if (s->state == MOBILE_CONNECTION_DISCONNECTED) {
        return error_packet(packet, 1);
    }
    if (packet->length < 1) return error_packet(packet, 2);  // NEWERR

    unsigned char conn = packet->data[0];

    // P2P connections use ID 0xFF, but the adapter ignores this
    if (s->state == MOBILE_CONNECTION_CALL) {
        //if (conn != 0xFF) return error_packet(packet, 2);  // NEWERR
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
        mobile_board_sock_close(_u, conn);
        s->connections[conn] = false;
        if (s->state == MOBILE_CONNECTION_CALL) {
            //s->state = MOBILE_CONNECTION_DISCONNECTED;
            // TODO: How do we detect a connection drop vs a disconnect?
            return error_packet(packet, 1);
        } else if (s->state == MOBILE_CONNECTION_INTERNET) {
            packet->command = MOBILE_COMMAND_TRANSFER_DATA_END;
            packet->length = 1;
        }
    }

    return packet;
}

static struct mobile_packet *command_reset(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Command 0x16 RESET seems to do the same as END_SESSION, except
    //         without a 500ms delay...
    //       Probably unused by games, however.
    (void)adapter;
    return error_packet(packet, 1);
}

static struct mobile_packet *command_telephone_status(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    switch (s->state) {
    // 0xFF if phone is disconnected
    // 1 if phone is ringing, 5 if incoming call is picked up
    default:
    case MOBILE_CONNECTION_DISCONNECTED:
        packet->data[0] = 0;
        break;
    case MOBILE_CONNECTION_CALL:
    case MOBILE_CONNECTION_INTERNET:
        packet->data[0] = 4;  // Outgoing call
        break;
    }
    switch (adapter->config.device) {
    default:
    case MOBILE_ADAPTER_BLUE:
        packet->data[1] = 0x4D;
        break;
    case MOBILE_ADAPTER_RED:
    case MOBILE_ADAPTER_YELLOW:
        packet->data[1] = 0x48;
        break;
    }
    packet->data[2] = adapter->config.unmetered ? 0xF0 : 0x00;
    packet->length = 3;
    return packet;
}

// Errors:
// 2 - Invalid contents
static struct mobile_packet *command_sio32_mode(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    if (packet->length < 1) {
        return error_packet(packet, 2);
    }
    if (packet->data[0] != 0 && packet->data[0] != 1) {
        return error_packet(packet, 2);
    }

    adapter->serial.mode_32bit = packet->data[0] == 1;

    packet->length = 0;
    return packet;
}

// Errors:
// 0 - NEWERR: Internal error (Failed to read config)
// 1 - UNKERR: Invalid contents
// 2 - Invalid use (Tried to read outside of configuration area)
static struct mobile_packet *command_read_configuration_data(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    void *_u = adapter->user;

    // TODO: Can't read chunks bigger than 0x80,
    //       Can read up to address 0x100

    if (packet->length != 2) return error_packet(packet, 1);  // UNKERR

    unsigned offset = packet->data[0];
    unsigned size = packet->data[1];
    if (offset + size > MOBILE_CONFIG_SIZE) {
        return error_packet(packet, 2);
    }
    packet->length = size + 1;  // Preserve offset byte
    if (size) {
        if (!mobile_board_config_read(_u, packet->data + 1, offset, size)) {
            return error_packet(packet, 0);  // NEWERR
        }
    }
    return packet;
}

// Errors:
// 0 - NEWERR: Internal error (Failed to write config)
// 1 - UNKERR: Invalid contents
// 2 - Invalid use (Tried to write outside of configuration area)
static struct mobile_packet *command_write_configuration_data(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    void *_u = adapter->user;

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
}

// Errors:
// 0 - NEWERR: Invalid contents
// 1 - Invalid use (Not in a call)
// 2 - Unknown error (some kind of timeout)
// 3 - Unknown error (some kind of timeout)
static struct mobile_packet *command_isp_login(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    if (s->state != MOBILE_CONNECTION_CALL) {
        return error_packet(packet, 1);
    }

    if (s->state == MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 2);  // UNKERR
    }

    // TODO: Maybe use connection to actually log in?
    // Make sure we aren't connected to an actual phone
    if (s->connections[p2p_conn]) return error_packet(packet, 1);

    const unsigned char *data = packet->data;
    if (packet->data + packet->length < data + 1) {
        return error_packet(packet, 0);  // NEWERR
    }
    unsigned id_size = *data++;
    if (id_size > 0x20) id_size = 0x20;
    if (packet->data + packet->length < data + id_size + 1) {
        return error_packet(packet, 0);  // NEWERR
    }
    // const unsigned char *id = data;
    data += id_size;
    unsigned pass_size = *data++;
    if (pass_size > 0x20) pass_size = 0x20;
    if (packet->data + packet->length < data + pass_size + 8) {
        return error_packet(packet, 0);  // NEWERR
    }
    // const unsigned char *pass = data;
    data += pass_size;
    const unsigned char *dns1 = data + 0;
    const unsigned char *dns2 = data + 4;

    // If either DNS address is all zeroes, the real adapter picks a
    //   dns address on its own, somehow.
    memcpy(s->dns1, dns1, 4);
    memcpy(s->dns2, dns2, 4);
    s->state = MOBILE_CONNECTION_INTERNET;

    // Return 3 IP addresses, the phone's IP, and the chosen DNS servers.
    memset(packet->data + 0, 0, 4);  // Phone's IP
    memset(packet->data + 4, 0, 4);  // Chosen DNS1 if requested DNS1 was zero
    memset(packet->data + 8, 0, 4);  // Chosen DNS2 if requested DNS2 was zero
    packet->length = 4 * 3;
    return packet;
}

// Errors:
// 0 - Not logged in
// 1 - Invalid use (Not in a call)
// 2 - Unknown error (some kind of timeout)
static struct mobile_packet *command_isp_logout(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    if (!do_isp_logout(adapter)) return error_packet(packet, 1);
    packet->length = 0;
    return packet;
}

static struct mobile_packet *command_open_tcp_connection_begin(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    if (s->state != MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 1);
    }
    if (packet->length != 6) {
        return error_packet(packet, 3);
    }

    // Find a free connection to use
    unsigned char conn;
    for (conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
        if (!s->connections[conn]) break;
    }
    if (conn >= MOBILE_MAX_CONNECTIONS) {
        return error_packet(packet, 0);
    }

    if (!mobile_board_sock_open(_u, conn, MOBILE_SOCKTYPE_TCP,
            MOBILE_ADDRTYPE_IPV4, 0)) {
        return error_packet(packet, 3);
    }
    s->connections[conn] = true;

    s->processing_data[0] = conn;
    s->processing = 1;  // command_open_tcp_connection_connecting
    return NULL;
}

static struct mobile_packet *command_open_tcp_connection_connecting(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    unsigned char conn = s->processing_data[0];

    struct mobile_addr4 addr = {
        .type = MOBILE_ADDRTYPE_IPV4,
        .port = packet->data[4] << 8 | packet->data[5],
    };
    memcpy(addr.host, packet->data, 4);

    int rc = mobile_board_sock_connect(_u, conn, (struct mobile_addr *)&addr);
    if (rc == 0) return NULL;
    if (rc == -1) {
        s->connections[conn] = false;
        return error_packet(packet, 3);
    }

    packet->data[0] = conn;
    packet->length = 1;
    return packet;
}

// Errors:
// 0 - Too many connections
// 1 - Invalid use (not in a call/logged in)
// 3 - Connection failed
static struct mobile_packet *command_open_tcp_connection(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    switch (s->processing) {
    case 0:
        mobile_board_time_latch(_u, MOBILE_TIMER_COMMAND);
        return command_open_tcp_connection_begin(adapter, packet);
    case 1:
        // TODO: Verify this timeout with a game
        if (mobile_board_time_check_ms(_u, MOBILE_TIMER_COMMAND, 60000)) {
            mobile_board_sock_close(_u, s->processing_data[0]);
            return error_packet(packet, 3);
        }
        return command_open_tcp_connection_connecting(adapter, packet);
    default:
        return error_packet(packet, 3);
    }
}

// Errors:
// 0 - Invalid connection (not connected)
// 1 - Invalid use (not in a call/logged in)
// 2 - Unknown error (no idea when returned)
static struct mobile_packet *command_close_tcp_connection(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    if (s->state != MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 1);
    }
    if (packet->length != 1) {
        return error_packet(packet, 0);
    }

    unsigned char conn = packet->data[0];
    if (conn >= MOBILE_MAX_CONNECTIONS || !s->connections[conn]) {
        return error_packet(packet, 0);  // UNKERR
    }
    mobile_board_sock_close(_u, conn);
    s->connections[conn] = false;

    packet->length = 1;
    return packet;
}

// Errors:
// 0 - Too many connections
// 1 - Invalid use (not in a call/logged in)
// 2 - Connection failed (though this is never returned)
static struct mobile_packet *command_open_udp_connection(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Implement
    (void)adapter;
    return error_packet(packet, 1);
}

// Errors:
// 0 - Invalid connection (not connected)
// 1 - Invalid use (not in a call/logged in)
// 2 - Unknown error (no idea when returned)
static struct mobile_packet *command_close_udp_connection(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Implement
    (void)adapter;
    return error_packet(packet, 1);
}

static struct mobile_packet *command_dns_query_start(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    if (s->state != MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 1);
    }

    // TODO: Parse IPv4 dot-notation string if string has only numbers and
    //         dots up to the first 0 byte.
    //       If it can't be parsed return 255.255.255.255. If it's 0,
    //         return error 2.
    // TODO: Limit the hostname to 0x20 bytes.
    //         EDIT: What was I smoking? There's no length check????

    // Find a free connection to use for the query
    unsigned char conn;
    for (conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
        if (!s->connections[conn]) break;
    }
    if (conn >= MOBILE_MAX_CONNECTIONS) {
        return error_packet(packet, 2);
    }

    if (!mobile_board_sock_open(_u, conn, MOBILE_SOCKTYPE_UDP,
            MOBILE_ADDRTYPE_IPV4, 0)) {
        return error_packet(packet, 2);
    }
    s->connections[conn] = true;

    if (!mobile_dns_query_send(adapter, conn, (char *)packet->data,
            packet->length)) {
        return error_packet(packet, 2);
    }

    s->processing_data[0] = conn;
    s->processing = 1;  // command_dns_query_check
    return NULL;
}

static struct mobile_packet *command_dns_query_check(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    unsigned char conn = s->processing_data[0];

    unsigned char ip[4];
    int rc = mobile_dns_query_recv(adapter, conn, ip);
    if (rc == 0) return NULL;
    mobile_board_sock_close(_u, conn);
    s->connections[conn] = false;
    if (rc == -1) return error_packet(packet, 2);

    memcpy(packet->data, ip, 4);
    packet->length = 4;
    return packet;
}

// Errors:
// 1 - Invalid use (not connected)
// 2 - Invalid contents/lookup failed
static struct mobile_packet *command_dns_query(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    switch (s->processing) {
    case 0:
        mobile_board_time_latch(_u, MOBILE_TIMER_COMMAND);
        return command_dns_query_start(adapter, packet);
    case 1:
        if (mobile_board_time_check_ms(_u, MOBILE_TIMER_COMMAND, 3000)) {
            mobile_board_sock_close(_u, s->processing_data[0]);
            return error_packet(packet, 2);
        }
        return command_dns_query_check(adapter, packet);
    default:
        return error_packet(packet, 2);
    }
}

static struct mobile_packet *command_firmware_version(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Command 0x3F FIRMWARE_VERSION never returns anything and locks
    //         you out of all commands,
    //         except 0x16 and 0x11 (blue adapter only).
    //       Only accepted when disconnected from calls/the internet.
    (void)adapter;
    return error_packet(packet, 1);
}

struct mobile_packet *mobile_commands_process(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    switch (packet->command) {
    case MOBILE_COMMAND_BEGIN_SESSION:
        return command_begin_session(adapter, packet);
    case MOBILE_COMMAND_END_SESSION:
        return command_end_session(adapter, packet);
    case MOBILE_COMMAND_DIAL_TELEPHONE:
        return command_dial_telephone(adapter, packet);
    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
        return command_hang_up_telephone(adapter, packet);
    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
        return command_wait_for_telephone_call(adapter, packet);
    case MOBILE_COMMAND_TRANSFER_DATA:
        return command_transfer_data(adapter, packet);
    case MOBILE_COMMAND_RESET:
        return command_reset(adapter, packet);
    case MOBILE_COMMAND_TELEPHONE_STATUS:
        return command_telephone_status(adapter, packet);
    case MOBILE_COMMAND_SIO32_MODE:
        return command_sio32_mode(adapter, packet);
    case MOBILE_COMMAND_READ_CONFIGURATION_DATA:
        return command_read_configuration_data(adapter, packet);
    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
        return command_write_configuration_data(adapter, packet);
    case MOBILE_COMMAND_ISP_LOGIN:
        return command_isp_login(adapter, packet);
    case MOBILE_COMMAND_ISP_LOGOUT:
        return command_isp_logout(adapter, packet);
    case MOBILE_COMMAND_OPEN_TCP_CONNECTION:
        return command_open_tcp_connection(adapter, packet);
    case MOBILE_COMMAND_CLOSE_TCP_CONNECTION:
        return command_close_tcp_connection(adapter, packet);
    case MOBILE_COMMAND_OPEN_UDP_CONNECTION:
        return command_open_udp_connection(adapter, packet);
    case MOBILE_COMMAND_CLOSE_UDP_CONNECTION:
        return command_close_udp_connection(adapter, packet);
    case MOBILE_COMMAND_DNS_QUERY:
        return command_dns_query(adapter, packet);
    case MOBILE_COMMAND_FIRMWARE_VERSION:
        return command_firmware_version(adapter, packet);
    default:
        // Nonexisting commands can't be used at any time
        return error_packet(packet, 1);
    }
}
