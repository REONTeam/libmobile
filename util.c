#include "util.h"

#include <string.h>

#include "data.h"

static unsigned mobile_addr_length(struct mobile_addr *addr)
{
    unsigned len = 0;
    if (addr->type == MOBILE_ADDRTYPE_IPV4) len = sizeof(struct mobile_addr4);
    if (addr->type == MOBILE_ADDRTYPE_IPV6) len = sizeof(struct mobile_addr6);
    return len;
}

// <dest> must be big enough to hold either type of address
// <src> doesn't have to be
bool mobile_addr_copy(struct mobile_addr *dest, struct mobile_addr *src)
{
    unsigned len = mobile_addr_length(src);
    if (!len) return false;
    memcpy(dest, src, len);
    return true;
}

bool mobile_addr_compare(struct mobile_addr *addr1, struct mobile_addr *addr2)
{
    if (addr1->type != addr2->type) return false;
    unsigned len = mobile_addr_length(addr2);
    if (!len) return false;
    return memcmp(addr1, addr2, len) == 0;
}
