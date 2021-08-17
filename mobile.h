#pragma once

// Public interface for libmobile
// Any other header should be considered private.

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "limits.h"
#include "serial.h"
#include "commands.h"
#include "dns.h"

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
    MOBILE_ACTION_RESET_SERIAL,
};

enum mobile_timers {
    MOBILE_TIMER_SERIAL,
    MOBILE_TIMER_COMMANDS,
    MOBILE_TIMER_RESERVED3,
    MOBILE_TIMER_RESERVED4,
};

enum mobile_socktype {
    MOBILE_SOCKTYPE_TCP,
    MOBILE_SOCKTYPE_UDP,
};

enum mobile_addrtype {
    MOBILE_ADDRTYPE_IPV4,
    MOBILE_ADDRTYPE_IPV6,
};

struct mobile_addr4 {
    enum mobile_addrtype type;
    unsigned port;
    unsigned char host[4];
};

struct mobile_addr6 {
    enum mobile_addrtype type;
    unsigned port;
    unsigned char host[16];
};

struct mobile_addr {
    union {
        enum mobile_addrtype type;
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
};
#define MOBILE_ADAPTER_CONFIG_DEFAULT (struct mobile_adapter_config){ \
    .device = MOBILE_ADAPTER_BLUE, \
    .p2p_port = 1027, \
    .unmetered = false \
}

struct mobile_adapter {
    void *user;
    struct mobile_adapter_config config;
    struct mobile_adapter_serial serial;
    struct mobile_adapter_commands commands;
    struct mobile_adapter_dns dns;
};

// Board-specific function prototypes (make sure these are defined elsewhere!)
// TODO: Actually document these functions, with expectations and assumptions.
void mobile_board_serial_disable(void *user);
void mobile_board_serial_enable(void *user);
void mobile_board_debug_cmd(void *user, int send, const struct mobile_packet *packet);
bool mobile_board_config_read(void *user, void *dest, uintptr_t offset, size_t size);
bool mobile_board_config_write(void *user, const void *src, uintptr_t offset, size_t size);
void mobile_board_time_latch(void *user, enum mobile_timers timer);
bool mobile_board_time_check_ms(void *user, enum mobile_timers timer, unsigned ms);
bool mobile_board_sock_open(void *user, unsigned conn, enum mobile_socktype type, enum mobile_addrtype addrtype, unsigned bindport);
void mobile_board_sock_close(void *user, unsigned conn);
int mobile_board_sock_connect(void *user, unsigned conn, const struct mobile_addr *addr);
bool mobile_board_sock_listen(void *user, unsigned conn);
bool mobile_board_sock_accept(void *user, unsigned conn);
bool mobile_board_sock_send(void *user, unsigned conn, const void *data, unsigned size, const struct mobile_addr *addr);
int mobile_board_sock_recv(void *user, unsigned conn, void *data, unsigned size, struct mobile_addr *addr);

enum mobile_action mobile_action_get(struct mobile_adapter *adapter);
void mobile_action_process(struct mobile_adapter *adapter, enum mobile_action action);
void mobile_loop(struct mobile_adapter *adapter);
void mobile_init(struct mobile_adapter *adapter, void *user, const struct mobile_adapter_config *config);

#ifdef __cplusplus
}
#endif
