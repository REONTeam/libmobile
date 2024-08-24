// SPDX-License-Identifier: LGPL-3.0-or-later
#include "commands.h"

#include <string.h>

#include "mobile_data.h"
#include "mobile_inet.h"
#include "util.h"
#include "compat.h"

#ifdef MOBILE_LIBCONF_USE
#include <mobile_config.h>
#endif

// Accessible area of the mobile config by the game boy
#define MOBILE_CONFIG_SIZE_REAL 0x100
static_assert(MOBILE_CONFIG_SIZE >= MOBILE_CONFIG_SIZE_REAL,
    "MOBILE_CONFIG_SIZE isn't big enough!");

// UNKERR is used for errors of which we don't really know if they exist, and
//   if so what error code they return, but have been implemented just in case.
// NEWERR is used to indicate an error code that we made up ourselves to
//   indicate something that couldn't happen with the real adapter.

// Connection number to use for p2p comms
static const int p2p_conn = 0;

// Static keys
static const char nintendo[] PROGMEM = {
    'N', 'I', 'N', 'T', 'E', 'N', 'D', 'O'
};
static const char happy[] PROGMEM = {
    0x45, 0x56, 0x45, 0x52, 0x59, 0x4f, 0x4e, 0x45,
    0x20, 0x48, 0x41, 0x50, 0x50, 0x59, 0x20, 0x4d,
    0x4f, 0x42, 0x49, 0x4c, 0x45, 0x20, 0x43, 0x4f,
    0x4e, 0x4e, 0x45, 0x43, 0x54, 0x49, 0x4f, 0x4e
};

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
    adapter->commands.session_started = false;
    adapter->commands.mode_32bit = false;
}

static struct mobile_packet *error_packet(struct mobile_packet *packet, unsigned char error)
{
    enum mobile_command command = packet->command;
    packet->command = MOBILE_COMMAND_ERROR;
    packet->data[0] = command;
    packet->data[1] = error;
    packet->length = 2;
    return packet;
}

static int connection_new(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // Find a free connection slot
    unsigned char conn;
    for (conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
        if (!s->connections[conn]) break;
    }
    if (conn >= MOBILE_MAX_CONNECTIONS) return -1;
    return conn;
}

static bool do_ppp_disconnect(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // Clean up internet connections if connected to the internet
    if (s->state != MOBILE_CONNECTION_INTERNET) return false;
    for (unsigned char conn = 0; conn < MOBILE_MAX_CONNECTIONS; conn++) {
        if (s->connections[conn]) {
            mobile_cb_sock_close(adapter, conn);
            s->connections[conn] = false;
        }
    }
    s->state = MOBILE_CONNECTION_CALL_ISP;
    return true;
}

static bool do_offline(struct mobile_adapter *adapter)
{
    do_ppp_disconnect(adapter);

    struct mobile_adapter_commands *s = &adapter->commands;

    // Can't hang up if not in a call
    if (s->state != MOBILE_CONNECTION_CALL &&
            s->state != MOBILE_CONNECTION_CALL_RECV &&
            s->state != MOBILE_CONNECTION_CALL_ISP) {
        return false;
    }

    mobile_cb_update_number(adapter, MOBILE_NUMBER_PEER, NULL);

    // Clean up p2p connections if in a call
    if (s->connections[p2p_conn]) {
        mobile_cb_sock_close(adapter, p2p_conn);
        s->connections[p2p_conn] = false;
    }

    s->state = MOBILE_CONNECTION_DISCONNECTED;
    return true;
}

static void do_end_session(struct mobile_adapter *adapter)
{
    do_offline(adapter);

    struct mobile_adapter_commands *s = &adapter->commands;

    // Clean up a possibly residual connection that wasn't established by
    //   the command_wait_call function
    if (s->connections[p2p_conn]) mobile_cb_sock_close(adapter, p2p_conn);

    s->session_started = false;
    s->mode_32bit = false;
}

static void do_start_session(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    s->session_started = true;
    s->state = MOBILE_CONNECTION_DISCONNECTED;
    memset(s->connections, false, sizeof(s->connections));

    mobile_number_fetch_cancel(adapter);
}

void mobile_commands_reset(struct mobile_adapter *adapter)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    if (s->session_started) do_end_session(adapter);
}

// Errors:
// 1 - Invalid use (Already begun a session)
// 2 - Invalid contents
static struct mobile_packet *command_start(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    if (s->session_started) return error_packet(packet, 1);
    if (packet->length == sizeof(happy) &&
            memcmp_P(packet->data, happy, sizeof(happy)) == 0) {
        do_start_session(adapter);
        return packet;
    }
    if (adapter->serial.device != MOBILE_ADAPTER_RED) {
        if (packet->length != sizeof(nintendo)) return error_packet(packet, 2);
    } else {
        if (packet->length < sizeof(nintendo)) return error_packet(packet, 2);
        packet->length = sizeof(nintendo);
    }
    if (memcmp_P(packet->data, nintendo, sizeof(nintendo)) != 0) {
        return error_packet(packet, 2);
    }

    do_start_session(adapter);
    return packet;
}

