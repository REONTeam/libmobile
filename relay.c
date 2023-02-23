// SPDX-License-Identifier: LGPL-3.0-or-later
#include "relay.h"

#include <string.h>

#include "mobile_data.h"
#include "compat.h"
#include "callback.h"

// Protocol description:
//
// The relay protocol can hook up two separate adapters, and create a link
// between them, across the internet. Much like protocols like SOCKS, the
// protocol follows a simple request-response model. Once the connection
// between adapters is established, the server acts as a "tunnel" between the
// devices, and relays all data between them.
//
// To be able to connect different users, each adapter is assigned a "phone
// number" of sorts by the server. Alongside this number, an authentication
// token is generated, which may be kept secret by the client to keep the
// assigned number across multiple connections and application restarts. The
// phone numbers are expected to be exchanged between users.

#define PROTOCOL_VERSION 0

// Maximum number size
#define MOBILE_RELAY_MAX_NUMBER_SIZE 16
static_assert(MOBILE_MAX_NUMBER_SIZE >= MOBILE_RELAY_MAX_NUMBER_SIZE,
    "MOBILE_MAX_NUMBER_SIZE isn't big enough!");

// Maximum packet sizes
//#define MAX_HANDSHAKE_SIZE (7 + 1 + MOBILE_RELAY_TOKEN_SIZE)  // 24
//#define MAX_COMMAND_CALL_SIZE (3 + MOBILE_RELAY_MAX_NUMBER_SIZE)  // 19
//#define MAX_COMMAND_WAIT_SIZE (4 + MOBILE_RELAY_MAX_NUMBER_SIZE)  // 20
//#define MAX_COMMAND_GET_NUMBER_SIZE (3 + MOBILE_RELAY_MAX_NUMBER_SIZE)  // 19
static_assert(MOBILE_RELAY_PACKET_SIZE >= 24,
    "MOBILE_RELAY_PACKET_SIZE isn't big enough!");

static const unsigned char handshake_magic[] PROGMEM = {
    PROTOCOL_VERSION,
    'M', 'O', 'B', 'I', 'L', 'E',
};

void mobile_relay_init(struct mobile_adapter *adapter)
{
    adapter->relay.state = MOBILE_RELAY_DISCONNECTED;
    adapter->relay.processing = 0;
}

static void relay_recv_reset(struct mobile_adapter *adapter) {
    adapter->relay.buffer_len = 0;
}

// Makes sure at least size bytes have been received, tries to read more if not.
// Returns requested size if bytes are available, 0 if not enough bytes have
//   been received, and -1 if an error occurred.
static int relay_recv(struct mobile_adapter *adapter, unsigned conn, unsigned size)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    if (size > MOBILE_RELAY_PACKET_SIZE) return -1;
    if (s->buffer_len >= size) return (int)size;

    int recv = mobile_cb_sock_recv(adapter, conn, s->buffer + s->buffer_len,
        size - s->buffer_len, NULL);
    if (recv < 0) return -1;
    s->buffer_len += recv;
    if (s->buffer_len < size) return 0;

    return (int)size;
}

static void debug_prefix(struct mobile_adapter *adapter)
{
    mobile_debug_print(adapter, PSTR("<RELAY> "));
}

static void relay_handshake_send_debug(struct mobile_adapter *adapter)
{
    debug_prefix(adapter);
    mobile_debug_print(adapter, PSTR("Authenticating"));
    if (adapter->config.relay_token_init) {
#ifdef NDEBUG
        mobile_debug_print(adapter, PSTR(" (with token)"));
#else
        mobile_debug_print(adapter, PSTR(" (with token: "));
        mobile_debug_print_hex(adapter, adapter->config.relay_token, 4);
        mobile_debug_print(adapter, PSTR("...)"));
#endif
    } else {
        mobile_debug_print(adapter, PSTR(" (without token)"));
    }
    mobile_debug_endl(adapter);
}

static bool relay_handshake_send(struct mobile_adapter *adapter, unsigned char conn)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    unsigned buffer_len = sizeof(handshake_magic) + 1;
    memcpy_P(s->buffer, handshake_magic, sizeof(handshake_magic));

    unsigned char *auth = s->buffer + sizeof(handshake_magic);
    auth[0] = mobile_config_get_relay_token(adapter, auth + 1);
    if (auth[0]) buffer_len += MOBILE_RELAY_TOKEN_SIZE;

    return mobile_cb_sock_send(adapter, conn, s->buffer, buffer_len, NULL);
}

