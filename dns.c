#include "dns.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mobile.h"

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
#define DNS_PORT 53

enum dns_qtype {
    DNS_QTYPE_A = 1,
    DNS_QTYPE_AAAA = 28
};

static bool dns_make_name(struct mobile_adapter_dns *state, const char *name, const unsigned name_len)
{
    unsigned char *d = state->name + 1;
    unsigned char *len = state->name;
    *len = 0;
    for (const char *c = name; c < name + name_len; c++) {
        if (d - state->name >= MOBILE_DNS_MAX_NAME_SIZE - 1) return false;
        if (*c == '.') {
            len = d++;
            *len = 0;
        } else {
            if (*len >= 63) return false;
            *d++ = *c;
            (*len)++;
        }
    }
    *d++ = 0;
    state->name_len = d - state->name;
    return true;
}

// NOTE: Return value might be 1 past the end of the buffer,
//   if the string ends at the end of the buffer.
static int dns_compare_name(struct mobile_adapter_dns *state, unsigned *offset)
{
    if (*offset >= state->buffer_len) return 0;
    if (!state->name_len) return 0;

    unsigned char *name = state->name;
    unsigned char *cmp = state->buffer + *offset;

    int end = -1;

    for (;;) {
        if (!*cmp) {
            break;
        } else if ((*cmp & 0xC0) == 0xC0) {
            // RFC1035 Section 4.1.4. Message compression
            if (cmp - state->buffer + 2 > state->buffer_len) return -1;
            if (end < 0) end = cmp - state->buffer + 2;

            unsigned off = (cmp[0] & 0x3F) << 8 | cmp[1];
            if (off >= state->buffer_len) return -1;
            cmp = state->buffer + off;
        } else if ((*cmp & 0xC0) == 0x00) {
            unsigned len = *cmp + 1;
            if (name - state->name + len >= state->name_len) return -1;
            if (cmp - state->buffer + len >= state->buffer_len) return -1;
            while (len--) {
                if (*cmp++ != *name++) return -1;
            }
        } else {
            return 0;
        }
    }
    if (*cmp != 0 || *name != 0) return -1;

    if (end < 0) end = cmp - state->buffer + 1;
    *offset = end;
    return 0;
}

static bool dns_make_query(struct mobile_adapter_dns *state, const enum dns_qtype type)
{
    state->id = rand() & 0xFFFF;
    state->type = type;

    *(uint16_t *)state->buffer = state->id;
    memcpy(state->buffer + 2, (unsigned char []){
        0x01, 0x00,  // Flags: Standard query, Recursion Desired
        0, 1,  // Questions: 1
        0, 0,  // Answers: 0
        0, 0,  // Authority records: 0
        0, 0,  // Additional records: 0
    }, DNS_HEADER_SIZE - 2);

    memcpy(state->buffer + DNS_HEADER_SIZE, state->name, state->name_len);
    unsigned char *question = state->buffer + DNS_HEADER_SIZE + state->name_len;
    question[0] = (state->type >> 8) & 0xFF;
    question[1] = (state->type >> 0) & 0xFF;
    question[2] = 0;
    question[3] = 1;  // QCLASS = IN
    state->buffer_len = DNS_HEADER_SIZE + state->name_len + DNS_QD_SIZE;

    return true;
}