// Errors:
// 2 - Still connected/failed to disconnect(?)
static struct mobile_packet *command_end(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Reset mode_32bit here? Verify on hardware.
    //       Currently reset in MOBILE_ACTION_RESET_SERIAL
    do_end_session(adapter);

    packet->length = 0;
    return packet;
}

enum process_tel {
    PROCESS_TEL_BEGIN,
    PROCESS_TEL_IP,
    PROCESS_TEL_RELAY
};

static struct mobile_packet *command_tel_begin(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    if (s->state != MOBILE_CONNECTION_DISCONNECTED &&
            s->state != MOBILE_CONNECTION_WAIT &&
            s->state != MOBILE_CONNECTION_WAIT_RELAY &&
            s->state != MOBILE_CONNECTION_WAIT_TIMEOUT) {
        return error_packet(packet, 1);
    }
    if (packet->length < 1) return error_packet(packet, 2);

    // Close any connection created by command_wait_call
    if (s->connections[p2p_conn]) {
        mobile_cb_sock_close(adapter, p2p_conn);
        s->connections[p2p_conn] = false;
    }
    s->state = MOBILE_CONNECTION_DISCONNECTED;

    // Validate the first byte (unknown purpose...)
    switch (adapter->serial.device) {
    case MOBILE_ADAPTER_BLUE:
        if (packet->data[0] != 0) return error_packet(packet, 2);
        break;

    case MOBILE_ADAPTER_GREEN:
        // Never released (games send value 1)
    case MOBILE_ADAPTER_RED:
        if (packet->data[0] != 1 && packet->data[0] != 9) {
            return error_packet(packet, 2);
        }
        break;

    default:
    case MOBILE_ADAPTER_YELLOW:
        // Not parsed (games send value 2)
        break;
    }

    // Filter acceptable characters out of the string
    unsigned char *w = packet->data + 1;
    for (int i = 0; i < packet->length - 1 && i < 0x20; i++) {
        unsigned char c = packet->data[i + 1];
        if (('0' <= c && c <= '9') || c == '#' || c == '*') *w++ = c;
    }
    *w = '\0';
    packet->length = (unsigned)(w - packet->data);

    // If we're calling an ISP number, simulate being connected
    for (const char *const *ptr = isp_numbers; pgm_read_ptr(ptr); ptr++) {
        const char *number = pgm_read_ptr(ptr);

        if ((unsigned)packet->length - 1 != strlen_P(number)) continue;
        if (memcmp_P(packet->data + 1, number, packet->length - 1) == 0) {
            // Report this number to the implementation
            if (packet->length - 1 <= MOBILE_MAX_NUMBER_SIZE) {
                packet->data[packet->length] = '\0';
                mobile_cb_update_number(adapter, MOBILE_NUMBER_PEER,
                    (char *)packet->data + 1);
            }

            s->state = MOBILE_CONNECTION_CALL_ISP;
            packet->length = 0;
            return packet;
        }
    }

    // If the relay is enabled, start the connection
    if (adapter->config.relay.type != MOBILE_ADDRTYPE_NONE) {
        mobile_addr_copy(&b->processing_addr, &adapter->config.relay);
        mobile_relay_init(adapter);

        if (!mobile_cb_sock_open(adapter, p2p_conn, MOBILE_SOCKTYPE_TCP,
                b->processing_addr.type, 0)) {
            return error_packet(packet, 3);
        }
        s->connections[p2p_conn] = true;

        b->processing = PROCESS_TEL_RELAY;
        return NULL;
    }

    // Interpret the number as an IP and connect to someone
    if (packet->length == 1 + 3 * 4) {
        // Convert the numerical phone "ip address" into a real ipv4 address
        struct mobile_addr4 *addr = (struct mobile_addr4 *)&b->processing_addr;
        if (!mobile_parse_phoneaddr(addr->host, (char *)packet->data + 1)) {
            return error_packet(packet, 3);
        }
        addr->type = MOBILE_ADDRTYPE_IPV4;
        addr->port = adapter->config.p2p_port;

        if (!mobile_cb_sock_open(adapter, p2p_conn, MOBILE_SOCKTYPE_TCP,
                b->processing_addr.type, 0)) {
            return error_packet(packet, 3);
        }
        s->connections[p2p_conn] = true;

        b->processing = PROCESS_TEL_IP;
        return NULL;
    }

    return error_packet(packet, 3);
}

