// SPDX-License-Identifier: LGPL-3.0-or-later
#include "commands.h"

#include <string.h>

#include "data.h"
#include "util.h"
#include "compat.h"
#include "inet_pton.h"

// UNKERR is used for errors of which we don't really know if they exist, and
//   if so what error code they return, but have been implemented just in case.
// NEWERR is used to indicate an error code that we made up ourselves to
//   indicate something that couldn't happen with the real adapter.

// Connection number to use for p2p comms
static const int p2p_conn = 0;

static const char isp_number_pdc_isp[] PROGMEM = "#9677";
static const char isp_number_pdc_serv[] PROGMEM = "#9477";
static const char isp_number_ddi_isp[] PROGMEM = "0077487751";
static const char isp_number_ddi_serv[] PROGMEM = "0077487752";
static const char isp_number_test[] PROGMEM = "0755311973";
static const char *const isp_numbers[] PROGMEM = {
    isp_number_pdc_isp,  // DION PDC/CDMAONE - ISP login
    isp_number_pdc_serv,  // DION PDC/CDMAONE - Service/configuration number
    isp_number_ddi_isp,  // DION DDI-POCKET - ISP login
    isp_number_ddi_serv,  // DION DDI-POCKET - Service/configuration number
    isp_number_test,  // NINTENDO TEST
    NULL
};

void mobile_commands_init(struct mobile_adapter *adapter)
{
    adapter->commands.session_begun = false;
    adapter->commands.packet_parsed = false;
    adapter->commands.mode_32bit = false;
}

static struct mobile_packet *error_packet(struct mobile_packet *packet, const unsigned char error)
{
    enum mobile_command command = packet->command;
    packet->command = MOBILE_COMMAND_ERROR;
    packet->data[0] = command;
    packet->data[1] = error;
    packet->length = 2;
    return packet;
}