static int dns_verify_response(struct mobile_adapter_dns *state, unsigned *offset)
{
    if (state->buffer_len < DNS_HEADER_SIZE) return -1;
    if (*(uint16_t *)state->buffer != state->id) return -1;

    // Make sure:
    // - We've got a response (bit 0) for a QUERY opcode (bits 1-4)
    // - It's not a truncated message (bit 6)
    // - The server supports recursion (bits 7 and 8)
    // - Nothing is assigned to the zero-area, as according to spec (bits 9-11)
    //   - These are used to know whether this is a verified answer or not,
    //     but if zero in the request the server should never return unverified
    //     responses.
    // - No error has happened (bits 12-15)
    unsigned flags = state->buffer[2] << 8 | state->buffer[3];
    if ((flags & 0xFBFF) != 0x8180) {
        return -2 - (flags & 0xF);
    }

    unsigned qdcount = state->buffer[4] << 8 | state->buffer[5];
    unsigned ancount = state->buffer[6] << 8 | state->buffer[7];
    //unsigned nscount = state->buffer[8] << 8 | state->buffer[9];
    //unsigned arcount = state->buffer[10] << 8 | state->buffer[11];

    if (qdcount != 1) return -18;
    if (ancount < 1) return -18;

    // Verify question section
    *offset = DNS_HEADER_SIZE;
    int cmp = dns_compare_name(state, offset);
    if (cmp != 0 || *offset + DNS_QD_SIZE > state->buffer_len) return -19;

    unsigned char *qflags = state->buffer + *offset;
    if ((unsigned)(qflags[0] << 8 | qflags[1]) != state->type) return -19;
    if ((qflags[2] << 8 | qflags[3]) != 1) return -19;  // QCLASS = IN
    *offset += DNS_QD_SIZE;

    return ancount;
}

static int dns_get_answer(struct mobile_adapter_dns *state, unsigned *offset)
{
    // Get the start of the RR info and make sure it all fits in the buffer
    int name_cmp = dns_compare_name(state, offset);
    if (*offset + DNS_RR_SIZE > state->buffer_len) return -1;

    // Check the response length fits in the packet
    unsigned char *info = state->buffer + *offset;
    unsigned rdlength = info[8] << 8 | info[9];
    unsigned rdata = *offset + DNS_RR_SIZE;
    if (*offset + DNS_RR_SIZE + rdlength > state->buffer_len) return -1;
    *offset += DNS_RR_SIZE + rdlength;

    // Make sure this is the kind of response we asked for
    if (name_cmp != 0) return -2;
    if ((unsigned)(info[0] << 8 | info[1]) != state->type) return -2;
    if ((info[2] << 8 | info[3]) != 1) return -2;  // QCLASS = IN
    if (state->type == DNS_QTYPE_A && rdlength != 4) return -2;
    if (state->type == DNS_QTYPE_AAAA && rdlength != 16) return -2;

    return rdata;
}

bool mobile_dns_query(struct mobile_adapter *adapter, const unsigned conn, unsigned char *ip, const char *host, const unsigned host_len)
{
    struct mobile_adapter_dns *s = &adapter->dns;
    void *_u = adapter->user;

    unsigned char *dns_ip = adapter->commands.dns1;

    if (!dns_make_name(s, host, host_len)) return false;
    if (!dns_make_query(s, DNS_QTYPE_A)) return false;

    if (!mobile_board_udp_open(_u, conn, 0)) return false;

    if (!mobile_board_udp_sendto(_u, conn, s->buffer, s->buffer_len,
                dns_ip, DNS_PORT)) {
        mobile_board_udp_close(_u, conn);
        return false;
    }

    unsigned char recv_ip[4];
    unsigned recv_port;
    for (;;) {
        int resp_len = mobile_board_udp_recvfrom(_u, conn, s->buffer,
                MOBILE_DNS_PACKET_SIZE, recv_ip, &recv_port);
        if (resp_len <= 0) {
            continue;
        }
        // TODO: Check sender
        // TODO: Don't block or busy loop
        s->buffer_len = resp_len;
        break;
    }

    mobile_board_udp_close(_u, conn);

    unsigned offset;
    int ancount = dns_verify_response(s, &offset);
    if (ancount < 0) return 0;

    while (ancount--) {
        int anoffset = dns_get_answer(s, &offset);
        if (anoffset < -1) continue;
        if (anoffset == -1) return false;
        memcpy(ip, s->buffer + anoffset, 4);
        break;
    }
    return true;
}