static void relay_handshake_recv_debug(struct mobile_adapter *adapter)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    debug_prefix(adapter);
    mobile_debug_print(adapter, PSTR("Logged in"));
    unsigned char *auth = s->buffer + sizeof(handshake_magic);
    if (auth[0] == 1) {
#ifdef NDEBUG
        mobile_debug_print(adapter, PSTR(" (new token)"));
#else
        mobile_debug_print(adapter, PSTR(" (new token: "));
        mobile_debug_print_hex(adapter, auth + 1, 4);
        mobile_debug_print(adapter, PSTR("...)"));
#endif
    }
    mobile_debug_endl(adapter);
}

static int relay_handshake_recv(struct mobile_adapter *adapter, unsigned char conn)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    unsigned recv_size = sizeof(handshake_magic) + 1;
    int recv = relay_recv(adapter, conn, recv_size);
    if (recv <= 0) return recv;

    if (memcmp_P(s->buffer, handshake_magic, sizeof(handshake_magic)) != 0) {
        return -1;
    }

    unsigned char *auth = s->buffer + sizeof(handshake_magic);
    if (auth[0] == 0) {
        return 1;
    } else if (auth[0] == 1) {
        recv_size += MOBILE_RELAY_TOKEN_SIZE;
        int recv = relay_recv(adapter, conn, recv_size);
        if (recv <= 0) return recv;

        mobile_config_set_relay_token(adapter, auth + 1);
        return 2;
    } else {
        return -1;
    }
}

static void relay_call_send_debug(struct mobile_adapter *adapter, const char *number, unsigned number_len)
{
    debug_prefix(adapter);
    mobile_debug_print(adapter, PSTR("Command: CALL "));
    mobile_debug_write(adapter, number, number_len);
    mobile_debug_endl(adapter);
}

static bool relay_call_send(struct mobile_adapter *adapter, unsigned char conn, const char *number, unsigned number_len)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    if (number_len > MOBILE_RELAY_MAX_NUMBER_SIZE) return false;
    unsigned buffer_len = 3 + number_len;
    s->buffer[0] = PROTOCOL_VERSION;
    s->buffer[1] = MOBILE_RELAY_COMMAND_CALL;
    s->buffer[2] = number_len;
    memcpy(s->buffer + 3, number, number_len);

    return mobile_cb_sock_send(adapter, conn, s->buffer, buffer_len, NULL);
}

static void relay_call_recv_debug(struct mobile_adapter *adapter)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    debug_prefix(adapter);
    switch (s->buffer[2] + 1) {
    case MOBILE_RELAY_CALL_RESULT_ACCEPTED:
        mobile_debug_print(adapter, PSTR("ACCEPTED"));
        break;
    case MOBILE_RELAY_CALL_RESULT_INTERNAL:
        mobile_debug_print(adapter, PSTR("Error: INTERNAL"));
        break;
    case MOBILE_RELAY_CALL_RESULT_BUSY:
        mobile_debug_print(adapter, PSTR("Error: BUSY"));
        break;
    case MOBILE_RELAY_CALL_RESULT_UNAVAILABLE:
        mobile_debug_print(adapter, PSTR("Error: UNAVAILABLE"));
        break;
    }
    mobile_debug_endl(adapter);
}

static int relay_call_recv(struct mobile_adapter *adapter, unsigned char conn)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    int recv = relay_recv(adapter, conn, 3);
    if (recv <= 0) return recv;

    if (s->buffer[0] != PROTOCOL_VERSION) return -1;
    if (s->buffer[1] != MOBILE_RELAY_COMMAND_CALL) return -1;
    int result = s->buffer[2] + 1;
    if (result >= MOBILE_RELAY_MAX_CALL_RESULT) return -1;

    return result;
}

static void relay_wait_send_debug(struct mobile_adapter *adapter)
{
    debug_prefix(adapter);
    mobile_debug_print(adapter, PSTR("Command: WAIT"));
    mobile_debug_endl(adapter);
}

static bool relay_wait_send(struct mobile_adapter *adapter, unsigned char conn)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    unsigned buffer_len = 2;
    s->buffer[0] = PROTOCOL_VERSION;
    s->buffer[1] = MOBILE_RELAY_COMMAND_WAIT;

    return mobile_cb_sock_send(adapter, conn, s->buffer, buffer_len, NULL);
}