static struct mobile_packet *command_tel_ip(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    // Check if we're connected until it either errors or succeeds
    int rc = mobile_cb_sock_connect(adapter, p2p_conn, &b->processing_addr);
    if (rc == 0) return NULL;
    if (rc < 0) {
        mobile_cb_sock_close(adapter, p2p_conn);
        s->connections[p2p_conn] = false;
        return error_packet(packet, 3);
    }

    // Report the called number to the implementation
    if (packet->length - 1 <= MOBILE_MAX_NUMBER_SIZE) {
        packet->data[packet->length] = '\0';
        mobile_cb_update_number(adapter, MOBILE_NUMBER_PEER,
            (char *)packet->data + 1);
    }

    s->state = MOBILE_CONNECTION_CALL;
    s->call_packets_sent = 0;

    packet->length = 0;
    return packet;
}

static struct mobile_packet *command_tel_relay(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    int rc = mobile_relay_proc_call(adapter, p2p_conn, &b->processing_addr,
        (char *)packet->data + 1, packet->length - 1);
    if (rc == 0) return NULL;
    if (rc < 0) {
        mobile_cb_sock_close(adapter, p2p_conn);
        s->connections[p2p_conn] = false;
        return error_packet(packet, 3);
    }

    // Interpret the result
    int errcode;
    switch (rc) {
        case MOBILE_RELAY_CALL_RESULT_ACCEPTED: errcode = -1; break;
        case MOBILE_RELAY_CALL_RESULT_INTERNAL: errcode = 3; break;
        case MOBILE_RELAY_CALL_RESULT_BUSY: errcode = 0; break;
        case MOBILE_RELAY_CALL_RESULT_UNAVAILABLE: errcode = 0; break;
        default: errcode = 3; break;
    }
    if (errcode != -1) {
        mobile_cb_sock_close(adapter, p2p_conn);
        s->connections[p2p_conn] = false;
        return error_packet(packet, errcode);
    }

    s->state = MOBILE_CONNECTION_CALL;
    s->call_packets_sent = 0;

    packet->length = 0;
    return packet;
}

// Errors:
// 0 - "BUSY" (the called number is busy)
// 1 - Invalid use (already connected)
// 2 - Invalid contents (first byte isn't correct)
// 3 - "NO CARRIER"/"ERROR"/timeout (internal phone/communication error)
// 4 - "REDIAL ERROR"
static struct mobile_packet *command_tel(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    switch (b->processing) {
    case PROCESS_TEL_BEGIN:
        mobile_cb_time_latch(adapter, MOBILE_TIMER_COMMAND);
        return command_tel_begin(adapter, packet);

    case PROCESS_TEL_IP:
        if (mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND, 60000)) {
            mobile_cb_sock_close(adapter, p2p_conn);
            s->connections[p2p_conn] = false;
            return error_packet(packet, 3);
        }
        return command_tel_ip(adapter, packet);

    case PROCESS_TEL_RELAY:
        if (mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND, 60000)) {
            mobile_cb_sock_close(adapter, p2p_conn);
            s->connections[p2p_conn] = false;
            return error_packet(packet, 3);
        }
        return command_tel_relay(adapter, packet);

    default:
        return error_packet(packet, 3);
    }
}

// Errors:
// 1 - Invalid use (already hung up/phone not connected)
static struct mobile_packet *command_offline(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    if (!do_offline(adapter)) return error_packet(packet, 1);
    packet->length = 0;
    return packet;
}

enum process_wait_call {
    PROCESS_WAIT_CALL_INIT,
    PROCESS_WAIT_CALL_INIT_DONE
};

static struct mobile_packet *command_wait_call_begin(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    // Time out if anything fails
    s->state = MOBILE_CONNECTION_WAIT_TIMEOUT;

    if (adapter->config.relay.type != MOBILE_ADDRTYPE_NONE) {
        mobile_addr_copy(&b->processing_addr, &adapter->config.relay);
        mobile_relay_init(adapter);

        // Open the relay connection
        if (!mobile_cb_sock_open(adapter, p2p_conn, MOBILE_SOCKTYPE_TCP,
                b->processing_addr.type, 0)) {
            return error_packet(packet, 0);
        }
        s->connections[p2p_conn] = true;

        s->state = MOBILE_CONNECTION_WAIT_RELAY;
        return NULL;
    }

    // Open the connection and start listening
    if (!mobile_cb_sock_open(adapter, p2p_conn, MOBILE_SOCKTYPE_TCP,
            MOBILE_ADDRTYPE_IPV4, adapter->config.p2p_port)) {
        return error_packet(packet, 0);
    }
    if (!mobile_cb_sock_listen(adapter, p2p_conn)) {
        mobile_cb_sock_close(adapter, p2p_conn);
        return error_packet(packet, 0);
    }
    s->connections[p2p_conn] = true;

    s->state = MOBILE_CONNECTION_WAIT;
    return NULL;
}

