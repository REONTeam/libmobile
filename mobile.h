// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Public interface for libmobile
// Any other header should be considered private, so please don't use any
//   functions or constants from other headers in a program.

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct mobile_adapter;  // data.h
struct mobile_packet;  // commands.h

// Limits any user of this library should abide by
#define MOBILE_CONFIG_SIZE 0xC0
#define MOBILE_MAX_CONNECTIONS 2
#define MOBILE_MAX_TIMERS 4
#define MOBILE_MAX_TRANSFER_SIZE 0xFE  // MOBILE_MAX_DATA_SIZE - 1

enum mobile_adapter_device {
    // The clients.
    MOBILE_ADAPTER_GAMEBOY,
    MOBILE_ADAPTER_GAMEBOY_ADVANCE,

    // The adapters, this is what we're emulating.
    MOBILE_ADAPTER_BLUE = 8,
    MOBILE_ADAPTER_YELLOW,
    MOBILE_ADAPTER_GREEN,
    MOBILE_ADAPTER_RED,
};

enum mobile_action {
    MOBILE_ACTION_NONE,
    MOBILE_ACTION_PROCESS_COMMAND,
    MOBILE_ACTION_CHANGE_32BIT_MODE,
    MOBILE_ACTION_DROP_CONNECTION,
    MOBILE_ACTION_RESET,
    MOBILE_ACTION_RESET_SERIAL,
};

enum mobile_timers {
    MOBILE_TIMER_SERIAL,
    MOBILE_TIMER_COMMAND,
    MOBILE_TIMER_RESERVED3,
    MOBILE_TIMER_RESERVED4,
};

enum mobile_socktype {
    MOBILE_SOCKTYPE_TCP,
    MOBILE_SOCKTYPE_UDP,
};

enum mobile_addrtype {
    MOBILE_ADDRTYPE_NONE,
    MOBILE_ADDRTYPE_IPV4,
    MOBILE_ADDRTYPE_IPV6,
};

#define MOBILE_HOSTLEN_IPV4 4
#define MOBILE_HOSTLEN_IPV6 16

struct mobile_addr4 {
    enum mobile_addrtype type;
    unsigned port;
    unsigned char host[MOBILE_HOSTLEN_IPV4];
};

struct mobile_addr6 {
    enum mobile_addrtype type;
    unsigned port;
    unsigned char host[MOBILE_HOSTLEN_IPV6];
};

struct mobile_addr {
    // Make sure it's big enough to hold all types
    union {
        enum mobile_addrtype type;

        // Don't access these directly, cast instead.
        struct mobile_addr4 _addr4;
        struct mobile_addr6 _addr6;
    };
};

struct mobile_adapter_config {
    enum mobile_adapter_device device;
    unsigned p2p_port;

    // Signals Pokémon Crystal (jp) that the connection isn't metered,
    //   removing the time limit in mobile battles.
    // We have no idea of the effects of this in other games.
    bool unmetered;

    struct mobile_addr dns1;
    struct mobile_addr dns2;
};
#define MOBILE_ADAPTER_CONFIG_DEFAULT (struct mobile_adapter_config){ \
    .device = MOBILE_ADAPTER_BLUE, \
    .p2p_port = 1027, \
}

// Data in this header depends on the config/types above
#ifndef MOBILE_INTERNAL
#include "data.h"
#endif

// Board-specific function prototypes (make sure these are defined elsewhere!)

// mobile_board_serial_disable - Disable serial communications
//
// This function must ensure nothing will call mobile_transfer() while the
// serial communcations are disabled, and the current byte transfer state is
// reset, in case a byte was partially transferred at the time this function
// was called. If mobile_transfer() and mobile_loop() are implemented as
// separate threads, a mutex-like locking mechanism may be used to accomplish
// this.
void mobile_board_serial_disable(void *user);

// mobile_board_serial_enable - Enable serial communications
//
// Exact opposite of mobile_board_serial_disable(). This function indicates
// mobile_transfer() may be called again, resuming communications.
void mobile_board_serial_enable(void *user);

// mobile_board_debug_cmd - Packet receive/send callback
//
// Called whenever a packet is received or sent, to allow the implementation to
// output debugging information.
//
// This is currently implemented in debug_cmd.h and it's discouraged to
// implement this independently, as this function is slated for replacement
// with one that simply passes along debug messages.
void mobile_board_debug_cmd(void *user, int send, const struct mobile_packet *packet);

