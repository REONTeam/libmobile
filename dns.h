// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stdbool.h>

struct mobile_adapter;
struct mobile_addr;

#define MOBILE_DNS_PACKET_SIZE 512

struct mobile_adapter_dns {
    unsigned id;
    unsigned type;
    unsigned buffer_len;
    unsigned char buffer[MOBILE_DNS_PACKET_SIZE];
};

void mobile_dns_init(struct mobile_adapter *adapter);
bool mobile_dns_query_send(struct mobile_adapter *adapter, unsigned conn, const struct mobile_addr *addr_send, const char *host, unsigned host_len);
int mobile_dns_query_recv(struct mobile_adapter *adapter, unsigned conn, const struct mobile_addr *addr_send, const char *host, unsigned host_len, unsigned char *ip);