static struct mobile_packet *command_wait_call_ip(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // Check if we've received any connection
    if (!mobile_cb_sock_accept(adapter, 0)) return NULL;

    s->state = MOBILE_CONNECTION_CALL_RECV;
    s->call_packets_sent = 0;

    packet->length = 0;
    return packet;
}

static struct mobile_packet *command_wait_call_relay(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    // Connect to the server and wait for a call
    int rc = mobile_relay_proc_wait(adapter, p2p_conn, &b->processing_addr);
    if (rc == 0) return NULL;
    if (rc < 0) {
        mobile_cb_sock_close(adapter, p2p_conn);
        s->connections[p2p_conn] = false;
        s->state = MOBILE_CONNECTION_WAIT_TIMEOUT;
        return error_packet(packet, 3);
    }

    // Interpret the result
    int errcode;
    switch (rc) {
        case MOBILE_RELAY_WAIT_RESULT_ACCEPTED: errcode = -1; break;
        case MOBILE_RELAY_WAIT_RESULT_INTERNAL: errcode = 3; break;
        default: errcode = 3; break;
    }
    if (errcode != -1) {
        mobile_cb_sock_close(adapter, p2p_conn);
        s->connections[p2p_conn] = false;
        s->state = MOBILE_CONNECTION_WAIT_TIMEOUT;
        return error_packet(packet, errcode);
    }

    s->state = MOBILE_CONNECTION_CALL_RECV;
    s->call_packets_sent = 0;

    packet->length = 0;
    return packet;
}

// Errors:
// 0 - No call received/phone not connected
// 1 - Invalid use (already calling)
// 3 - Internal error (ringing but picking up fails)
static struct mobile_packet *command_wait_call(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    if (s->state != MOBILE_CONNECTION_DISCONNECTED &&
            s->state != MOBILE_CONNECTION_WAIT &&
            s->state != MOBILE_CONNECTION_WAIT_RELAY &&
            s->state != MOBILE_CONNECTION_WAIT_TIMEOUT) {
        return error_packet(packet, 1);
    }

    if (b->processing == PROCESS_WAIT_CALL_INIT) {
        // If a previous timeout is in effect, wait it out
        if (s->state == MOBILE_CONNECTION_WAIT_TIMEOUT) {
            if (!mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND,
                    1000)) {
                return NULL;
            }
            s->state = MOBILE_CONNECTION_DISCONNECTED;
        }

        mobile_cb_time_latch(adapter, MOBILE_TIMER_COMMAND);
        b->processing = PROCESS_WAIT_CALL_INIT_DONE;
    }

    // The s->state variable is preserved across multiple commands,
    //   allowing us to resume a started command.
    switch (s->state) {
    default:
    case MOBILE_CONNECTION_DISCONNECTED:
        return command_wait_call_begin(adapter, packet);

    case MOBILE_CONNECTION_WAIT:
        if (mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND, 1000)) {
            return error_packet(packet, 0);
        }
        return command_wait_call_ip(adapter, packet);

    case MOBILE_CONNECTION_WAIT_RELAY:
        if (mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND, 1000)) {
            // If not done connecting to the server, the connection is hanging
            // Treat it as if the connection failed
            if (adapter->relay.state != MOBILE_RELAY_RECV_WAIT) {
                mobile_cb_sock_close(adapter, p2p_conn);
                s->connections[p2p_conn] = false;
                s->state = MOBILE_CONNECTION_DISCONNECTED;
                return error_packet(packet, 3);
            }
            return error_packet(packet, 0);
        }
        return command_wait_call_relay(adapter, packet);
    }
}

enum process_data {
    PROCESS_DATA_INIT,
    PROCESS_DATA_INIT_DONE
};

enum procdata_data {
    PROCDATA_DATA_SENT_SIZE
};

