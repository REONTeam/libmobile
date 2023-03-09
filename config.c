// SPDX-License-Identifier: LGPL-3.0-or-later
#include "config.h"

#include <string.h>

#include "mobile_data.h"
#include "util.h"
#include "compat.h"

// The area of the config in which data is actually stored by the game boy
#define MOBILE_CONFIG_SIZE_INTERNAL 0xC0
// Extra data used by the library
#define MOBILE_CONFIG_OFFSET_LIBRARY 0x100
#define MOBILE_CONFIG_SIZE_LIBRARY 0x60
static_assert(MOBILE_CONFIG_SIZE >= MOBILE_CONFIG_OFFSET_LIBRARY +
    MOBILE_CONFIG_SIZE_LIBRARY, "MOBILE_CONFIG_SIZE isn't big enough!");

static uint16_t checksum(unsigned char *buf, unsigned len)
{
    uint16_t sum = 0;
    while (len--) sum += *buf++;
    return sum;
}

static void config_internal_clear(struct mobile_adapter *adapter)
{
    unsigned char buffer[MOBILE_CONFIG_SIZE_INTERNAL / 2] = {0};
    mobile_cb_config_write(adapter, buffer, sizeof(buffer) * 0,
        sizeof(buffer));
    mobile_cb_config_write(adapter, buffer, sizeof(buffer) * 1,
        sizeof(buffer));
}

static bool config_internal_verify(struct mobile_adapter *adapter)
{
    unsigned char buffer[MOBILE_CONFIG_SIZE_INTERNAL / 2];
    if (!mobile_cb_config_read(adapter, buffer, sizeof(buffer) * 0,
            sizeof(buffer))) {
        return false;
    }
    if (buffer[0] != 'M' || buffer[1] != 'A') {
        return false;
    }

    uint16_t sum = checksum(buffer, sizeof(buffer));
    if (!mobile_cb_config_read(adapter, buffer, sizeof(buffer) * 1,
            sizeof(buffer))) {
        return false;
    }
    sum += checksum(buffer, sizeof(buffer) - 2);

    uint16_t config_sum = buffer[sizeof(buffer) - 2] << 8 |
        buffer[sizeof(buffer) - 1];
    return sum == config_sum;
}

static void config_library_load_host(struct mobile_addr *addr, const void *host, const unsigned char *port)
{
    if (addr->type == MOBILE_ADDRTYPE_IPV4) {
        struct mobile_addr4 *addr4 = (struct mobile_addr4 *)addr;
        static_assert(sizeof(addr4->host) == 4, "addr size mismatch");
        addr4->port = port[0];
        addr4->port |= port[1] << 8;
        memcpy(addr4->host, host, sizeof(addr4->host));
    } else if (addr->type == MOBILE_ADDRTYPE_IPV6) {
        struct mobile_addr6 *addr6 = (struct mobile_addr6 *)addr;
        static_assert(sizeof(addr6->host) == 16, "addr size mismatch");
        addr6->port = port[0];
        addr6->port |= port[1] << 8;
        memcpy(addr6->host, host, sizeof(addr6->host));
    }
}

static bool config_library_load(struct mobile_adapter *adapter)
{
    struct mobile_adapter_config *config = &adapter->config;

    unsigned char buffer[MOBILE_CONFIG_SIZE_LIBRARY];
    if (!mobile_cb_config_read(adapter, buffer, MOBILE_CONFIG_OFFSET_LIBRARY,
            sizeof(buffer))) {
        return false;
    }

    if (buffer[0] != 'L') return false;
    if (buffer[1] != 'M') return false;
    if (buffer[2] != 0) return false;
    uint16_t sum = checksum(buffer + 5, sizeof(buffer) - 5);
    uint16_t config_sum = buffer[3] | buffer[4] << 8;
    if (sum != config_sum) return false;

    config->device = buffer[0x05];
    config->dns1.type = buffer[0x06];
    config->dns2.type = buffer[0x07];
    config->p2p_port = buffer[0x08];
    config->p2p_port |= buffer[0x09] << 8;
    config->relay.type = buffer[0x0a];
    config->relay_token_init = buffer[0x0b];

    config_library_load_host(&config->dns1, buffer + 0x20, buffer + 0x1a);
    config_library_load_host(&config->dns2, buffer + 0x30, buffer + 0x1c);
    config_library_load_host(&config->relay, buffer + 0x40, buffer + 0x1e);

    if (config->relay_token_init) {
        static_assert(sizeof(config->relay_token) == 0x10,
            "token size mismatch");
        memcpy(config->relay_token, buffer + 0x50,
            sizeof(config->relay_token));
    }

    return true;
}

static void config_library_save_host(const struct mobile_addr *addr, void *host, unsigned char *port)
{
    if (addr->type == MOBILE_ADDRTYPE_IPV4) {
        const struct mobile_addr4 *addr4 = (struct mobile_addr4 *)addr;
        static_assert(sizeof(addr4->host) == 4, "addr size mismatch");
        port[0] = addr4->port;
        port[1] = addr4->port >> 8;
        memcpy(host, addr4->host, sizeof(addr4->host));
    } else if (addr->type == MOBILE_ADDRTYPE_IPV6) {
        const struct mobile_addr6 *addr6 = (struct mobile_addr6 *)addr;
        static_assert(sizeof(addr6->host) == 16, "addr size mismatch");
        port[0] = addr6->port;
        port[1] = addr6->port >> 8;
        memcpy(host, addr6->host, sizeof(addr6->host));
    }
}

