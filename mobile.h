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

// Limits any user of this library should abide by
#define MOBILE_MAX_CONNECTIONS 2
#define MOBILE_MAX_TIMERS 4
#define MOBILE_MAX_TRANSFER_SIZE 0xFE  // MOBILE_MAX_DATA_SIZE - 1
#define MOBILE_MAX_NUMBER_SIZE 0x20  // Allowed phone number length: 7-16
#define MOBILE_CONFIG_SIZE 0x200
#define MOBILE_RELAY_TOKEN_SIZE 0x10

#define MOBILE_DNS_PORT 53
#define MOBILE_DEFAULT_P2P_PORT 1027
#define MOBILE_DEFAULT_RELAY_PORT 31227

#define MOBILE_HOSTLEN_IPV4 4
#define MOBILE_HOSTLEN_IPV6 16

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
    MOBILE_ACTION_DROP_CONNECTION,
    MOBILE_ACTION_RESET,
    MOBILE_ACTION_RESET_SERIAL,
    MOBILE_ACTION_CHANGE_32BIT_MODE,
    MOBILE_ACTION_WRITE_CONFIG
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

enum mobile_number {
    MOBILE_NUMBER_USER,
    MOBILE_NUMBER_PEER
};

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

        // Don't access these directly, cast instead
        struct mobile_addr4 _addr4;
        struct mobile_addr6 _addr6;
    };
};

// Board-specific function prototypes (make sure these are defined elsewhere!)

// mobile_func_debug_log - Output a line of text for debug
//
// Called to output a line of text to the debug log. This is extremely useful
// for both development of the frontend, as well as development of homebrew
// games using the adapter.
//
// Every line is separate and should be separated with newlines or a different
// implementation-defined mechanism. A line may be empty to split up multiple
// "blocks" of communication. The <line> parameter is a null-terminated string.
//
// This function is completely optional.
typedef void (*mobile_func_debug_log)(void *user, const char *line);
void mobile_impl_debug_log(void *user, const char *line);
void mobile_def_debug_log(struct mobile_adapter *adapter, mobile_func_debug_log func);

// mobile_func_serial_disable - Disable serial communications
//
// This function must ensure nothing will call mobile_transfer() while the
// serial communcations are disabled, and the current byte transfer state is
// reset, in case a byte was partially transferred at the time this function
// was called. If mobile_transfer() and mobile_loop() are implemented as
// separate threads, a mutex-like locking mechanism may be used to accomplish
// this.
typedef void (*mobile_func_serial_disable)(void *user);
void mobile_impl_serial_disable(void *user);
void mobile_def_serial_disable(struct mobile_adapter *adapter, mobile_func_serial_disable func);

// mobile_func_serial_enable - Enable serial communications
//
// Exact opposite of mobile_func_serial_disable(). This function indicates
// mobile_transfer() may be called again, resuming communications.
//
// The <mode_32bit> parameter indicates in which mode the serial peripheral
// should be initialized. It indicates whether each serial transmission by the
// console will contain 8 or 32 bits. This information is relevant for hardware
// implementations, which must account for this.
typedef void (*mobile_func_serial_enable)(void *user, bool mode_32bit);
void mobile_impl_serial_enable(void *user, bool mode_32bit);
void mobile_def_serial_enable(struct mobile_adapter *adapter, mobile_func_serial_enable func);

// mobile_func_config_read - Read from the configuration data
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
typedef bool (*mobile_func_config_read)(void *user, void *dest, uintptr_t offset, size_t size);
bool mobile_impl_config_read(void *user, void *dest, uintptr_t offset, size_t size);
void mobile_def_config_read(struct mobile_adapter *adapter, mobile_func_config_read func);

// mobile_func_config_write - Write to the configuration data
//
// Analogue of mobile_func_config_read(), writing to the configuration data
// instead. The same information applies here as well.
//
// Parameters:
// - src: Source buffer where <size> bytes will be read from
// - offset: Configuration data offset to write to
// - size: Amount of data in bytes to write
// Returns: true on success, false if the configuration can't be written
typedef bool (*mobile_func_config_write)(void *user, const void *src, uintptr_t offset, size_t size);
bool mobile_impl_config_write(void *user, const void *src, uintptr_t offset, size_t size);
void mobile_def_config_write(struct mobile_adapter *adapter, mobile_func_config_write func);

