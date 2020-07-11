#pragma once

#include <stdbool.h>
struct mobile_adapter;

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

bool mobile_dns_query(struct mobile_adapter *adapter, const unsigned conn, unsigned char *ip, const char *host, const unsigned host_len);