// Errors:
// 0 - Invalid connection/communication failed
// 1 - Invalid use (Call was ended/never made)
static struct mobile_packet *command_data(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    if (s->state != MOBILE_CONNECTION_CALL &&
            s->state != MOBILE_CONNECTION_CALL_RECV &&
            s->state != MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 1);
    }
    if (packet->length < 1) return error_packet(packet, 0);
    const bool internet = s->state == MOBILE_CONNECTION_INTERNET;

    unsigned char conn = packet->data[0];

    // P2P connections use ID 0xff, but the adapter ignores this
    if (!internet) conn = p2p_conn;

    if (conn >= MOBILE_MAX_CONNECTIONS || !s->connections[conn]) {
        return error_packet(packet, 0);
    }

    if (b->processing == PROCESS_DATA_INIT) {
        b->processing_data[PROCDATA_DATA_SENT_SIZE] = 0;
        mobile_cb_time_latch(adapter, MOBILE_TIMER_COMMAND);
        b->processing = PROCESS_DATA_INIT_DONE;
    }

    unsigned sent_size = b->processing_data[PROCDATA_DATA_SENT_SIZE];
    unsigned char *data = packet->data + 1;
    unsigned send_size = packet->length - 1;

    if (send_size > sent_size) {
        int rc = mobile_cb_sock_send(adapter, conn, data + sent_size,
            send_size - sent_size, NULL);
        if (rc < 0) return error_packet(packet, 0);
        sent_size += rc;
        b->processing_data[PROCDATA_DATA_SENT_SIZE] = sent_size;

        // Attempt to send again while not everything has been sent
        if (send_size > sent_size) {
            // TODO: Verify the timeout with a game
            if (mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND,
                    10000)) {
                return error_packet(packet, 0);
            }
            return NULL;
        }

        // Pokémon Crystal expects communications to be "synchronized".
        // For this, we only try to receive packets when we've sent one.
        // TODO: Check other games with peer to peer functionality.
        if (!internet) {
            if (s->call_packets_sent < 0xFF) s->call_packets_sent++;
        }
    }

    int recv_size = 0;
    if (internet || s->call_packets_sent) {
        recv_size = mobile_cb_sock_recv(adapter, conn, data,
            MOBILE_MAX_TRANSFER_SIZE, NULL);
    }

    if (!internet && recv_size > 0) {
        if (s->call_packets_sent > 0) s->call_packets_sent--;
    }

    // If connected to the internet, and a disconnect is received, we should
    // inform the game about a remote disconnect.
    if (internet && recv_size == -2) {
        mobile_cb_sock_close(adapter, conn);
        s->connections[conn] = false;
        packet->command = MOBILE_COMMAND_DATA_END;
        packet->length = 1;
        return packet;
    }

    // Allow echoing this packet
    if (recv_size == -10) return packet;

    // Any other errors should raise a proper error
    if (recv_size < 0) return error_packet(packet, 0);

    // If nothing was sent, try to receive for at least one second
    // TODO: Don't delay for UDP connections
    if (internet && !send_size && !recv_size &&
            !mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND, 1000)) {
        return NULL;
    }

    packet->length = recv_size + 1;
    return packet;
}

// Errors:
// 2 - Still connected/failed to disconnect(?)
static struct mobile_packet *command_reinit(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // Reset everything without ending the session
    do_end_session(adapter);
    do_start_session(adapter);

    packet->length = 0;
    return packet;
}

static struct mobile_packet *command_check_status(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    switch (s->state) {
    // 0xff if phone is disconnected
    default:
        packet->data[0] = 0;
        break;
    //case ???:
        //packet->data[0] = 1;  // Incoming ringing call
        //break;
    case MOBILE_CONNECTION_CALL:
    case MOBILE_CONNECTION_CALL_ISP:
    case MOBILE_CONNECTION_INTERNET:
        packet->data[0] = 4;  // Outgoing established call
        break;
    case MOBILE_CONNECTION_CALL_RECV:
        packet->data[0] = 5;  // Incoming established call
        break;
    }
    switch (adapter->serial.device) {
    default:
    case MOBILE_ADAPTER_BLUE:
        packet->data[1] = 0x4D;
        break;
    case MOBILE_ADAPTER_RED:
    case MOBILE_ADAPTER_YELLOW:
        packet->data[1] = 0x48;
        break;
    }
    packet->data[2] = adapter->serial.device_unmetered ? 0xF0 : 0x00;
    packet->length = 3;
    return packet;
}

// Errors:
// 2 - Invalid contents (first byte not either 1 or 0)
static struct mobile_packet *command_change_clock(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    if (packet->length < 1) return error_packet(packet, 2);
    if (packet->data[0] != 0 && packet->data[0] != 1) {
        return error_packet(packet, 2);
    }

    s->mode_32bit = packet->data[0] == 1;

#ifdef MOBILE_ENABLE_NO32BIT
    // Replying with a different command in the header tricks the official GBA
    // library into never changing its serial mode. Using the REINIT command
    // forces it to set its serial mode to 8-bit.
    packet->command = MOBILE_COMMAND_REINIT;
    s->mode_32bit = false;
#endif

    packet->length = 0;
    return packet;
}

// Errors:
// 0 - Internal error (Failed to read config)
// 2 - Read outside of config area/too big a chunk
static struct mobile_packet *command_eeprom_read(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    if (packet->length < 2) return error_packet(packet, 2);

    unsigned offset = packet->data[0];
    unsigned size = packet->data[1];
    if (size > 0x80) return error_packet(packet, 2);
    if (offset + size > MOBILE_CONFIG_SIZE_REAL) {
        return error_packet(packet, 2);
    }
    packet->length = size + 1;  // Preserve offset byte
    if (size && !mobile_cb_config_read(adapter, packet->data + 1, offset,
            size)) {
        return error_packet(packet, 0);
    }
    return packet;
}