static bool do_isp_logout(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    // Clean up internet connections if connected to the internet
    if (s->state != MOBILE_CONNECTION_INTERNET) return false;
    for (unsigned char conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
        if (s->connections[conn]) {
            mobile_board_sock_close(_u, conn);
            s->connections[conn] = false;
        }
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
    if (s->connections[p2p_conn]) {
        mobile_board_sock_close(_u, p2p_conn);
        s->connections[p2p_conn] = false;
    }
    s->state = MOBILE_CONNECTION_DISCONNECTED;
    return true;
}

static void do_end_session(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    // Clean up a possibly residual connection that wasn't established
    if (s->connections[p2p_conn]) mobile_board_sock_close(_u, p2p_conn);

    s->session_begun = false;
}

static void do_begin_session(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    s->session_begun = true;
    s->state = MOBILE_CONNECTION_DISCONNECTED;
    memset(s->connections, false, sizeof(s->connections));
}

void mobile_commands_reset(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    if (s->session_begun) do_end_session(adapter);
    s->mode_32bit = false;
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

    do_begin_session(adapter);
    return packet;
}

// Errors:
// 2 - Still connected/failed to disconnect(?)
static struct mobile_packet *command_end_session(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Reset mode_32bit here? Verify on hardware.
    do_end_session(adapter);

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
    for (const char *const *number = isp_numbers; *number; number++) {
        if (packet->length - 1 != strlen_P(pgm_read_ptr(number))) continue;
        if (memcmp_P(packet->data + 1, pgm_read_ptr(number),
                packet->length - 1) == 0) {
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
    if (!mobile_parse_phoneaddr(addr.host, (char *)packet->data + 1)) {
        return error_packet(packet, 3);
    }

    // Check if we're connected until it either errors or succeeds
    int rc = mobile_board_sock_connect(_u, p2p_conn, (struct mobile_addr *)&addr);
    if (rc == 0) return NULL;  // Not connected, no error; try again
    if (rc == -1) {
        mobile_board_sock_close(_u, p2p_conn);
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
// 3 - Communication failed/phone not connected
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
            s->connections[p2p_conn] = false;
            return error_packet(packet, 3);
        }
        return command_dial_telephone_ip(adapter, packet);
    default:
        return error_packet(packet, 3);
    }
}

// Errors:
// 1 - Invalid use (already hung up/phone not connected)
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
        if (!mobile_board_sock_open(_u, p2p_conn, MOBILE_SOCKTYPE_TCP,
                MOBILE_ADDRTYPE_IPV4, adapter->config.p2p_port)) {
            return error_packet(packet, 0);
        }
        if (!mobile_board_sock_listen(_u, p2p_conn)) {
            mobile_board_sock_close(_u, p2p_conn);
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
// 0 - Invalid connection/communication failed
// 1 - Invalid use (Call was ended/never made)
static struct mobile_packet *command_transfer_data(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    if (s->state == MOBILE_CONNECTION_DISCONNECTED) {
        return error_packet(packet, 1);
    }
    if (packet->length < 1) return error_packet(packet, 0);

    unsigned char conn = packet->data[0];

    // P2P connections use ID 0xFF, but the adapter ignores this
    if (s->state == MOBILE_CONNECTION_CALL) conn = p2p_conn;

    if (conn >= MOBILE_MAX_CONNECTIONS || !s->connections[conn]) {
        return error_packet(packet, 0);
    }

    if (s->processing == 0) {
        s->processing_data[0] = 0;
        mobile_board_time_latch(_u, MOBILE_TIMER_COMMAND);
        s->processing = 1;
    }

    unsigned sent_size = s->processing_data[0];
    unsigned char *data = packet->data + 1;
    unsigned send_size = packet->length - 1;

    if (send_size > sent_size) {
        int rc = mobile_board_sock_send(_u, conn, data + sent_size,
            send_size - sent_size, NULL);
        if (rc < 0) return error_packet(packet, 0);
        sent_size += rc;
        s->processing_data[0] = sent_size;

        // Attempt to send again while not everything has been sent
        if (send_size > sent_size) {
            // TODO: Verify the timeout with a game
            if (mobile_board_time_check_ms(_u, MOBILE_TIMER_COMMAND, 10000)) {
                return error_packet(packet, 0);
            }
            return NULL;
        }

        // Pokémon Crystal expects communications to be "synchronized".
        // For this, we only try to receive packets when we've sent one.
        // TODO: Check other games with peer to peer functionality.
        if (s->state == MOBILE_CONNECTION_CALL) {
            s->call_packets_sent++;
        }
    }

    int recv_size = 0;
    if (s->state != MOBILE_CONNECTION_CALL || s->call_packets_sent) {
        recv_size = mobile_board_sock_recv(_u, conn, data,
            MOBILE_MAX_TRANSFER_SIZE, NULL);
    } else {
        // Check if connection is alive
        recv_size = mobile_board_sock_recv(_u, conn, NULL, 0, NULL);
        if (recv_size >= 0) recv_size = 0;
    }

    if (s->state == MOBILE_CONNECTION_CALL && recv_size > 0) {
        if (s->call_packets_sent) s->call_packets_sent--;
    }

    // If connected to the internet, and a disconnect is received, we should
    // inform the game about this.
    if (recv_size == -2 && s->state == MOBILE_CONNECTION_INTERNET) {
        mobile_board_sock_close(_u, conn);
        s->connections[conn] = false;
        packet->command = MOBILE_COMMAND_TRANSFER_DATA_END;
        packet->length = 1;
        return packet;
    }

    // Allow echoing this packet
    if (recv_size == -10) return packet;

    // Any other errors should raise a proper error
    if (recv_size < 0) return error_packet(packet, 0);

    // If nothing was sent, try to receive for at least one second
    // TODO: Don't delay for UDP connections
    if (s->state == MOBILE_CONNECTION_INTERNET && !send_size && !recv_size &&
            !mobile_board_time_check_ms(_u, MOBILE_TIMER_COMMAND, 1000)) {
        return NULL;
    }

    packet->length = recv_size + 1;
    return packet;
}

// Errors:
// 2 - Still connected/failed to disconnect(?)
static struct mobile_packet *command_reset(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // Reset everything without ending the session
    do_end_session(adapter);
    do_begin_session(adapter);
    s->mode_32bit = false;

    packet->length = 0;
    return packet;
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
// 2 - Invalid contents (first byte not either 1 or 0)
static struct mobile_packet *command_sio32_mode(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    if (packet->length < 1) {
        return error_packet(packet, 2);
    }
    if (packet->data[0] != 0 && packet->data[0] != 1) {
        return error_packet(packet, 2);
    }

    s->mode_32bit = packet->data[0] == 1;

    packet->length = 0;
    return packet;
}

// Errors:
// 0 - Internal error (Failed to read config)
// 2 - Read outside of config area/too big a chunk
static struct mobile_packet *command_read_configuration_data(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    void *_u = adapter->user;

    if (packet->length != 2) return error_packet(packet, 2);

    unsigned offset = packet->data[0];
    unsigned size = packet->data[1];
    if (size > 0x80) return error_packet(packet, 2);
    if (offset + size > MOBILE_CONFIG_SIZE) return error_packet(packet, 2);
    packet->length = size + 1;  // Preserve offset byte
    if (size) {
        if (!mobile_board_config_read(_u, packet->data + 1, offset, size)) {
            return error_packet(packet, 0);
        }
    }
    return packet;
}

// Errors:
// 0 - Internal error (Failed to write config)
// 2 - Invalid use (Tried to write outside of configuration area)
static struct mobile_packet *command_write_configuration_data(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    void *_u = adapter->user;

    if (packet->length < 1) return error_packet(packet, 2);

    unsigned offset = packet->data[0];
    unsigned size = packet->length - 1;
    if (size > 0x80) return error_packet(packet, 2);
    if (offset + size > MOBILE_CONFIG_SIZE) return error_packet(packet, 2);
    if (size) {
        if (!mobile_board_config_write(_u, packet->data + 1, offset, size)) {
            return error_packet(packet, 0);
        }
    }

    packet->length = 2;
    packet->data[1] = size;
    return packet;
}

// Errors:
// 0 - NEWERR: Invalid contents
// 1 - Invalid use (Not in a call)
// 2 - Unknown error (some kind of timeout?)
// 3 - Unknown error (internal error?)
static struct mobile_packet *command_isp_login(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // TODO: The original adapter allows logging in multiple times without
    //         logging out first, but I'm unsure if that's a bug.
    if (s->state != MOBILE_CONNECTION_CALL) {
        return error_packet(packet, 1);
    }

    // Make sure we aren't connected to an actual phone
    if (s->connections[p2p_conn]) return error_packet(packet, 3);

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

    // Check if the DNS addresses are empty
    static const unsigned char dns0[MOBILE_HOSTLEN_IPV4] = {0};
    bool dns1_empty = memcmp(dns1, dns0, MOBILE_HOSTLEN_IPV4) == 0;
    bool dns2_empty = memcmp(dns2, dns0, 4) == 0;

    // Initialize the DNS address structures with whatever data we've got
    s->dns1.type = dns1_empty ? MOBILE_ADDRTYPE_NONE : MOBILE_ADDRTYPE_IPV4;
    s->dns1.port = MOBILE_DNS_PORT;
    memcpy(s->dns1.host, dns1, MOBILE_HOSTLEN_IPV4);
    s->dns2.type = dns2_empty ? MOBILE_ADDRTYPE_NONE : MOBILE_ADDRTYPE_IPV4;
    s->dns2.port = MOBILE_DNS_PORT;
    memcpy(s->dns2.host, dns2, MOBILE_HOSTLEN_IPV4);

    // If either DNS is empty, return the address in the config if available
    const unsigned char *dns1_ret = dns0;
    const unsigned char *dns2_ret = dns0;
    if (dns1_empty && adapter->config.dns1.type == MOBILE_ADDRTYPE_IPV4) {
        struct mobile_addr4 *c_dns1 =
            (struct mobile_addr4 *)&adapter->config.dns1;
        dns1_ret = c_dns1->host;
    }
    if (dns2_empty && adapter->config.dns2.type == MOBILE_ADDRTYPE_IPV4) {
        struct mobile_addr4 *c_dns2 =
            (struct mobile_addr4 *)&adapter->config.dns2;
        dns2_ret = c_dns2->host;
    }

    s->dns2_use = 0;
    s->state = MOBILE_CONNECTION_INTERNET;

    // Return 3 IP addresses, the phone's IP, and the chosen DNS servers.
    static const unsigned char ip_local[] = {127, 0, 0, 1};
    memcpy(packet->data + 0, ip_local, MOBILE_HOSTLEN_IPV4);  // Phone's IP
    memcpy(packet->data + 4, dns1_ret, MOBILE_HOSTLEN_IPV4);  // Chosen DNS1
    memcpy(packet->data + 8, dns2_ret, MOBILE_HOSTLEN_IPV4);  // Chosen DNS2
    packet->length = 4 * 3;
    return packet;
}

// Errors:
// 0 - Invalid use (Not logged in)
// 1 - Invalid use (Not in a call)
// 2 - Unknown error (some kind of timeout?)
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
        mobile_board_sock_close(_u, conn);
        s->connections[conn] = false;
        return error_packet(packet, 3);
    }

    packet->data[0] = conn;
    packet->length = 1;
    return packet;
}

// Errors:
// 0 - Too many connections
// 1 - Invalid use (Not logged in)
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
            s->connections[s->processing_data[0]] = false;
            return error_packet(packet, 3);
        }
        return command_open_tcp_connection_connecting(adapter, packet);
    default:
        return error_packet(packet, 3);
    }
}

