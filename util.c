#include "util.h"

#include <string.h>

#include "data.h"

static unsigned mobile_addr_size(const struct mobile_addr *addr)
{
    unsigned size = 0;
    if (addr->type == MOBILE_ADDRTYPE_IPV4) size = sizeof(struct mobile_addr4);
    if (addr->type == MOBILE_ADDRTYPE_IPV6) size = sizeof(struct mobile_addr6);
    return size;
}

// Copy enough bytes of address <src> into <dest>, without exceeding the
//   contents of <src>.
// <dest> must have enough space to hold any kind of address.
bool mobile_addr_copy(struct mobile_addr *dest, const struct mobile_addr *src)
{
    unsigned size = mobile_addr_size(src);
    if (!size) return false;
    memcpy(dest, src, size);
    return true;
}

// Compare addresses <addr1> and <addr2> without reading out of their bounds.
bool mobile_addr_compare(const struct mobile_addr *addr1, const struct mobile_addr *addr2)
{
    if (addr1->type != addr2->type) return false;
    unsigned size = mobile_addr_size(addr2);
    if (!size) return false;
    return memcmp(addr1, addr2, size) == 0;
}

// Converts a string of 12 characters to a binary representation for an IPv4
//   address. It also checks for the validity of the address while doing so.
// The output will be a buffer of 4 bytes, representing the address.
bool mobile_parse_phoneaddr(unsigned char *address, const char *data)
{
    const char *cur_data = data;
    unsigned char *cur_addr = address;
    for (unsigned y = 0; y < 4; y++) {
        unsigned cur_num = 0;
        for (unsigned x = 0; x < 3; x++) {
            if (*cur_data < '0' || *cur_data > '9') return false;
            cur_num *= 10;
            cur_num += *cur_data++ - '0';
        }
        if (cur_num > 255) return false;
        *cur_addr++ = cur_num;
    }
    return true;
}

// Check if a string is an IP address, or a dns address.
bool mobile_is_ipaddr(const char *str, unsigned length)
{
    // If there's a colon, it's definitely not a DNS address, it's ipv6
    for (const char *c = str; c < str + length; c++) {
        if (*c == ':') return true;
    }
    // If there's only numbers and periods, it's an ipv4 address
    // This is also a valid DNS address, but blame the mobile adapter devs.
    for (const char *c = str; c < str + length; c++) {
        if ((*c < '0' || *c > '9') && *c != '.') return false;
    }
    return true;
}