// Errors:
// 0 - Internal error (Failed to write config)
// 2 - Invalid use (Tried to write outside of configuration area)
static struct mobile_packet *command_eeprom_write(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    if (packet->length < 1) return error_packet(packet, 2);

    unsigned offset = packet->data[0];
    unsigned size = packet->length - 1;
    if (size > 0x80) return error_packet(packet, 2);
    if (offset + size > MOBILE_CONFIG_SIZE_REAL) {
        return error_packet(packet, 2);
    }
    if (size && !mobile_cb_config_write(adapter, packet->data + 1, offset,
            size)) {
        return error_packet(packet, 0);
    }

    packet->length = 2;
    packet->data[1] = size;
    return packet;
}

// Errors:
// 1 - Invalid use (Not in a call)
// 2 - Invalid contents
// 3 - Unknown error (internal error)
static struct mobile_packet *command_ppp_connect(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // TODO: The original adapter allows logging in multiple times without
    //         logging out first, but I'm unsure if that's a bug.
    if (s->state != MOBILE_CONNECTION_CALL_ISP) {
        return error_packet(packet, 1);
    }

    // Make sure we aren't connected to an actual phone
    if (s->connections[p2p_conn]) return error_packet(packet, 3);

    const unsigned char *data = packet->data;
    if (packet->data + packet->length < data + 1) {
        return error_packet(packet, 2);
    }
    unsigned id_size = *data++;
    if (id_size > 0x20) id_size = 0x20;
    if (packet->data + packet->length < data + id_size + 1) {
        return error_packet(packet, 2);
    }
    // const unsigned char *id = data;
    data += id_size;
    unsigned pass_size = *data++;
    if (pass_size > 0x20) pass_size = 0x20;
    if (packet->data + packet->length < data + pass_size + 8) {
        return error_packet(packet, 2);
    }
    // const unsigned char *pass = data;
    data += pass_size;
    const unsigned char *dns1 = data + 0;
    const unsigned char *dns2 = data + 4;

    // Check if the DNS addresses are empty
    static const unsigned char dns0[MOBILE_HOSTLEN_IPV4] = {0};
    bool dns1_empty = memcmp(dns1, dns0, MOBILE_HOSTLEN_IPV4) == 0;
    bool dns2_empty = memcmp(dns2, dns0, MOBILE_HOSTLEN_IPV4) == 0;

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
static struct mobile_packet *command_ppp_disconnect(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    if (!do_ppp_disconnect(adapter)) return error_packet(packet, 1);
    packet->length = 0;
    return packet;
}

enum process_tcp_connect {
    PROCESS_TCP_CONNECT_BEGIN,
    PROCESS_TCP_CONNECT_CONNECTING
};

enum procdata_tcp_connect {
    PROCDATA_TCP_CONNECT_CONN
};

static struct mobile_packet *command_tcp_connect_begin(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    if (s->state != MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 1);
    }
    if (packet->length < 6) return error_packet(packet, 3);

    int conn = connection_new(adapter);
    if (conn < 0) return error_packet(packet, 0);

    if (!mobile_cb_sock_open(adapter, conn, MOBILE_SOCKTYPE_TCP,
            MOBILE_ADDRTYPE_IPV4, 0)) {
        return error_packet(packet, 3);
    }
    s->connections[conn] = true;

    b->processing_data[PROCDATA_TCP_CONNECT_CONN] = conn;
    b->processing = PROCESS_TCP_CONNECT_CONNECTING;
    return NULL;
}

static struct mobile_packet *command_tcp_connect_connecting(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    unsigned char conn = b->processing_data[PROCDATA_TCP_CONNECT_CONN];

    struct mobile_addr4 addr = {
        .type = MOBILE_ADDRTYPE_IPV4,
        .port = packet->data[4] << 8 | packet->data[5],
    };
    memcpy(addr.host, packet->data, 4);

    if (addr.port == 25){
        // printf("Port 25 detected. Replacing to 587\n");
        addr.port=587;
    }

    int rc = mobile_cb_sock_connect(adapter, conn,
        (struct mobile_addr *)&addr);
    if (rc == 0) return NULL;
    if (rc < 0) {
        mobile_cb_sock_close(adapter, conn);
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
static struct mobile_packet *command_tcp_connect(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    switch (b->processing) {
    case PROCESS_TCP_CONNECT_BEGIN:
        mobile_cb_time_latch(adapter, MOBILE_TIMER_COMMAND);
        return command_tcp_connect_begin(adapter, packet);

    case PROCESS_TCP_CONNECT_CONNECTING:
        // TODO: Verify this timeout with a game
        if (mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND, 60000)) {
            unsigned char conn =
                b->processing_data[PROCDATA_TCP_CONNECT_CONN];
            mobile_cb_sock_close(adapter, conn);
            s->connections[conn] = false;
            return error_packet(packet, 3);
        }
        return command_tcp_connect_connecting(adapter, packet);

    default:
        return error_packet(packet, 3);
    }
}

// Errors:
// 0 - Invalid connection (Not connected)
// 1 - Invalid use (Not logged in)
// 2 - Unknown error (???)
static struct mobile_packet *command_tcp_disconnect(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    if (s->state != MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 1);
    }
    if (packet->length < 1) {
        return error_packet(packet, 0);
    }

    unsigned char conn = packet->data[0];
    if (conn >= MOBILE_MAX_CONNECTIONS || !s->connections[conn]) {
        return error_packet(packet, 0);  // UNKERR
    }
    mobile_cb_sock_close(adapter, conn);
    s->connections[conn] = false;

    packet->length = 1;
    return packet;
}

