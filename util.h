#pragma once

#include <stdbool.h>

struct mobile_addr;

bool mobile_addr_copy(struct mobile_addr *dest, const struct mobile_addr *src);
bool mobile_addr_compare(const struct mobile_addr *addr1, const struct mobile_addr *addr2);
bool mobile_parse_phoneaddr(unsigned char *address, const char *data);
bool mobile_is_ipaddr(const char *str, unsigned length);
