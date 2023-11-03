// SPDX-License-Identifier: LGPL-3.0-or-later
#include "dns.h"

#include <string.h>

#include "mobile_data.h"
#include "util.h"
#include "compat.h"

// Implemented RFCs:
// RFC1035 - DOMAIN NAMES - IMPLEMENTATION AND SPECIFICATION
// RFC6895 - Domain Name System (DNS) IANA Considerations
// RFC3596 - DNS Extensions to Support IP Version 6

// Not implemented but possibly relevant for the future:
// RFC6891 - Extension Mechanisms for DNS (EDNS(0))
// RFC7873 - Domain Name System (DNS) Cookies

#define DNS_HEADER_SIZE 12
#define DNS_QD_SIZE 4
#define DNS_RR_SIZE 10

enum dns_qtype {
    DNS_QTYPE_A = 1,
    DNS_QTYPE_AAAA = 28
};

void mobile_dns_init(struct mobile_adapter *adapter)
{
    adapter->dns.id = 0;
}

static void debug_prefix(struct mobile_adapter *adapter)
{
    mobile_debug_print(adapter, PSTR("<DNS> "));
}

static bool dns_make_name(struct mobile_buffer_dns *state, unsigned *offset, const char *name, unsigned name_len)
{
    unsigned char *plen = state->data + *offset;
    unsigned char *pdat = plen + 1;
    unsigned count = 0;

    for (const char *c = name; c < name + name_len; c++) {
        if (pdat - state->data + 1 > MOBILE_DNS_PACKET_SIZE) return false;
        if (*c == '.') {
            *plen = count;
            count = 0;
            plen = pdat++;
        } else {
            if (count++ >= 63) return false;
            *pdat++ = *c;
        }
    }
    if (pdat - state->data + 1 > MOBILE_DNS_PACKET_SIZE) return false;
    *plen = count;
    *pdat++ = 0;
    *offset = (unsigned)(pdat - state->data);
    return true;
}

static bool dns_name_compare(struct mobile_buffer_dns *state, unsigned *offset, const char *name, unsigned name_len)
{
    if (*offset + 1 > state->size) return false;
    if (!name_len) return false;

    const char *pname = name;
    const unsigned char *pcmp = state->data + *offset;

    const unsigned char *end = NULL;

    for (;;) {
        if (!*pcmp) {
            break;
        } else if ((*pcmp & 0xC0) == 0xC0) {
            // RFC1035 Section 4.1.4. Message compression
            if (pcmp - state->data + 2U > state->size) return false;
            if (!end) end = pcmp + 1;

            unsigned off = (pcmp[0] & 0x3F) << 8 | pcmp[1];
            if (off + 1 > state->size) return false;
            pcmp = state->data + off;
        } else if ((*pcmp & 0xC0) == 0x00) {
            unsigned len = *pcmp++;
            if (pcmp - state->data + len + 1 > state->size) return false;
            if (pname != name && *pname++ != '.') return false;
            if (pname - name + len > name_len) return false;
            while (len--) {
                if (*pcmp++ != *pname++) return false;
            }
        } else {
            return false;
        }
    }
    if ((unsigned)(pname - name) != name_len) return false;

    if (!end) end = pcmp;
    *offset = (unsigned)(end - state->data) + 1;
    return true;
}

static int dns_name_len(struct mobile_buffer_dns *state, unsigned offset)
{
    if (offset + 1 > state->size) return -1;

    const unsigned char *pcmp = state->data + offset;
    for (;;) {
        if (!*pcmp) {
            break;
        } else if ((*pcmp & 0xC0) == 0xC0) {
            // RFC1035 Section 4.1.4. Message compression
            unsigned pos = (unsigned)(pcmp - state->data);
            if (pos + 2 > state->size) return -1;
            return pos + 2 - offset;
        } else if ((*pcmp & 0xC0) == 0x00) {
            unsigned len = *pcmp++;
            if (pcmp - state->data + len + 1 > state->size) return -1;
            pcmp += len;
        } else {
            return -1;
        }
    }

    return (unsigned)(pcmp - state->data) + 1 - offset;
}

static bool dns_make_query(struct mobile_buffer_dns *state, unsigned id, enum dns_qtype type, const char *name, unsigned name_len)
{
    state->id = id;
    state->type = type;

    state->data[0] = (state->id >> 8) & 0xFF;
    state->data[1] = state->id & 0xFF;
    static const unsigned char header[] PROGMEM = {
        0x01, 0x00,  // Flags: Standard query, Recursion Desired
        0, 1,  // Questions: 1
        0, 0,  // Answers: 0
        0, 0,  // Authority records: 0
        0, 0,  // Additional records: 0
    };
    memcpy_P(state->data + 2, header, DNS_HEADER_SIZE - 2);

    unsigned offset = DNS_HEADER_SIZE;
    if (!dns_make_name(state, &offset, name, name_len)) return false;
    if (offset + DNS_QD_SIZE > MOBILE_DNS_PACKET_SIZE) return false;

    unsigned char *question = state->data + offset;
    question[0] = (state->type >> 8) & 0xFF;
    question[1] = (state->type >> 0) & 0xFF;
    question[2] = 0;
    question[3] = 1;  // QCLASS = IN

    state->size = offset + DNS_QD_SIZE;
    return true;
}