// mobile_func_time_latch - Latch a timer
//
// Timers are used to keep track of time, allowing libmobile to implement
// timeouts and other time-related mechanisms. The time tracked must reflect
// the same measurement the connected gameboy is using. This means that when an
// emulated gameboy is sped up, the timer will move faster as well. If
// connected to a real-life gameboy, this will track real time. An
// implementation must be able to track MOBILE_MAX_TIMERS amount of timers, and
// all timers must be treated exactly the same. Timers must be able to keep
// track of at least 60 seconds, with millisecond precision, preferably with
// little to no time skew.
//
// This function will "latch" the current time to the specified timer, by
// storing the current value so it may later be compared.
//
// Parameters:
// - timer: timer that should be latched
typedef void (*mobile_func_time_latch)(void *user, unsigned timer);
void mobile_impl_time_latch(void *user, unsigned timer);
void mobile_def_time_latch(struct mobile_adapter *adapter, mobile_func_time_latch func);

// mobile_func_time_check_ms - Check if a certain amount of time has passed
//
// Checks if a specified amount of milliseconds has passed since a timer has
// been latched by mobile_func_time_latch(). The notes on that function apply
// here as well.
//
// Checking a timer that hasn't been latched is undefined, libmobile shall never
// do this.
//
// Returns: true if the specified time has passed, false otherwise
// Parameters:
// - timer: timer that should be compared against
// - ms: amount of milliseconds that should be compared with
typedef bool (*mobile_func_time_check_ms)(void *user, unsigned timer, unsigned ms);
bool mobile_impl_time_check_ms(void *user, unsigned timer, unsigned ms);
void mobile_def_time_check_ms(struct mobile_adapter *adapter, mobile_func_time_check_ms func);

// mobile_func_sock_open - Open a socket
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
// using an unopened socket with any of the mobile_func_sock_* functions may
// result in the same.
//
// Returns: true if socket was created successfully, false on error
// Parameters:
// - conn: Socket number
// - type: MOBILE_SOCKTYPE_TCP vs MOBILE_SOCKTYPE_UDP
// - addrtype: MOBILE_ADDRTYPE_IPV4 vs MOBILE_ADDRTYPE_IPV6
// - bindport: Port to bind()
typedef bool (*mobile_func_sock_open)(void *user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport);
bool mobile_impl_sock_open(void *user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport);
void mobile_def_sock_open(struct mobile_adapter *adapter, mobile_func_sock_open func);

// mobile_func_sock_close - Close a socket
//
// Closes a socket opened through mobile_func_sock_open().
//
// Closing a socket that hasn't been opened is undefined and may produce an
// error or terminate the program, libmobile shall never do this.
//
// Parameters:
// - conn: Socket number
typedef void (*mobile_func_sock_close)(void *user, unsigned conn);
void mobile_impl_sock_close(void *user, unsigned conn);
void mobile_def_sock_close(struct mobile_adapter *adapter, mobile_func_sock_close func);

// mobile_func_sock_connect - Connect a socket
//
// Performs a TCP connect on a TCP socket, against the address specified in
// <addr>. The request is non-blocking, and will be called repeatedly until it
// either connects, errors out, or the connection is canceled by calling
// mobile_func_sock_close().
//
// If the socket is a UDP socket, this function merely sets the default
// recipient for any further mobile_func_sock_send() and
// mobile_func_sock_recv() calls, discarding any other source addresses.
//
// Connecting a socket to an <addr> of a different type as the socket should
// produce an error, libmobile shall never do this.
//
// Returns: 1 on success, 0 if connect is in progress, -1 on error
// Parameters:
// - conn: Socket number
// - addr: Address to connect to
typedef int (*mobile_func_sock_connect)(void *user, unsigned conn, const struct mobile_addr *addr);
int mobile_impl_sock_connect(void *user, unsigned conn, const struct mobile_addr *addr);
void mobile_def_sock_connect(struct mobile_adapter *adapter, mobile_func_sock_connect func);

// mobile_func_sock_listen - Start listening on a socket
//
// Starts listening on an opened TCP socket, with a queue/backlog of one
// connection, that will be accepted later through mobile_func_sock_accept().
// The bound address/port is specified through mobile_func_sock_open().
//
// Listening on an UDP socket, or a connected TCP socket should produce an
// error, libmobile shall never do this.
// 
// Returns: true if socket started listening, false on error
// Parameters:
// - conn: Socket number
typedef bool (*mobile_func_sock_listen)(void *user, unsigned conn);
bool mobile_impl_sock_listen(void *user, unsigned conn);
void mobile_def_sock_listen(struct mobile_adapter *adapter, mobile_func_sock_listen func);

// mobile_func_sock_accept - Accept an incoming connection
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
typedef bool (*mobile_func_sock_accept)(void *user, unsigned conn);
bool mobile_impl_sock_accept(void *user, unsigned conn);
void mobile_def_sock_accept(struct mobile_adapter *adapter, mobile_func_sock_accept func);