// Errors:
// 0 - Invalid connection (Not connected)
// 1 - Invalid use (Not logged in)
// 2 - Unknown error (???)
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
// 1 - Invalid use (Not logged in)
// 2 - Connection failed (though this can't happen)
static struct mobile_packet *command_open_udp_connection(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Implement
    (void)adapter;
    return error_packet(packet, 1);
}

// Errors:
// 0 - Invalid connection (Not connected)
// 1 - Invalid use (Not logged in)
// 2 - Unknown error (???)
static struct mobile_packet *command_close_udp_connection(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Implement
    (void)adapter;
    return error_packet(packet, 1);
}

static struct mobile_addr *dns_get_addr(struct mobile_adapter *adapter, unsigned char id)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // Try DNS2 first if that's the one that worked
    if (s->dns2_use == 1) {
        id = (id + 2) % 4;
    }

    switch (id) {
    default:
    // DNS1
    case 0: return &adapter->config.dns1;
    case 1: return (struct mobile_addr *)&adapter->commands.dns1;
    // DNS2
    case 2: return &adapter->config.dns2;
    case 3: return (struct mobile_addr *)&adapter->commands.dns2;
    }
}

static int dns_query_start(struct mobile_adapter *adapter, struct mobile_packet *packet, unsigned conn, unsigned addr_id)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    // Check any of the DNS addresses to see if they can be used
    // Fall through from DNS1 into DNS2 if DNS1 can't be used
    struct mobile_addr *addr_send;
    for (; addr_id < 4; addr_id++) {
        addr_send = dns_get_addr(adapter, addr_id);
        if (addr_send->type != MOBILE_ADDRTYPE_NONE) break;
    }
    if (addr_id >= 4) return -1;

    // Open connection and send query
    if (!mobile_board_sock_open(_u, conn, MOBILE_SOCKTYPE_UDP,
            addr_send->type, 0)) {
        return -1;
    }
    if (!mobile_dns_query_send(adapter, conn, addr_send, (char *)packet->data,
            packet->length)) {
        mobile_board_sock_close(_u, conn);
        return -1;
    }
    s->connections[conn] = true;

    // Return the DNS ID that was used
    return addr_id;
}