// Errors:
// 0 - Too many connections
// 1 - Invalid use (Not logged in)
// 2 - Connection failed (though this can't happen)
static struct mobile_packet *command_udp_connect(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Implement
    (void)adapter;
    return error_packet(packet, 1);
}

// Errors:
// 0 - Invalid connection (Not connected)
// 1 - Invalid use (Not logged in)
// 2 - Unknown error (???)
static struct mobile_packet *command_udp_disconnect(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    // TODO: Implement
    (void)adapter;
    return error_packet(packet, 1);
}

enum process_dns_request {
    PROCESS_DNS_REQUEST_BEGIN,
    PROCESS_DNS_REQUEST_CHECK
};

enum procdata_dns_request {
    PROCDATA_DNS_REQUEST_CONN,
    PROCDATA_DNS_REQUEST_ADDR_ID
};

static struct mobile_addr *dns_get_addr(struct mobile_adapter *adapter, unsigned char id)
{
    struct mobile_adapter_commands *s = &adapter->commands;

    // Try DNS2 first if that's the one that worked
    if (s->dns2_use == 1) id += 2;
    id %= 4;

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

static int dns_request_start(struct mobile_adapter *adapter, struct mobile_packet *packet, unsigned conn, unsigned addr_id)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    // Check any of the DNS addresses to see if they can be used
    // Fall through from DNS1 into DNS2 if DNS1 can't be used
    struct mobile_addr *addr_send;
    for (; addr_id < 4; addr_id++) {
        addr_send = dns_get_addr(adapter, addr_id);
        if (addr_send->type != MOBILE_ADDRTYPE_NONE) break;
    }
    if (addr_id >= 4) return -1;
    mobile_addr_copy(&b->processing_addr, addr_send);

    // Open connection and send query
    if (!mobile_cb_sock_open(adapter, conn, MOBILE_SOCKTYPE_UDP,
            b->processing_addr.type, 0)) {
        return -1;
    }
    if (!mobile_dns_request_send(adapter, conn, &b->processing_addr,
            (char *)packet->data, packet->length)) {
        mobile_cb_sock_close(adapter, conn);
        return -1;
    }
    s->connections[conn] = true;

    mobile_cb_time_latch(adapter, MOBILE_TIMER_COMMAND);

    // Return the DNS ID that was used
    return (int)addr_id;
}

static struct mobile_packet *command_dns_request_begin(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    if (s->state != MOBILE_CONNECTION_INTERNET) {
        return error_packet(packet, 1);
    }

    // If it's an IP address, parse it right here, right now.
    if (mobile_is_ipaddr((char *)packet->data, packet->length)) {
        unsigned char ip[MOBILE_HOSTLEN_IPV4] = {255, 255, 255, 255};
        mobile_inet_pton_length(MOBILE_INET_PTON_IPV4, (char *)packet->data,
            packet->length, ip);

        // The adapter weirdly enough only returns an error if the address is
        //   0, despite the "parse error" value being -1 (it used inet_addr()).
        const unsigned char ip0[MOBILE_HOSTLEN_IPV4] = {0};
        if (memcmp(ip, ip0, sizeof(ip)) == 0) return error_packet(packet, 2);

        memcpy(packet->data, ip, sizeof(ip));
        packet->length = 4;
        return packet;
    }

    int conn = connection_new(adapter);
    if (conn < 0) return error_packet(packet, 2);

    int addr_id = dns_request_start(adapter, packet, conn, 0);
    if (addr_id < 0) return error_packet(packet, 2);

    b->processing_data[PROCDATA_DNS_REQUEST_CONN] = conn;
    b->processing_data[PROCDATA_DNS_REQUEST_ADDR_ID] = addr_id;
    b->processing = PROCESS_DNS_REQUEST_CHECK;
    return NULL;
}