// mobile_func_sock_send - Send data over a socket
//
// Sends data over the specified socket, optionally specifying a destination
// address through the <addr> parameter. The implementation must be able to
// send at least MOBILE_MAX_TRANSFER_SIZE bytes at once.
//
// If a TCP socket is being used and the <addr> parameter is not NULL, the
// parameter must be ignored. If a UDP socket is being used that hasn't been
// connected through mobile_func_sock_connect(), and the <addr> parameter is
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
// - addr: Address to send to, if using a UDP socket. NULL if none.
typedef int (*mobile_func_sock_send)(void *user, unsigned conn, const void *data, unsigned size, const struct mobile_addr *addr);
int mobile_impl_sock_send(void *user, unsigned conn, const void *data, unsigned size, const struct mobile_addr *addr);
void mobile_def_sock_send(struct mobile_adapter *adapter, mobile_func_sock_send func);

// mobile_func_sock_recv - Receive data from a socket
//
// Receives data from the specified socket, optionally returning the origin
// address through the <addr> parameter. The implementation must be able to
// receive at least 512 bytes at once. The <size> parameter specifies the
// maximum amount of data to be stored in the buffer pointed to by the <data>
// parameter. If there isn't enough data, returning less is OK.
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
typedef int (*mobile_func_sock_recv)(void *user, unsigned conn, void *data, unsigned size, struct mobile_addr *addr);
int mobile_impl_sock_recv(void *user, unsigned conn, void *data, unsigned size, struct mobile_addr *addr);
void mobile_def_sock_recv(struct mobile_adapter *adapter, mobile_func_sock_recv func);

// mobile_func_update_number - Receive number
//
// This function is called whenever the library either connects to the relay to
// retrieve its own mobile number, or when a different number is dialed.
//
// The buffer passed along with the <number> parameter is a NULL-terminated C
// string, containing up to MOBILE_MAX_NUMBER_SIZE characters, not including
// the terminator. The parameter might also be a NULL pointer, in which case
// the specified number is no longer valid. This may happen when the phone
// disconnects from the peer.
//
// Implementing this callback is purely informational, but highly recommended,
// as this information should be shown to the user. The information is usually
// also relayed through the mobile_func_debug_log function, but it's harder
// for the user to find in there.
//
// Parameters:
// - type: Which number is being updated
// - number: Zero-delimited ASCII string containing the number
typedef void (*mobile_func_update_number)(void *user, enum mobile_number type, const char *number);
void mobile_impl_update_number(void *user, enum mobile_number type, const char *number);
void mobile_def_update_number(struct mobile_adapter *adapter, mobile_func_update_number func);

void mobile_config_set_device(struct mobile_adapter *adapter, enum mobile_adapter_device device, bool unmetered);
void mobile_config_get_device(struct mobile_adapter *adapter, enum mobile_adapter_device *device, bool *unmetered);
void mobile_config_set_dns(struct mobile_adapter *adapter, const struct mobile_addr *dns1, const struct mobile_addr *dns2);
void mobile_config_get_dns(struct mobile_adapter *adapter, struct mobile_addr *dns1, struct mobile_addr *dns2);
void mobile_config_set_p2p_port(struct mobile_adapter *adapter, unsigned p2p_port);
void mobile_config_get_p2p_port(struct mobile_adapter *adapter, unsigned *p2p_port);
void mobile_config_set_relay(struct mobile_adapter *adapter, const struct mobile_addr *relay);
void mobile_config_get_relay(struct mobile_adapter *adapter, struct mobile_addr *relay);
void mobile_config_set_relay_token(struct mobile_adapter *adapter, const unsigned char *token);
bool mobile_config_get_relay_token(struct mobile_adapter *adapter, unsigned char *token);

// mobile_config_load - Manually force a load of the configuration values
//
// Makes sure the configuration has been loaded. The configuration is loaded
// only once per instance of the library, and calling this function will
// guarantee this.
//
// The necessary callback functions must be set up before this function is ran.
//
// This function is generally optional, but must be executed before any
// mobile_get_* and mobile_set_* functions are used.
//
// Parameters:
// - adapter: Library state
void mobile_config_load(struct mobile_adapter *adapter);

// mobile_config_save - Manually force a save of the configuration values
//
// Makes sure the configuration has been written. This function is a no-op if
// none of the configuration values have changed.
//
// Parameters:
// - adapter: Library state
void mobile_config_save(struct mobile_adapter *adapter);

// mobile_action_get - Advanced library main loop, get next action
//
// This function may be used in place of mobile_loop(), to see which action is
// about to be executed. The user may use this knowledge to influence user
// code, such as putting the main loop thread to sleep if no action has been
// received for a bit. They may also prevent certain actions from being
// executed, but never trigger actions that weren't received through this
// function.
//
// If unsure, use mobile_loop() instead.
//
// Parameters:
// - adapter: Library state
// Returns: Action to execute
enum mobile_action mobile_action_get(struct mobile_adapter *adapter);