static struct mobile_packet *command_dns_query_start(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    if (s->state != MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 1);
    }

    // If it's an IP address, parse it right here, right now.
    if (mobile_is_ipaddr((char *)packet->data, packet->length)) {
        unsigned char ip[MOBILE_HOSTLEN_IPV4];
        memset(ip, -1, sizeof(ip));
        mobile_pton_length(MOBILE_PTON_IPV4, (char *)packet->data,
            packet->length, ip);

        // The adapter weirdly enough only returns an error if the address is
        //   0, despite the "parse error" value being -1.
        const unsigned char ip0[MOBILE_HOSTLEN_IPV4] = {0};
        if (memcmp(ip, ip0, sizeof(ip)) == 0) return error_packet(packet, 2);

        memcpy(packet->data, ip, sizeof(ip));
        packet->length = 4;
        return packet;
    }

    // Find a free connection to use for the query
    unsigned char conn;
    for (conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
        if (!s->connections[conn]) break;
    }
    if (conn >= MOBILE_MAX_CONNECTIONS) {
        return error_packet(packet, 2);
    }

    int addr_id = dns_query_start(adapter, packet, conn, 0);
    if (addr_id < 0) return error_packet(packet, 2);

    s->processing_data[0] = conn;
    s->processing_data[1] = addr_id;
    s->processing = 1;  // command_dns_query_check
    return NULL;
}

