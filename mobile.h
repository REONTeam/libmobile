#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "spi.h"
#include "commands.h"

enum mobile_adapter_device {
    // The clients.
    MOBILE_ADAPTER_GAMEBOY,

    // The adapters, this is what we're emulating.
    MOBILE_ADAPTER_BLUE = 8,
    MOBILE_ADAPTER_YELLOW,
    MOBILE_ADAPTER_GREEN,
    MOBILE_ADAPTER_RED
};

struct mobile_adapter {
    void *userdata;
    enum mobile_adapter_device device;
    struct mobile_adapter_spi spi;
    struct mobile_adapter_commands commands;
};

// Board-specific function prototypes (make sure these are defined elsewhere!)
// TODO: Actually document these functions, with expectations and assumptions.
void mobile_board_disable_spi(void);
void mobile_board_enable_spi(void);
void mobile_board_debug_cmd(const int send, const struct mobile_packet *packet);
bool mobile_board_config_read(void *dest, const uintptr_t offset, const size_t size);
bool mobile_board_config_write(const void *src, const uintptr_t offset, const size_t size);
void mobile_board_time_latch(void);
bool mobile_board_time_check_ms(const unsigned ms);
bool mobile_board_tcp_connect(const unsigned char *host, const unsigned port);
bool mobile_board_tcp_listen(const unsigned port);
void mobile_board_tcp_disconnect(void);
bool mobile_board_tcp_send(const void *data, const unsigned size);
int mobile_board_tcp_receive(void *data);

void mobile_loop(struct mobile_adapter *a);
void mobile_init(struct mobile_adapter *a);

#ifdef __cplusplus
}
#endif