static void relay_wait_recv_debug(struct mobile_adapter *adapter)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    debug_prefix(adapter);
    switch (s->buffer[2] + 1) {
    case MOBILE_RELAY_WAIT_RESULT_ACCEPTED:
        mobile_debug_print(adapter, PSTR("ACCEPTED "));
        mobile_debug_write(adapter, (char *)s->buffer + 4, s->buffer[3]);
        break;
    case MOBILE_RELAY_WAIT_RESULT_INTERNAL:
        mobile_debug_print(adapter, PSTR("Error: INTERNAL"));
        break;
    }
    mobile_debug_endl(adapter);
}

static int relay_wait_recv(struct mobile_adapter *adapter, unsigned char conn, char *number, unsigned *number_len)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    unsigned recv_size = 4;
    int recv = relay_recv(adapter, conn, recv_size);
    if (recv <= 0) return recv;

    if (s->buffer[0] != PROTOCOL_VERSION) return -1;
    if (s->buffer[1] != MOBILE_RELAY_COMMAND_WAIT) return -1;
    int result = s->buffer[2] + 1;
    if (result >= MOBILE_RELAY_MAX_WAIT_RESULT) return -1;

    unsigned _number_len = s->buffer[3];
    if (_number_len == 0 || _number_len > MOBILE_RELAY_MAX_NUMBER_SIZE) {
        return -1;
    }

    recv_size += _number_len;
    recv = relay_recv(adapter, conn, recv_size);
    if (recv <= 0) return recv;
    memcpy(number, s->buffer + 4, _number_len);
    *number_len = _number_len;

    return result;
}

static void relay_get_number_send_debug(struct mobile_adapter *adapter)
{
    debug_prefix(adapter);
    mobile_debug_print(adapter, PSTR("Command: GET_NUMBER"));
    mobile_debug_endl(adapter);
}

static bool relay_get_number_send(struct mobile_adapter *adapter, unsigned char conn)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    unsigned buffer_len = 2;
    s->buffer[0] = PROTOCOL_VERSION;
    s->buffer[1] = MOBILE_RELAY_COMMAND_GET_NUMBER;

    return mobile_cb_sock_send(adapter, conn, s->buffer, buffer_len, NULL);
}

static void relay_get_number_recv_debug(struct mobile_adapter *adapter)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    debug_prefix(adapter);
    mobile_debug_print(adapter, PSTR("Number: "));
    mobile_debug_write(adapter, (char *)s->buffer + 3, s->buffer[2]);
    mobile_debug_endl(adapter);
}

static int relay_get_number_recv(struct mobile_adapter *adapter, unsigned char conn, char *number, unsigned *number_len)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    unsigned recv_size = 3;
    int recv = relay_recv(adapter, conn, recv_size);
    if (recv <= 0) return recv;

    if (s->buffer[0] != PROTOCOL_VERSION) return -1;
    if (s->buffer[1] != MOBILE_RELAY_COMMAND_GET_NUMBER) return -1;

    unsigned _number_len = s->buffer[2];
    if (_number_len == 0 || _number_len > MOBILE_RELAY_MAX_NUMBER_SIZE) {
        return -1;
    }

    recv_size += _number_len;
    recv = relay_recv(adapter, conn, recv_size);
    if (recv <= 0) return recv;
    memcpy(number, s->buffer + 3, _number_len);
    *number_len = _number_len;

    return 1;
}

// mobile_relay_connect - Connect to and authenticate with the relay server
//
// Sends the authentication token to recover the adapter's phone number. If
// the adapter has no authentication token, a new one will be generated.
//
// The server may also send a new authentication token to replace the existing
// one at its discretion.
//
// Returns: -1 on error, 0 if processing, 1 on success/already connected
int mobile_relay_connect(struct mobile_adapter *adapter, unsigned char conn, const struct mobile_addr *server)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    int rc;

    switch (s->state) {
    case MOBILE_RELAY_DISCONNECTED:
        debug_prefix(adapter);
        mobile_debug_print(adapter, PSTR("Connecting to "));
        mobile_debug_print_addr(adapter, server);
        mobile_debug_endl(adapter);
        s->state = MOBILE_RELAY_RECV_CONNECT;
        // fallthrough

    case MOBILE_RELAY_RECV_CONNECT:
        rc = mobile_cb_sock_connect(adapter, conn, server);
        if (rc == 0) return 0;
        if (rc < 0) {
            debug_prefix(adapter);
            mobile_debug_print(adapter, PSTR("Connection failed"));
            mobile_debug_endl(adapter);
            s->state = MOBILE_RELAY_DISCONNECTED;
            return -1;
        }

        relay_handshake_send_debug(adapter);
        if (!relay_handshake_send(adapter, conn)) return -1;
        relay_recv_reset(adapter);
        s->state = MOBILE_RELAY_RECV_HANDSHAKE;
        return 0;

    case MOBILE_RELAY_RECV_HANDSHAKE:
        rc = relay_handshake_recv(adapter, conn);
        if (rc == 0) return 0;
        if (rc < 0) {
            debug_prefix(adapter);
            mobile_debug_print(adapter, PSTR("Authentication failed"));
            mobile_debug_endl(adapter);
            s->state = MOBILE_RELAY_DISCONNECTED;
            return -1;
        }
        relay_handshake_recv_debug(adapter);
        s->state = MOBILE_RELAY_CONNECTED;
        return 1;

    case MOBILE_RELAY_CONNECTED:
    default:
        return 1;
    }
}

