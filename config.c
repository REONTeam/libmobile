// SPDX-License-Identifier: LGPL-3.0-or-later
#include "config.h"

#include <string.h>
#include "data.h"
#include "util.h"

void mobile_config_init(struct mobile_adapter *adapter)
{
    adapter->config.device = MOBILE_ADAPTER_BLUE;
    adapter->config.dns1 = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
    adapter->config.dns2 = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
    adapter->config.p2p_port = MOBILE_DEFAULT_P2P_PORT;
    adapter->config.relay = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
    adapter->config.relay_token_init = false;
    memset(adapter->config.relay_token, 0, MOBILE_RELAY_TOKEN_SIZE);
}

void mobile_config_set_device(struct mobile_adapter *adapter, enum mobile_adapter_device device, bool unmetered)
{
    // Latched at the start of a command when session hasn't been started.
    // In serial.c:mobile_serial_transfer()
    adapter->config.device = device |
        (unmetered ? MOBILE_CONFIG_DEVICE_UNMETERED : 0);
}

void mobile_config_set_dns(struct mobile_adapter *adapter, const struct mobile_addr *dns1, const struct mobile_addr *dns2)
{
    // Latched for each dns query
    mobile_addr_copy(&adapter->config.dns1, dns1);
    mobile_addr_copy(&adapter->config.dns2, dns2);
}

void mobile_config_set_p2p_port(struct mobile_adapter *adapter, unsigned p2p_port)
{
    // Latched whenever a number a dialed or the wait command is executed
    if (p2p_port == 0) return;
    adapter->config.p2p_port = p2p_port;
}

void mobile_config_set_relay(struct mobile_adapter *adapter, const struct mobile_addr *relay)
{
    // Latched whenever a number a dialed or the wait command is executed
    mobile_addr_copy(&adapter->config.relay, relay);
}

void mobile_config_set_relay_token(struct mobile_adapter *adapter, const unsigned char *token)
{
    adapter->config.relay_token_init = true;
    memcpy(adapter->config.relay_token, token, MOBILE_RELAY_TOKEN_SIZE);
}

bool mobile_config_get_relay_token(struct mobile_adapter *adapter, unsigned char *token)
{
    if (!adapter->config.relay_token_init) return false;
    memcpy(token, adapter->config.relay_token, MOBILE_RELAY_TOKEN_SIZE);
    return true;
}