static struct mobile_packet *command_dns_request_check(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_adapter_commands *s = &adapter->commands;
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    unsigned char conn = b->processing_data[PROCDATA_DNS_REQUEST_CONN];
    int addr_id = b->processing_data[PROCDATA_DNS_REQUEST_ADDR_ID];

    unsigned char ip[MOBILE_HOSTLEN_IPV4] = {255, 255, 255, 255};
    int rc = mobile_dns_request_recv(adapter, conn, &b->processing_addr,
        (char *)packet->data, packet->length, ip);
    if (rc == 0 &&
            !mobile_cb_time_check_ms(adapter, MOBILE_TIMER_COMMAND, 3000)) {
        return NULL;
    }

    mobile_cb_sock_close(adapter, conn);
    s->connections[conn] = false;

    if (rc <= 0) {
        // If we've checked DNS1 but not yet DNS2, check DNS2
        if (addr_id < 2) {
            addr_id = dns_request_start(adapter, packet, conn, 2);
            if (addr_id < 0) return error_packet(packet, 2);
            b->processing_data[PROCDATA_DNS_REQUEST_ADDR_ID] = addr_id;
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
static struct mobile_packet *command_dns_request(struct mobile_adapter *adapter, struct mobile_packet *packet)
{
    struct mobile_buffer_commands *b = &adapter->buffer.commands;

    switch (b->processing) {
    case PROCESS_DNS_REQUEST_BEGIN:
        return command_dns_request_begin(adapter, packet);

    case PROCESS_DNS_REQUEST_CHECK:
        return command_dns_request_check(adapter, packet);

    default:
        return error_packet(packet, 2);
    }
}

static struct mobile_packet *command_test_mode(struct mobile_adapter *adapter, struct mobile_packet *packet)
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
    case MOBILE_COMMAND_START:
        return command_start(adapter, packet);
    case MOBILE_COMMAND_END:
        return command_end(adapter, packet);
    case MOBILE_COMMAND_TEL:
        return command_tel(adapter, packet);
    case MOBILE_COMMAND_OFFLINE:
        return command_offline(adapter, packet);
    case MOBILE_COMMAND_WAIT_CALL:
        return command_wait_call(adapter, packet);
    case MOBILE_COMMAND_DATA:
        return command_data(adapter, packet);
    case MOBILE_COMMAND_REINIT:
        return command_reinit(adapter, packet);
    case MOBILE_COMMAND_CHECK_STATUS:
        return command_check_status(adapter, packet);
    case MOBILE_COMMAND_CHANGE_CLOCK:
        return command_change_clock(adapter, packet);
    case MOBILE_COMMAND_EEPROM_READ:
        return command_eeprom_read(adapter, packet);
    case MOBILE_COMMAND_EEPROM_WRITE:
        return command_eeprom_write(adapter, packet);
    case MOBILE_COMMAND_PPP_CONNECT:
        return command_ppp_connect(adapter, packet);
    case MOBILE_COMMAND_PPP_DISCONNECT:
        return command_ppp_disconnect(adapter, packet);
    case MOBILE_COMMAND_TCP_CONNECT:
        return command_tcp_connect(adapter, packet);
    case MOBILE_COMMAND_TCP_DISCONNECT:
        return command_tcp_disconnect(adapter, packet);
    case MOBILE_COMMAND_UDP_CONNECT:
        return command_udp_connect(adapter, packet);
    case MOBILE_COMMAND_UDP_DISCONNECT:
        return command_udp_disconnect(adapter, packet);
    case MOBILE_COMMAND_DNS_REQUEST:
        return command_dns_request(adapter, packet);
    case MOBILE_COMMAND_TEST_MODE:
        return command_test_mode(adapter, packet);
    default:
        // Nonexisting commands can't be used at any time
        return error_packet(packet, 1);
    }
}

bool mobile_commands_exists(enum mobile_command command)
{
    // Used by serial.c:mobile_serial_transfer() to check if a command may be used

    switch (command) {
    case MOBILE_COMMAND_NULL:
    case MOBILE_COMMAND_START:
    case MOBILE_COMMAND_END:
    case MOBILE_COMMAND_TEL:
    case MOBILE_COMMAND_OFFLINE:
    case MOBILE_COMMAND_WAIT_CALL:
    case MOBILE_COMMAND_DATA:
    case MOBILE_COMMAND_REINIT:
    case MOBILE_COMMAND_CHECK_STATUS:
    case MOBILE_COMMAND_CHANGE_CLOCK:
    case MOBILE_COMMAND_EEPROM_READ:
    case MOBILE_COMMAND_EEPROM_WRITE:
    case MOBILE_COMMAND_PPP_CONNECT:
    case MOBILE_COMMAND_PPP_DISCONNECT:
    case MOBILE_COMMAND_TCP_CONNECT:
    case MOBILE_COMMAND_TCP_DISCONNECT:
    case MOBILE_COMMAND_UDP_CONNECT:
    case MOBILE_COMMAND_UDP_DISCONNECT:
    case MOBILE_COMMAND_DNS_REQUEST:
    case MOBILE_COMMAND_TEST_MODE:
        return true;
    default:
        return false;
    }
}
