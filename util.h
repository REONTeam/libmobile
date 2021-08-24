#pragma once

#include <stdbool.h>

struct mobile_addr;

bool mobile_addr_copy(struct mobile_addr *dest, struct mobile_addr *src);
bool mobile_addr_compare(struct mobile_addr *addr1, struct mobile_addr *addr2);