// mobile_board_config_read - Read from the configuration data
//
// The mobile adapter is able to store configuration data. This function may
// read from it. Use MOBILE_CONFIG_SIZE to determine the size of this data.
//
// libmobile will never request any data outside of the MOBILE_CONFIG_SIZE
// area, so boundary checks aren't necessary if enough space is allocated.
//
// Parameters:
// - dest: Destination buffer where <size> bytes will be written to
// - offset: Configuration data offset to read from
// - size: Amount of data in bytes to be read
// Returns: true on success, false if the configuration can't be read
bool mobile_board_config_read(void *user, void *dest, uintptr_t offset, size_t size);

// mobile_board_config_write - Write to the configuration data
//
// Analogue of mobile_board_config_read(), writing to the configuration data
// instead. The same information applies here as well.
//
// Parameters:
// - src: Source buffer where <size> bytes will be read from
// - offset: Configuration data offset to write to
// - size: Amount of data in bytes to write
// Returns: true on success, false if the configuration can't be written
bool mobile_board_config_write(void *user, const void *src, uintptr_t offset, size_t size);

// mobile_board_time_latch - Latch a timer
//
// Timers are used to keep track of time, allowing libmobile to implement
// timeouts and other time-related mechanisms. The time tracked must reflect
// the same measurement the connected gameboy is using. This means that when an
// emulated gameboy is sped up, the timer will move faster as well. If
// connected to a real-life gameboy, this will track real time. An
// implementation must be able to track MOBILE_MAX_TIMERS amount of timers, and
// all timers must be treated exactly the same.  Timers must be able to keep
// track of at least 60 seconds, with millisecond precision, preferably with
// little to no time skew.
//
// This function will "latch" the current time to the specified timer, by
// storing the current value so it may later be compared.
// 
// Parameters:
// - timer: timer that should be latched
void mobile_board_time_latch(void *user, enum mobile_timers timer);

// mobile_board_time_check_ms - Check if a certain amount of time has passed
//
// Checks if a specified amount of milliseconds has passed since a timer has
// been latched by mobile_board_time_latch(). The notes on that function apply
// here as well.
//
// Checking a timer that hasn't been latched is undefined, libmobile shall never
// do this.
//
// Returns: true if the specified time has passed, false otherwise
// Parameters:
// - timer: timer that should be compared against
// - ms: amount of milliseconds that should be compared with
bool mobile_board_time_check_ms(void *user, enum mobile_timers timer, unsigned ms);

// mobile_board_sock_open - Open a socket
//
// Creates a socket of the specified type and address type. The available
// socket types are TCP and UDP, and address types are IPV4 and IPV6. Both
// socket types must be supported, and IPV4 must be supported, but IPV6 support
// is optional, and the implementation is allowed to fail with that.
//
// The port to use for the socket is specified through the <bindport>
// parameter, where port 0 means a random port. Depending on the system, the
// SO_REUSEADDR option must be set, in order to avoid not being able to bind an
// otherwise unused port. The <conn> parameter indicates the selected socket
// that should be opened, of which there are at most MOBILE_MAX_CONNECTIONS.
//
// Since non-blocking operations will be required for different socket-related
// functions, enabling non-blocking mode on this socket might be necessary.
//
// Opening a socket that hasn't been closed is undefined and may produce an
// error or terminate the program, libmobile shall never do this. Similarly,
// using an unopened socket with any of the mobile_board_sock_* functions may
// result in the same.
//
// Returns: true if socket was created successfully, false on error
// Parameters:
// - conn: Socket number
// - type: TCP vs UDP
// - addrtype: IPV4 vs IPV6
// - bindport: Port to bind()
bool mobile_board_sock_open(void *user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport);

// mobile_board_sock_close - Close a socket
//
// Closes a socket opened through mobile_board_sock_open().
//
// Closing a socket that hasn't been opened is undefined and may produce an
// error or terminate the program, libmobile shall never do this.
//
// Parameters:
// - conn: Socket number
void mobile_board_sock_close(void *user, unsigned conn);

// mobile_board_sock_connect - Connect a socket
//
// Performs a TCP connect on a TCP socket, against the address specified in
// <addr>. The request is non-blocking, and will be called repeatedly until it
// either connects, errors out, or the connection is canceled by calling
// mobile_board_sock_close().
//
// If the socket is a UDP socket, this function merely sets the default
// recipient for any further mobile_board_sock_send() and
// mobile_board_sock_recv() calls, discarding any other source addresses.
//
// Connecting a socket to an <addr> of a different type as the socket should
// produce an error, libmobile shall never do this.
//
// Returns: 1 on success, 0 if connect is in progress, -1 on error
// Parameters:
// - conn: Socket number
// - addr: Address to connect to
int mobile_board_sock_connect(void *user, unsigned conn, const struct mobile_addr *addr);

