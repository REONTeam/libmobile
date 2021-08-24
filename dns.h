// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stdbool.h>

struct mobile_adapter;
struct mobile_addr;

#define MOBILE_DNS_PORT 53

#define MOBILE_DNS_MAX_NAME_SIZE 256
#define MOBILE_DNS_PACKET_SIZE 512

struct mobile_adapter_dns {
    unsigned id;
    unsigned type;
    unsigned name_len;
    unsigned char name[MOBILE_DNS_MAX_NAME_SIZE];
    unsigned buffer_len;
    unsigned char buffer[MOBILE_DNS_PACKET_SIZE];
};

void mobile_dns_init(struct mobile_adapter *adapter);
bool mobile_dns_query_send(struct mobile_adapter *adapter, const unsigned conn, struct mobile_addr *addr_send, const char *host, const unsigned host_len);
int mobile_dns_query_recv(struct mobile_adapter *adapter, const unsigned conn, struct mobile_addr *addr_send, unsigned char *ip);