// mobile_relay_call - Call a phone number and establish a connection
//
// Calls another adapter, and waits for it to either pick up or reject the
// call. Once the call is accepted, the adapters are linked, and any further
// data will be relayed directly to the other adapter.
//
// Parameters:
// - number: ASCII string containing the number
// - number_len: Length of the string (max MOBILE_RELAY_MAX_NUMBER_SIZE)
// Returns: enum mobile_relay_call_result value
int mobile_relay_call(struct mobile_adapter *adapter, unsigned char conn, const char *number, unsigned number_len)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    int rc;

    switch (s->state) {
    case MOBILE_RELAY_CONNECTED:
        relay_call_send_debug(adapter, number, number_len);
        if (!relay_call_send(adapter, conn, number, number_len)) return -1;
        relay_recv_reset(adapter);
        s->state = MOBILE_RELAY_RECV_CALL;
        return 0;

    case MOBILE_RELAY_RECV_CALL:
        rc = relay_call_recv(adapter, conn);
        if (rc == 0) return 0;
        if (rc < 0) {
            debug_prefix(adapter);
            mobile_debug_print(adapter, PSTR("Call failed"));
            mobile_debug_endl(adapter);
            s->state = MOBILE_RELAY_CONNECTED;
            return -1;
        }

        relay_call_recv_debug(adapter);
        if (rc != MOBILE_RELAY_CALL_RESULT_ACCEPTED) {
            s->state = MOBILE_RELAY_CONNECTED;
            return rc;
        }
        s->state = MOBILE_RELAY_LINKED;
        return rc;

    case MOBILE_RELAY_LINKED:
        return 1;

    default:
        return -1;
    }
}

// mobile_relay_wait - Wait for a telephone call
//
// Starts waiting for a telephone call. If the server replies with a correct
// packet, the connection is established and the adapters are linked.
//
// Once this function is called, you won't be able to execute any other
// functions unless the connection is closed and restarted from scratch.
//
// Parameters:
// - number: Buffer to copy the number into
// - number_len: Pointer to resulting size of the number
// Returns: enum mobile_relay_wait_result value
int mobile_relay_wait(struct mobile_adapter *adapter, unsigned char conn, char *number, unsigned *number_len)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    int rc;

    switch (s->state) {
    case MOBILE_RELAY_CONNECTED:
        relay_wait_send_debug(adapter);
        if (!relay_wait_send(adapter, conn)) return -1;
        relay_recv_reset(adapter);
        s->state = MOBILE_RELAY_RECV_WAIT;
        return 0;

    case MOBILE_RELAY_RECV_WAIT:
        rc = relay_wait_recv(adapter, conn, number, number_len);
        if (rc == 0) return 0;
        if (rc < 0) {
            debug_prefix(adapter);
            mobile_debug_print(adapter, PSTR("Wait failed"));
            mobile_debug_endl(adapter);
            s->state = MOBILE_RELAY_CONNECTED;
            return -1;
        }

        relay_wait_recv_debug(adapter);
        if (rc != MOBILE_RELAY_WAIT_RESULT_ACCEPTED) {
            s->state = MOBILE_RELAY_CONNECTED;
            return rc;
        }
        s->state = MOBILE_RELAY_LINKED;
        return rc;

    case MOBILE_RELAY_LINKED:
        return 1;

    default:
        return -1;
    }
}

