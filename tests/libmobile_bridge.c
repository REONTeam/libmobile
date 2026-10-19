/* Test environment only. Game requests use mobile_transfer/mobile_loop. */
#include <stdlib.h>
#include <string.h>
#include "mobile_data.h"

struct context {
    struct mobile_adapter adapter;
    unsigned char config[MOBILE_CONFIG_SIZE];
    unsigned char incoming[512];
    unsigned incoming_size;
    unsigned char staged;
    bool echo;
    bool datagram;
    unsigned char destination[MOBILE_MAX_CONNECTIONS][6];
    unsigned char peer[6];
    bool peer_set;
};

static bool config_read(void *user, void *dest, uintptr_t offset, size_t size)
{
    struct context *ctx = user;
    if (offset + size > sizeof(ctx->config)) return false;
    memcpy(dest, ctx->config + offset, size);
    return true;
}

static bool config_write(void *user, const void *src, uintptr_t offset, size_t size)
{
    struct context *ctx = user;
    if (offset + size > sizeof(ctx->config)) return false;
    memcpy(ctx->config + offset, src, size);
    return true;
}

static int socket_send(void *user, unsigned conn, const void *data,
                       unsigned size, const struct mobile_addr *address)
{
    struct context *ctx = user;
    (void)conn; (void)address;
    if (conn >= MOBILE_MAX_CONNECTIONS) return -1;
    unsigned char destination[6];
    memcpy(destination, ctx->destination[conn], 6);
    if (address && address->type == MOBILE_ADDRTYPE_IPV4) {
        const struct mobile_addr4 *ipv4 = (const struct mobile_addr4 *)address;
        memcpy(destination, ipv4->host, 4);
        destination[4] = ipv4->port >> 8;
        destination[5] = ipv4->port;
    }
    bool reached_peer = !ctx->peer_set || memcmp(ctx->peer, destination, 6) == 0;
    if (ctx->echo && reached_peer && size <= sizeof(ctx->incoming)) {
        memcpy(ctx->incoming, data, size);
        ctx->incoming_size = size;
    }
    return size;
}

static int socket_receive(void *user, unsigned conn, void *data,
                          unsigned size, struct mobile_addr *address)
{
    struct context *ctx = user;
    (void)conn; (void)address;
    unsigned copied = ctx->incoming_size < size ? ctx->incoming_size : size;
    memcpy(data, ctx->incoming, copied);
    if (ctx->datagram) ctx->incoming_size = 0;
    else {
        ctx->incoming_size -= copied;
        memmove(ctx->incoming, ctx->incoming + copied, ctx->incoming_size);
    }
    return copied;
}

static int socket_connect(void *user, unsigned conn, const struct mobile_addr *address)
{
    struct context *ctx = user;
    if (conn >= MOBILE_MAX_CONNECTIONS) return -1;
    const struct mobile_addr4 *ipv4 = (const struct mobile_addr4 *)address;
    memcpy(ctx->destination[conn], ipv4->host, 4);
    ctx->destination[conn][4] = ipv4->port >> 8;
    ctx->destination[conn][5] = ipv4->port;
    return 1;
}

void test_peer(void *user, const unsigned char *address, unsigned port)
{
    struct context *ctx = user;
    memcpy(ctx->peer, address, 4);
    ctx->peer[4] = port >> 8;
    ctx->peer[5] = port;
    ctx->peer_set = true;
}

void *test_new(unsigned device)
{
    struct context *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    mobile_init(&ctx->adapter, ctx);
    for (unsigned i = 0; i < 256; i++) ctx->config[i] = i;
    mobile_def_config_read(&ctx->adapter, config_read);
    mobile_def_config_write(&ctx->adapter, config_write);
    mobile_def_sock_connect(&ctx->adapter, socket_connect);
    mobile_def_sock_send(&ctx->adapter, socket_send);
    mobile_def_sock_recv(&ctx->adapter, socket_receive);
    /* Select the emulated adapter and avoid library configuration migration. */
    ctx->adapter.config.loaded = true;
    ctx->adapter.config.device = device;
    ctx->staged = 0xD2;
    mobile_start(&ctx->adapter);
    return ctx;
}

void test_free(void *user)
{
    free(user);
}

unsigned test_byte(void *user, unsigned byte)
{
    struct context *ctx = user;
    unsigned previous = ctx->staged;
    mobile_loop(&ctx->adapter);
    ctx->staged = mobile_transfer(&ctx->adapter, byte);
    mobile_loop(&ctx->adapter);
    return previous;
}

void test_internet(void *user, unsigned connected_udp)
{
    struct context *ctx = user;
    ctx->adapter.commands.state = MOBILE_CONNECTION_INTERNET;
    ctx->adapter.commands.connections[0] = connected_udp;
    ctx->datagram = true;
}

void test_tcp(void *user)
{
    struct context *ctx = user;
    ctx->adapter.commands.state = MOBILE_CONNECTION_INTERNET;
    ctx->adapter.commands.connections[0] = true;
    ctx->datagram = false;
}

void test_receive(void *user, const unsigned char *data, unsigned size, unsigned echo)
{
    struct context *ctx = user;
    if (size > sizeof(ctx->incoming)) return;
    memcpy(ctx->incoming, data, size);
    ctx->incoming_size = size;
    ctx->echo = echo;
}