// mobile_action_process - Advanced library main loop, process next action
//
// Execute the action received through mobile_action_get(), see its
// documentation for more details.
//
// This function may NEVER be used to manually execute library actions, and may
// only be called with the result of mobile_action_get().
//
// Parameters:
// - adapter: Library state
// - action: Action to execute
void mobile_action_process(struct mobile_adapter *adapter, enum mobile_action action);

// mobile_loop - Library main loop
//
// Must be called regularly for the library to function. Ideally as frequently
// as possible, but at a minimum every 100ms, in a main loop. This function
// will never block, and will manage its own timeouts, unless a blocking action
// is performed by the user in one of the mobile_func_* callbacks. So, unless
// absolutely sure, let it do its thing.
//
// Shorthand for mobile_action_process(adapter, mobile_action_get(adapter))
// See the documentation for either of these functions for advanced useage.
//
// Parameters:
// - adapter: Library state
void mobile_loop(struct mobile_adapter *adapter);

// mobile_transfer - Exchange a byte between the adapter and the console
//
// This function takes the byte received during the last exchange with the
// console, and returns the byte that must be sent during the next exchange.
//
// In simpler terms, here's what a serial slave interrupt service routine would
// look like (e.g. on a game boy or other hardware):
//     void serial_isr() {
//         SB = mobile_transfer(adapter, SB);
//     }
//
// Yet, on something like an emulator, which intercepts the byte mid-transfer
// on the sending console's side, it might look more like this:
//     unsigned char slave_SB;
//     void serial_exchange() {  // Called if ((SC & 0x80) != 0)
//         unsigned char temp_SB = emu_SB;
//         emu_SB = slave_SB;  // Send previous byte to master
//         slave_SB = mobile_transfer(adapter, temp_SB);
//     }
//
// This is the only thread-safe function in the library, and as such, may be
// called wherever and whenever. The recommended practice on hardware devices
// is to have this function be called during an interrupt. On emulators, one
// can opt to execute this function in the main emulation thread, while
// delegating the rest of the library to for example the GUI thread, or simply
// run the entire library in the same thread.
//
// To ensure thread-safety, this function may NEVER be executed until
// mobile_func_serial_enable() is called for the first time, or whenever
// mobile_func_serial_disable() is called, before the serial is enabled again.
// The user may use a mutex-like locking mechanism or disable interrupts to
// achieve this. See the documentation on mobile_func_serial_disable() for more
// information.
//
// Parameters:
// - adapter: Library state
// - c: Byte received in previous exchange
// Returns: Byte to send in next exchange
unsigned char mobile_transfer(struct mobile_adapter *adapter, unsigned char c);

// mobile_start - Begin the library operation
//
// Does necessary post-initialization, such as making sure the configuration is
// loaded, and enables the serial communication.
//
// This function must be called before mobile_transfer(), mobile_loop() or its
// component functions are executed, but after the relevant callbacks have been
// set up and configured.
//
// Parameters:
// - adapter: Library state
void mobile_start(struct mobile_adapter *adapter);

// mobile_stop - Terminate the library
//
// Stops library operation by disabling the serial, saving any necessary state
// and resetting the library.
//
// It's recommended to call this function before ending the program execution,
// but it may also be used to temporarily pause the library, before calling
// mobile_start() again.
//
// Parameters:
// - adapter: Library state
void mobile_stop(struct mobile_adapter *adapter);

// mobile_init - Initialize library
//
// Initializes the library state at <adapter>. No other functions may be used
// before this one. Can be used to reset the library as well, though care must
// be taken that no other library functions are running when this function is
// executed.
//
// Multiple instances of the library may be initialized and used concurrently,
// each instance having its own <adapter> library state, provided the
// user-provided mobile_func_* callback functions take this into account (e.g.
// by storing all instance-specific state in the <user> parameter).
//
// The <user> parameter is not used by the library for any other purpose than
// being passed to the mobile_func_* functions.
//
// See also mobile_new(), which is a recommended wrapper for this function.
//
// Parameters:
// - adapter: Library state
// - user: User data pointer for callbacks
void mobile_init(struct mobile_adapter *adapter, void *user);

// mobile_new - Allocate memory and initialize library
//
// Allocates space for a mobile_adapter structure and initializes it. This must
// be used in place of mobile_init() whenever the library is used as a shared
// library, to ensure forwards ABI compatibility, but is recommended in either
// case. mobile_init() may be used to reinitialize the allocated memory.
//
// The memory returned by this function may be released using free().
//
// See mobile_init() for exact behavior.
//
// Parameters:
// - user: User data pointer for callbacks
// Returns: Library state
struct mobile_adapter *mobile_new(void *user);

// In case anyone ever needs to reserve memory for the library state
// themselves, here is a sizeof(struct mobile_adapter).
extern const size_t mobile_sizeof;

#ifdef __cplusplus
}
#endif