// mobile_relay_get_number - Get current phone number
//
// Queries the server to get the current adapter's phone number. The result is
// an ASCII string containing at most MOBILE_RELAY_MAX_NUMBER_SIZE characters.
//
// The result buffer must be big enough to contain a full-sized number.
//
// Parameters:
// - number: Buffer to copy the number into
// - number_len: Pointer to resulting size of the number
// Returns: -1 on error, 0 if processing, 1 on success
int mobile_relay_get_number(struct mobile_adapter *adapter, unsigned char conn, char *number, unsigned *number_len)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    int rc;

    switch (s->state) {
    case MOBILE_RELAY_CONNECTED:
        relay_get_number_send_debug(adapter);
        if (!relay_get_number_send(adapter, conn)) return -1;
        relay_recv_reset(adapter);
        s->state = MOBILE_RELAY_RECV_GET_NUMBER;
        return 0;

    case MOBILE_RELAY_RECV_GET_NUMBER:
        rc = relay_get_number_recv(adapter, conn, number, number_len);
        if (rc == 0) return 0;
        if (rc < 0) {
            debug_prefix(adapter);
            mobile_debug_print(adapter, PSTR("Get number failed"));
            mobile_debug_endl(adapter);
            s->state = MOBILE_RELAY_CONNECTED;
            return -1;
        }
        relay_get_number_recv_debug(adapter);
        s->state = MOBILE_RELAY_CONNECTED;
        return 1;

    default:
        return -1;
    }
}

enum process_call {
    PROCESS_CALL_BEGIN,
    PROCESS_CALL_GET_NUMBER,
    PROCESS_CALL_CALL
};

// mobile_relay_proc_call - Stateful outgoing call procedure
int mobile_relay_proc_call(struct mobile_adapter *adapter, unsigned char conn, const struct mobile_addr *server, const char *number, unsigned number_len)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    char _number[MOBILE_RELAY_MAX_NUMBER_SIZE + 1];
    unsigned _number_len;

    int rc = -1;

    switch (s->processing) {
    case PROCESS_CALL_BEGIN:
        rc = mobile_relay_connect(adapter, conn, server);
        if (rc <= 0) break;

        s->processing = PROCESS_CALL_GET_NUMBER;
        // fallthrough

    case PROCESS_CALL_GET_NUMBER:
        rc = mobile_relay_get_number(adapter, conn, _number, &_number_len);
        if (rc <= 0) break;

        _number[_number_len] = '\0';
        mobile_cb_update_number(adapter, MOBILE_NUMBER_USER, _number);

        s->processing = PROCESS_CALL_CALL;
        // fallthrough

    case PROCESS_CALL_CALL:
        rc = mobile_relay_call(adapter, conn, number, number_len);
        if (rc == MOBILE_RELAY_CALL_RESULT_ACCEPTED) {
            // NOTE: mobile_relay_call checks max number length
            _number_len = number_len;
            memcpy(_number, number, _number_len);

            _number[_number_len] = '\0';
            mobile_cb_update_number(adapter, MOBILE_NUMBER_PEER, _number);
        }
    }

    return rc;
}

enum process_wait {
    PROCESS_WAIT_BEGIN,
    PROCESS_WAIT_GET_NUMBER,
    PROCESS_WAIT_WAIT
};

// mobile_relay_proc_wait - Stateful incoming call procedure
int mobile_relay_proc_wait(struct mobile_adapter *adapter, unsigned char conn, const struct mobile_addr *server)
{
    struct mobile_adapter_relay *s = &adapter->relay;

    char _number[MOBILE_RELAY_MAX_NUMBER_SIZE + 1];
    unsigned _number_len;

    int rc = -1;

    switch (s->processing) {
    case PROCESS_WAIT_BEGIN:
        rc = mobile_relay_connect(adapter, conn, server);
        if (rc <= 0) break;

        s->processing = PROCESS_WAIT_GET_NUMBER;
        // fallthrough

    case PROCESS_WAIT_GET_NUMBER:
        rc = mobile_relay_get_number(adapter, conn, _number, &_number_len);
        if (rc <= 0) break;

        _number[_number_len] = '\0';
        mobile_cb_update_number(adapter, MOBILE_NUMBER_USER, _number);

        s->processing = PROCESS_WAIT_WAIT;
        // fallthrough

    case PROCESS_WAIT_WAIT:
        rc = mobile_relay_wait(adapter, conn, _number, &_number_len);
        if (rc == MOBILE_RELAY_WAIT_RESULT_ACCEPTED) {
            _number[_number_len] = '\0';
            mobile_cb_update_number(adapter, MOBILE_NUMBER_PEER, _number);
        }
    }

    return rc;
}