static int dns_verify_response(struct mobile_buffer_dns *state, unsigned *offset, const char *name, unsigned name_len)
{
    if (state->size < DNS_HEADER_SIZE) return -1;
    if ((unsigned)(state->data[0] << 8 | state->data[1]) != state->id) {
        return -1;
    }

    // Make sure:
    // - We've got a response (bit 0) for a QUERY opcode (bits 1-4)
    // - It's not a truncated message (bit 6)
    // - The recursion bit is set (bit 7)
    // - No error has happened (bits 12-15)
    unsigned flags = state->data[2] << 8 | state->data[3];
    if ((flags & 0xFB0F) != 0x8100) {
        return -2 - (flags & 0xF);
    }

    unsigned qdcount = state->data[4] << 8 | state->data[5];
    unsigned ancount = state->data[6] << 8 | state->data[7];
    //unsigned nscount = state->data[8] << 8 | state->data[9];
    //unsigned arcount = state->data[10] << 8 | state->data[11];

    if (qdcount != 1) return -18;
    if (ancount < 1) return -18;

    // Verify question section
    *offset = DNS_HEADER_SIZE;
    if (!dns_name_compare(state, offset, name, name_len)) return -19;
    if (*offset + DNS_QD_SIZE > state->size) return -19;

    unsigned char *qflags = state->data + *offset;
    if ((unsigned)(qflags[0] << 8 | qflags[1]) != state->type) return -19;
    if ((qflags[2] << 8 | qflags[3]) != 1) return -19;  // QCLASS = IN
    *offset += DNS_QD_SIZE;

    return ancount;
}

static int dns_get_answer(struct mobile_buffer_dns *state, unsigned *offset, const char *name, unsigned name_len)
{
    // Get the start of the RR info and make sure it all fits in the buffer
    int rname_len = dns_name_len(state, *offset);
    if (rname_len < 0) return -1;
    if (*offset + rname_len + DNS_RR_SIZE > state->size) return -1;

    // Check the response length fits in the packet
    unsigned char *info = state->data + *offset + rname_len;
    unsigned rdlength = info[8] << 8 | info[9];
    unsigned rdata = *offset + rname_len + DNS_RR_SIZE;
    if (*offset + rname_len + DNS_RR_SIZE + rdlength > state->size) {
        return -1;
    }

    // Make sure this is the kind of response we asked for
    if (!dns_name_compare(state, offset, name, name_len)) return -2;
    if ((unsigned)(info[0] << 8 | info[1]) != state->type) return -2;
    if ((info[2] << 8 | info[3]) != 1) return -2;  // QCLASS = IN
    if (state->type == DNS_QTYPE_A && rdlength != 4) return -2;
    if (state->type == DNS_QTYPE_AAAA && rdlength != 16) return -2;

    *offset += DNS_RR_SIZE + rdlength;
    return rdata;
}

bool mobile_dns_request_send(struct mobile_adapter *adapter, unsigned conn, const struct mobile_addr *addr_send, const char *host, unsigned host_len)
{
    struct mobile_adapter_dns *s = &adapter->dns;
    struct mobile_buffer_dns *b = &adapter->buffer.dns;

    if (!dns_make_query(b, ++s->id, DNS_QTYPE_A, host, host_len)) return false;

    if (!mobile_cb_sock_send(adapter, conn, b->data, b->size, addr_send)) {
        return false;
    }

    return true;
}

// Returns: -1 on error, 0 if processing, 1 on success
int mobile_dns_request_recv(struct mobile_adapter *adapter, unsigned conn, const struct mobile_addr *addr_send, const char *host, unsigned host_len, unsigned char *ip)
{
    struct mobile_buffer_dns *b = &adapter->buffer.dns;

    struct mobile_addr addr_recv = {0};
    int recv = mobile_cb_sock_recv(adapter, conn, b->data,
        MOBILE_DNS_PACKET_SIZE, &addr_recv);
    if (recv <= 0) return recv;
    b->size = recv;

    // Verify sender, discard if incorrect
    if (!mobile_addr_compare(addr_send, &addr_recv)) return 0;

    unsigned offset;
    int ancount = dns_verify_response(b, &offset, host, host_len);
    if (ancount < 0) {
        debug_prefix(adapter);
        mobile_debug_print(adapter, PSTR("Query result error: %d"), ancount);
        mobile_debug_endl(adapter);
        return -1;
    }

    while (ancount--) {
        int anoffset = dns_get_answer(b, &offset, host, host_len);
        if (anoffset < -1) continue;
        if (anoffset == -1) break;
        memcpy(ip, b->data + anoffset, MOBILE_HOSTLEN_IPV4);
        return 1;
    }
    debug_prefix(adapter);
    mobile_debug_print(adapter, PSTR("No valid answer received"));
    mobile_debug_endl(adapter);
    return -1;
}