static void config_library_save(struct mobile_adapter *adapter)
{
    struct mobile_adapter_config *config = &adapter->config;

    unsigned char buffer[MOBILE_CONFIG_SIZE_LIBRARY] = {0};
    buffer[0] = 'L';
    buffer[1] = 'M';
    buffer[2] = 0;

    buffer[0x05] = config->device;
    buffer[0x06] = config->dns1.type;
    buffer[0x07] = config->dns2.type;
    buffer[0x08] = config->p2p_port;
    buffer[0x09] = config->p2p_port >> 8;
    buffer[0x0a] = config->relay.type;
    buffer[0x0b] = config->relay_token_init;

    // 0x0c - 0x19 unused

    config_library_save_host(&config->dns1, buffer + 0x20, buffer + 0x1a);
    config_library_save_host(&config->dns2, buffer + 0x30, buffer + 0x1c);
    config_library_save_host(&config->relay, buffer + 0x40, buffer + 0x1e);

    if (config->relay_token_init) {
        static_assert(sizeof(config->relay_token) == 0x10,
            "token size mismatch");
        memcpy(buffer + 0x50, config->relay_token,
            sizeof(config->relay_token));
    }

    uint16_t sum = checksum(buffer + 5, sizeof(buffer) - 5);
    buffer[0x03] = sum & 0xff;
    buffer[0x04] = sum >> 8;

    mobile_cb_config_write(adapter, buffer, MOBILE_CONFIG_OFFSET_LIBRARY,
        sizeof(buffer));
}

void mobile_config_init(struct mobile_adapter *adapter)
{
    adapter->config.loaded = false;
    adapter->config.dirty = true;
    adapter->config.device = MOBILE_ADAPTER_BLUE;
    adapter->config.dns1 = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
    adapter->config.dns2 = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
    adapter->config.p2p_port = MOBILE_DEFAULT_P2P_PORT;
    adapter->config.relay = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
    adapter->config.relay_token_init = false;
    memset(adapter->config.relay_token, 0, MOBILE_RELAY_TOKEN_SIZE);
}

void mobile_config_load(struct mobile_adapter *adapter)
{
    if (adapter->config.loaded) return;
    if (!config_internal_verify(adapter)) config_internal_clear(adapter);
    if (config_library_load(adapter)) adapter->config.dirty = false;
    adapter->config.loaded = true;
}

void mobile_config_save(struct mobile_adapter *adapter)
{
    if (!adapter->config.dirty) return;
    config_library_save(adapter);
    adapter->config.dirty = false;
}

static void mobile_config_apply(struct mobile_adapter *adapter)
{
    adapter->config.dirty = true;
    adapter->config.loaded = true;
}

void mobile_config_set_device(struct mobile_adapter *adapter, enum mobile_adapter_device device, bool unmetered)
{
    // Latched at the start of a command when session hasn't been started.
    // In serial.c:mobile_serial_transfer()
    adapter->config.device = device |
        (unmetered ? MOBILE_CONFIG_DEVICE_UNMETERED : 0);

    mobile_config_apply(adapter);
}

void mobile_config_get_device(struct mobile_adapter *adapter, enum mobile_adapter_device *device, bool *unmetered)
{
    *device = adapter->config.device & ~MOBILE_CONFIG_DEVICE_UNMETERED;
    *unmetered = adapter->config.device & MOBILE_CONFIG_DEVICE_UNMETERED;
}

void mobile_config_set_dns(struct mobile_adapter *adapter, const struct mobile_addr *dns1, const struct mobile_addr *dns2)
{
    // Latched for each dns query
    mobile_addr_copy(&adapter->config.dns1, dns1);
    mobile_addr_copy(&adapter->config.dns2, dns2);

    mobile_config_apply(adapter);
}

void mobile_config_get_dns(struct mobile_adapter *adapter, struct mobile_addr *dns1, struct mobile_addr *dns2)
{
    mobile_addr_copy(dns1, &adapter->config.dns1);
    mobile_addr_copy(dns2, &adapter->config.dns2);
}

void mobile_config_set_p2p_port(struct mobile_adapter *adapter, unsigned p2p_port)
{
    // Latched whenever a number a dialed or the wait command is executed
    if (p2p_port == 0) return;
    adapter->config.p2p_port = p2p_port;

    mobile_config_apply(adapter);
}

void mobile_config_get_p2p_port(struct mobile_adapter *adapter, unsigned *p2p_port)
{
    *p2p_port = adapter->config.p2p_port;
}

void mobile_config_set_relay(struct mobile_adapter *adapter, const struct mobile_addr *relay)
{
    // Latched whenever a number a dialed or the wait command is executed
    mobile_addr_copy(&adapter->config.relay, relay);

    mobile_config_apply(adapter);
    mobile_number_fetch_reset(adapter);
}

void mobile_config_get_relay(struct mobile_adapter *adapter, struct mobile_addr *relay)
{
    mobile_addr_copy(relay, &adapter->config.relay);
}

void mobile_config_set_relay_token_internal(struct mobile_adapter *adapter, const unsigned char *token)
{
    adapter->config.relay_token_init = !!token;
    if (token) {
        memcpy(adapter->config.relay_token, token, MOBILE_RELAY_TOKEN_SIZE);
    }

    mobile_config_apply(adapter);
}

void mobile_config_set_relay_token(struct mobile_adapter *adapter, const unsigned char *token)
{
    mobile_config_set_relay_token_internal(adapter, token);
    mobile_number_fetch_reset(adapter);
}

bool mobile_config_get_relay_token(struct mobile_adapter *adapter, unsigned char *token)
{
    if (!adapter->config.relay_token_init) return false;
    memcpy(token, adapter->config.relay_token, MOBILE_RELAY_TOKEN_SIZE);
    return true;
}