// mobile_board_sock_listen - Start listening on a socket
//
// Starts listening on an opened TCP socket, with a queue/backlog of one
// connection, that will be accepted later through mobile_board_sock_accept().
// The bound address/port is specified through mobile_board_sock_open().
//
// Listening on an UDP socket, or a connected TCP socket should produce an
// error, libmobile shall never do this.
// 
// Returns: true if socket started listening, false on error
// Parameters:
// - conn: Socket number
bool mobile_board_sock_listen(void *user, unsigned conn);

// mobile_board_sock_accept - Accept an incoming connection
//
// Accepts an incoming TCP connection on a listening socket. This automatically
// discards the listening socket upon success, and any further actions using
// the same <conn> parameter will be performed with the connected socket.
//
// It's recommended to apply the same settings/options to the new socket as
// those that were applied to the original listening socket. This might be
// important for example to enable non-blocking mode.
//
// This function is non-blocking, and will be called repeatedly until a
// connection is received and accepted successfully.
//
// Performing this operation on a socket that isn't listening or is already
// connected should produce an error, libmobile shall never do this.
//
// Returns: true if a connection was accepted,
//          false if there's no incoming connections
// Parameters:
// - conn: Socket number
bool mobile_board_sock_accept(void *user, unsigned conn);

// mobile_board_sock_send - Send data over a socket
//
// Sends data over the specified socket, optionally specifying a destination
// address through the <addr> parameter. The implementation must be able to
// send at least MOBILE_MAX_TRANSFER_SIZE bytes at once.
//
// If a TCP socket is being used and the <addr> parameter is not NULL, the
// parameter must be ignored. If a UDP socket is being used that hasn't been
// connected through mobile_board_sock_connect(), and the <addr> parameter is
// NULL, this function should produce an error. Sending to an address of a
// different type as the one the socket was opened with should produce an
// error. Similarly, sending using a TCP socket that isn't connected should
// produce an error as well. libmobile shall never do any of these things.
//
// This function is non-blocking, and will be called repeatedly until all of
// the data is sent, or a timeout triggers.
//
// Returns: non-negative amount of data sent on success, -1 on error
// Parameters:
// - conn: Socket number
// - data: Data to be sent
// - size: Size of data to be sent
// - addr: Address to send to, if using a UDP socket
int mobile_board_sock_send(void *user, unsigned conn, const void *data, unsigned size, const struct mobile_addr *addr);

// mobile_board_sock_recv - Receive data from a socket
//
// Receives data from the specified socket, optionally returning the origin
// address through the <addr> parameter. The implementation must be able to
// receive at least MOBILE_MAX_TRANSFER_SIZE bytes at once. The <size>
// parameter specifies the maximum amount of data to be stored in the buffer
// pointed to by the <data> parameter. If there isn't enough data, returning
// less is OK.
//
// If the <addr> parameter is non-NULL, and at least one byte has been
// received, the <struct mobile_addr> buffer pointed to by it must be filled
// with the appropriate address. This buffer is big enough to hold the biggest
// possible address.
//
// If the <data> parameter is NULL, this function must check if the connection
// is still alive, and return 0 if it is, -2 if the remote has reset the TCP
// connection/disconnected, and -1 if any other error occurred.
//
// If a TCP socket is being used and the <addr> parameter is not NULL, the
// parameter may be ignored. Sending using a TCP socket that isn't connected
// should produce an error. libmobile shall never do any of these things.
//
// Returns: amount of data received on success,
//          -1 on error,
//          -2 on remote disconnect
// Parameters:
// - conn: Socket number
// - data: Receive buffer
// - size: Maximum data to receive
// - addr: Origin address buffer
int mobile_board_sock_recv(void *user, unsigned conn, void *data, unsigned size, struct mobile_addr *addr);

enum mobile_action mobile_action_get(struct mobile_adapter *adapter);
void mobile_action_process(struct mobile_adapter *adapter, enum mobile_action action);
void mobile_loop(struct mobile_adapter *adapter);
unsigned char mobile_transfer(struct mobile_adapter *adapter, unsigned char c);
void mobile_init(struct mobile_adapter *adapter, void *user, const struct mobile_adapter_config *config);

#ifdef __cplusplus
}
#endif