static struct mobile_packet *command_dns_query_check(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    void *_u = adapter->user;

    unsigned char conn = s->processing_data[0];
    int addr_id = s->processing_data[1];
    struct mobile_addr *addr = dns_get_addr(adapter, addr_id);

    unsigned char ip[MOBILE_HOSTLEN_IPV4];
    int rc = mobile_dns_query_recv(adapter, conn, addr, (char *)packet->data,
        packet->length, ip);
    if (rc == 0) return NULL;

    mobile_board_sock_close(_u, conn);
    s->connections[conn] = false;

    if (rc < 0) {
        // If we've checked DNS1 but not yet DNS2, check DNS2
        if (addr_id < 2) {
            addr_id = dns_query_start(adapter, packet, conn, 2);
            if (addr_id < 0) return error_packet(packet, 2);
            s->processing_data[1] = addr_id;
            return NULL;
        }

        // Otherwise we're done...
        return error_packet(packet, 2);
    }

    // If we've checked DNS2 and it worked, store that
    if (addr_id >= 2) s->dns2_use = !s->dns2_use;

    memcpy(packet->data, ip, sizeof(ip));
    packet->length = 4;
    return packet;
}

// Errors:
// 1 - Invalid use (not logged in)
// 2 - Invalid contents/lookup failed
static struct mobile_packet *command_dns_query(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    switch (s->processing) {
    case 0:
        return command_dns_query_start(adapter, packet);
    case 1:
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

bool mobile_commands_exists(enum mobile_command command)
{
    // Used by serial.c:mobile_serial_transfer() to check if a command may be used

    switch (command) {
    case MOBILE_COMMAND_EMPTY:
    case MOBILE_COMMAND_BEGIN_SESSION:
    case MOBILE_COMMAND_END_SESSION:
    case MOBILE_COMMAND_DIAL_TELEPHONE:
    case MOBILE_COMMAND_HANG_UP_TELEPHONE:
    case MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL:
    case MOBILE_COMMAND_TRANSFER_DATA:
    case MOBILE_COMMAND_RESET:
    case MOBILE_COMMAND_TELEPHONE_STATUS:
    case MOBILE_COMMAND_SIO32_MODE:
    case MOBILE_COMMAND_READ_CONFIGURATION_DATA:
    case MOBILE_COMMAND_WRITE_CONFIGURATION_DATA:
    case MOBILE_COMMAND_ISP_LOGIN:
    case MOBILE_COMMAND_ISP_LOGOUT:
    case MOBILE_COMMAND_OPEN_TCP_CONNECTION:
    case MOBILE_COMMAND_CLOSE_TCP_CONNECTION:
    case MOBILE_COMMAND_OPEN_UDP_CONNECTION:
    case MOBILE_COMMAND_CLOSE_UDP_CONNECTION:
    case MOBILE_COMMAND_DNS_QUERY:
    case MOBILE_COMMAND_FIRMWARE_VERSION:
        return true;
    default:
        return false;
    }
}
