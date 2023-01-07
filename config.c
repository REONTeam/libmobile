// SPDX-License-Identifier: LGPL-3.0-or-later
#include "config.h"

#include "data.h"
#include "util.h"

void mobile_config_init(struct mobile_adapter *adapter)
{
    adapter->config.device = MOBILE_ADAPTER_BLUE;
    adapter->config.device_unmetered = false;
    adapter->config.dns1 = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
    adapter->config.dns2 = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
    adapter->config.p2p_port = MOBILE_DEFAULT_P2P_PORT;
    adapter->config.relay = (struct mobile_addr){.type = MOBILE_ADDRTYPE_NONE};
}

void mobile_config_set_device(struct mobile_adapter *adapter, enum mobile_adapter_device device, bool unmetered)
{
    adapter->config.device = device;
    adapter->config.device_unmetered = unmetered;
}

void mobile_config_set_dns(struct mobile_adapter *adapter, const struct mobile_addr *dns1, const struct mobile_addr *dns2)
{
    mobile_addr_copy(&adapter->config.dns1, dns1);
    mobile_addr_copy(&adapter->config.dns2, dns2);
}

void mobile_config_set_p2p_port(struct mobile_adapter *adapter, unsigned p2p_port)
{
    if (p2p_port == 0) return;
    adapter->config.p2p_port = p2p_port;
}

void mobile_config_set_relay(struct mobile_adapter *adapter, const struct mobile_addr *relay)
{
    mobile_addr_copy(&adapter->config.relay, relay);
}
