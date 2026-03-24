// SPDX-License-Identifier: LGPL-3.0-or-later
#include "reon.h"

#include <string.h>

#include "mobile_data.h"
#include "commands.h"
#include "compat.h"

// Option descriptors - static list of available options
// Note: Names are stored in PROGMEM for AVR compatibility
static const char opt_name_impl_name[] PROGMEM = "Implementation name";
static const char opt_name_user_number[] PROGMEM = "User number";
static const char opt_name_current_ip[] PROGMEM = "Current IP";
static const char opt_name_adapter_type[] PROGMEM = "Adapter type";
static const char opt_name_unmetered[] PROGMEM = "Unmetered";
static const char opt_name_dns1[] PROGMEM = "DNS 1";
static const char opt_name_dns1_port[] PROGMEM = "DNS 1 Port";
static const char opt_name_dns2[] PROGMEM = "DNS 2";
static const char opt_name_dns2_port[] PROGMEM = "DNS 2 Port";
static const char opt_name_relay[] PROGMEM = "Relay server";
static const char opt_name_p2p_port[] PROGMEM = "P2P Port";
static const char opt_name_baud_rate[] PROGMEM = "Baud rate";
static const char opt_name_wifi_ap_list[] PROGMEM = "WiFi AP list";
static const char opt_name_bt_device_list[] PROGMEM = "BT device list";

// Build the list of available options dynamically based on implementation callbacks
static int reon_build_option_list(struct mobile_adapter *adapter,
                                   struct mobile_reon_option_desc *options,
                                   int max_options)
{
    int count = 0;

    // Implementation name (if callback returns non-NULL)
    const char *impl_name = mobile_cb_reon_impl_name(adapter);
    if (impl_name && count < max_options) {
        options[count].id = MOBILE_REON_OPT_IMPL_NAME;
        options[count].type = MOBILE_REON_TYPE_STRING;
        options[count].flags = MOBILE_REON_FLAG_READONLY;
        options[count].name = opt_name_impl_name;
        count++;
    }

    // User number/friend code (if callback returns non-NULL)
    const char *user_number = mobile_cb_reon_get_number(adapter);
    if (user_number && count < max_options) {
        options[count].id = MOBILE_REON_OPT_USER_NUMBER;
        options[count].type = MOBILE_REON_TYPE_STRING;
        options[count].flags = MOBILE_REON_FLAG_READONLY;
        options[count].name = opt_name_user_number;
        count++;
    }

    // Current IP (if callback returns valid address)
    struct mobile_addr current_ip;
    if (mobile_cb_reon_get_current_ip(adapter, &current_ip) && count < max_options) {
        options[count].id = MOBILE_REON_OPT_CURRENT_IP;
        options[count].type = MOBILE_REON_TYPE_IP;
        options[count].flags = MOBILE_REON_FLAG_READONLY;
        options[count].name = opt_name_current_ip;
        count++;
    }

    // Adapter type (always available, R/W)
    if (count < max_options) {
        options[count].id = MOBILE_REON_OPT_ADAPTER_TYPE;
        options[count].type = MOBILE_REON_TYPE_BYTE;
        options[count].flags = 0;
        options[count].name = opt_name_adapter_type;
        count++;
    }

    // Unmetered (always available, R/W)
    if (count < max_options) {
        options[count].id = MOBILE_REON_OPT_UNMETERED;
        options[count].type = MOBILE_REON_TYPE_BYTE;
        options[count].flags = 0;
        options[count].name = opt_name_unmetered;
        count++;
    }

    // DNS 1 (always available, R/W)
    if (count < max_options) {
        options[count].id = MOBILE_REON_OPT_DNS1;
        options[count].type = MOBILE_REON_TYPE_IP;
        options[count].flags = 0;
        options[count].name = opt_name_dns1;
        count++;
    }

    // DNS 1 Port (always available, R/W)
    if (count < max_options) {
        options[count].id = MOBILE_REON_OPT_DNS1_PORT;
        options[count].type = MOBILE_REON_TYPE_UINT16;
        options[count].flags = 0;
        options[count].name = opt_name_dns1_port;
        count++;
    }

    // DNS 2 (always available, R/W)
    if (count < max_options) {
        options[count].id = MOBILE_REON_OPT_DNS2;
        options[count].type = MOBILE_REON_TYPE_IP;
        options[count].flags = 0;
        options[count].name = opt_name_dns2;
        count++;
    }

    // DNS 2 Port (always available, R/W)
    if (count < max_options) {
        options[count].id = MOBILE_REON_OPT_DNS2_PORT;
        options[count].type = MOBILE_REON_TYPE_UINT16;
        options[count].flags = 0;
        options[count].name = opt_name_dns2_port;
        count++;
    }

    // Relay server (always available, R/W)
    if (count < max_options) {
        options[count].id = MOBILE_REON_OPT_RELAY;
        options[count].type = MOBILE_REON_TYPE_IP;
        options[count].flags = 0;
        options[count].name = opt_name_relay;
        count++;
    }

    // P2P Port (always available, R/W)
    if (count < max_options) {
        options[count].id = MOBILE_REON_OPT_P2P_PORT;
        options[count].type = MOBILE_REON_TYPE_UINT16;
        options[count].flags = 0;
        options[count].name = opt_name_p2p_port;
        count++;
    }

    // Baud rate (if callback returns valid rate)
    unsigned baud_rate;
    bool baud_writable;
    if (mobile_cb_reon_get_baud_rate(adapter, &baud_rate, &baud_writable) && count < max_options) {
        options[count].id = MOBILE_REON_OPT_BAUD_RATE;
        options[count].type = MOBILE_REON_TYPE_UINT32;
        options[count].flags = baud_writable ? 0 : MOBILE_REON_FLAG_READONLY;
        options[count].name = opt_name_baud_rate;
        count++;
    }

    // WiFi AP list (if WiFi scanning is supported)
    unsigned wifi_count;
    if (mobile_cb_reon_wifi_ap_count(adapter, &wifi_count) && count < max_options) {
        options[count].id = MOBILE_REON_OPT_WIFI_AP_LIST;
        options[count].type = MOBILE_REON_TYPE_ARRAY | MOBILE_REON_TYPE_BYTE;
        options[count].flags = MOBILE_REON_FLAG_READONLY;
        options[count].name = opt_name_wifi_ap_list;
        count++;
    }

    // BT device list (if Bluetooth scanning is supported)
    unsigned bt_count;
    if (mobile_cb_reon_bt_device_count(adapter, &bt_count) && count < max_options) {
        options[count].id = MOBILE_REON_OPT_BT_DEVICE_LIST;
        options[count].type = MOBILE_REON_TYPE_ARRAY | MOBILE_REON_TYPE_BYTE;
        options[count].flags = MOBILE_REON_FLAG_READONLY;
        options[count].name = opt_name_bt_device_list;
        count++;
    }

    // Note: Custom options (0x30+) are handled separately in GET_OPTIONS
    // because they require dynamic name retrieval

    return count;
}

// Get count of custom options from adapter
static unsigned reon_get_custom_count(struct mobile_adapter *adapter)
{
    unsigned count;
    if (mobile_cb_reon_custom_count(adapter, &count)) {
        return count;
    }
    return 0;
}

// Find option descriptor by ID
static const struct mobile_reon_option_desc *reon_find_option(
    struct mobile_adapter *adapter,
    unsigned char id,
    struct mobile_reon_option_desc *options,
    int count)
{
    (void)adapter;
    for (int i = 0; i < count; i++) {
        if (options[i].id == id) return &options[i];
    }
    return NULL;
}

// GET_REON_CONFIG_OPTIONS (0x30)
// Request: [offset:1] [count:1]
// Response: [total:1] [returned:1] [options...]
struct mobile_packet *command_reon_get_options(struct mobile_adapter *adapter,
                                                struct mobile_packet *packet)
{
    // Note: REON mode check is done at serial layer - commands don't exist when not in REON mode

    // Validate request
    if (packet->length < 2) {
        return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
    }

    unsigned char offset = packet->data[0];
    unsigned char req_count = packet->data[1];

    if (req_count == 0 || req_count > MOBILE_REON_MAX_OPTIONS_PER_PAGE) {
        return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
    }

    // Build core option list
    struct mobile_reon_option_desc options[20];
    int core_count = reon_build_option_list(adapter, options, 20);
    unsigned custom_count = reon_get_custom_count(adapter);
    int total = core_count + (int)custom_count;

    // Calculate how many options to return
    int start = offset;
    if (start >= total) start = total;
    int count = total - start;
    if (count > (int)req_count) count = req_count;

    // Build response
    unsigned char *out = packet->data;
    *out++ = total;       // Total options available
    *out++ = count;       // Options in this response

    int returned = 0;
    for (int i = 0; i < count; i++) {
        int idx = start + i;

        unsigned char id, type, flags;
        const char *name;
        char custom_name[64];
        bool is_custom = (idx >= core_count);

        if (is_custom) {
            // Custom option - get descriptor from callback
            unsigned custom_idx = idx - core_count;
            if (!mobile_cb_reon_custom_get_desc(adapter, custom_idx, &id, &type, &flags, custom_name)) {
                continue;
            }
            name = custom_name;
        } else {
            // Core option
            const struct mobile_reon_option_desc *opt = &options[idx];
            id = opt->id;
            type = opt->type;
            flags = opt->flags;
            name = opt->name;
        }

        size_t name_len = is_custom ? strlen(name) : strlen_P(name);
        if (name_len > 253) name_len = 253;

        // Check if we have enough space (max packet size is 254)
        if ((out - packet->data) + 4 + name_len > MOBILE_MAX_TRANSFER_SIZE) {
            // Truncate and update returned count
            packet->data[1] = returned;
            break;
        }

        *out++ = id;
        *out++ = type;
        *out++ = flags;
        *out++ = name_len;
        if (is_custom) {
            memcpy(out, name, name_len);
        } else {
            memcpy_P(out, name, name_len);
        }
        out += name_len;
        returned++;
    }

    // Update returned count if we completed normally
    if (returned == count) {
        packet->data[1] = returned;
    }

    packet->length = out - packet->data;
    return packet;
}

// GET_REON_CONFIG_VALUE (0x31)
// Request: [option_id:1] [index:1]? (index only for arrays)
// Response: [option_id:1] [type:1] [value_len:1] [value:N]
struct mobile_packet *command_reon_get_value(struct mobile_adapter *adapter,
                                              struct mobile_packet *packet)
{
    // Note: REON mode check is done at serial layer - commands don't exist when not in REON mode

    // Validate request
    if (packet->length < 1) {
        return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
    }

    unsigned char option_id = packet->data[0];

    // Build option list and find the option
    struct mobile_reon_option_desc options[20];
    int total = reon_build_option_list(adapter, options, 20);
    const struct mobile_reon_option_desc *opt = reon_find_option(adapter, option_id, options, total);

    if (!opt) {
        return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
    }

    unsigned char *out = packet->data;
    *out++ = option_id;

    // Get value based on option ID
    switch (option_id) {
    case MOBILE_REON_OPT_IMPL_NAME: {
        const char *name = mobile_cb_reon_impl_name(adapter);
        if (!name) return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        size_t len = strlen(name);
        if (len > 253) len = 253;
        *out++ = MOBILE_REON_TYPE_STRING;
        *out++ = len;
        memcpy(out, name, len);
        out += len;
        break;
    }

    case MOBILE_REON_OPT_USER_NUMBER: {
        const char *number = mobile_cb_reon_get_number(adapter);
        if (!number) return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        size_t len = strlen(number);
        if (len > MOBILE_MAX_NUMBER_SIZE) len = MOBILE_MAX_NUMBER_SIZE;
        *out++ = MOBILE_REON_TYPE_STRING;
        *out++ = len;
        memcpy(out, number, len);
        out += len;
        break;
    }

    case MOBILE_REON_OPT_CURRENT_IP: {
        struct mobile_addr addr;
        if (!mobile_cb_reon_get_current_ip(adapter, &addr)) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        if (addr.type == MOBILE_ADDRTYPE_IPV4) {
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&addr;
            *out++ = MOBILE_REON_TYPE_IPV4;
            *out++ = MOBILE_HOSTLEN_IPV4;
            memcpy(out, addr4->host, MOBILE_HOSTLEN_IPV4);
            out += MOBILE_HOSTLEN_IPV4;
        } else if (addr.type == MOBILE_ADDRTYPE_IPV6) {
            struct mobile_addr6 *addr6 = (struct mobile_addr6 *)&addr;
            *out++ = MOBILE_REON_TYPE_IPV6;
            *out++ = MOBILE_HOSTLEN_IPV6;
            memcpy(out, addr6->host, MOBILE_HOSTLEN_IPV6);
            out += MOBILE_HOSTLEN_IPV6;
        } else {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        break;
    }

    case MOBILE_REON_OPT_ADAPTER_TYPE: {
        *out++ = MOBILE_REON_TYPE_BYTE;
        *out++ = 1;
        *out++ = adapter->serial.device;
        break;
    }

    case MOBILE_REON_OPT_UNMETERED: {
        *out++ = MOBILE_REON_TYPE_BYTE;
        *out++ = 1;
        *out++ = adapter->serial.device_unmetered ? 0x01 : 0x00;
        break;
    }

    case MOBILE_REON_OPT_DNS1: {
        struct mobile_addr dns;
        mobile_config_get_dns(adapter, &dns, MOBILE_DNS1);
        if (dns.type == MOBILE_ADDRTYPE_IPV4) {
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&dns;
            *out++ = MOBILE_REON_TYPE_IPV4;
            *out++ = MOBILE_HOSTLEN_IPV4;
            memcpy(out, addr4->host, MOBILE_HOSTLEN_IPV4);
            out += MOBILE_HOSTLEN_IPV4;
        } else if (dns.type == MOBILE_ADDRTYPE_IPV6) {
            struct mobile_addr6 *addr6 = (struct mobile_addr6 *)&dns;
            *out++ = MOBILE_REON_TYPE_IPV6;
            *out++ = MOBILE_HOSTLEN_IPV6;
            memcpy(out, addr6->host, MOBILE_HOSTLEN_IPV6);
            out += MOBILE_HOSTLEN_IPV6;
        } else {
            // No address configured
            *out++ = MOBILE_REON_TYPE_IPV4;
            *out++ = MOBILE_HOSTLEN_IPV4;
            memset(out, 0, MOBILE_HOSTLEN_IPV4);
            out += MOBILE_HOSTLEN_IPV4;
        }
        break;
    }

    case MOBILE_REON_OPT_DNS1_PORT: {
        struct mobile_addr dns;
        mobile_config_get_dns(adapter, &dns, MOBILE_DNS1);
        unsigned port = MOBILE_DNS_PORT;
        if (dns.type == MOBILE_ADDRTYPE_IPV4) {
            port = ((struct mobile_addr4 *)&dns)->port;
        } else if (dns.type == MOBILE_ADDRTYPE_IPV6) {
            port = ((struct mobile_addr6 *)&dns)->port;
        }
        *out++ = MOBILE_REON_TYPE_UINT16;
        *out++ = 2;
        *out++ = (port >> 8) & 0xFF;
        *out++ = port & 0xFF;
        break;
    }

    case MOBILE_REON_OPT_DNS2: {
        struct mobile_addr dns;
        mobile_config_get_dns(adapter, &dns, MOBILE_DNS2);
        if (dns.type == MOBILE_ADDRTYPE_IPV4) {
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&dns;
            *out++ = MOBILE_REON_TYPE_IPV4;
            *out++ = MOBILE_HOSTLEN_IPV4;
            memcpy(out, addr4->host, MOBILE_HOSTLEN_IPV4);
            out += MOBILE_HOSTLEN_IPV4;
        } else if (dns.type == MOBILE_ADDRTYPE_IPV6) {
            struct mobile_addr6 *addr6 = (struct mobile_addr6 *)&dns;
            *out++ = MOBILE_REON_TYPE_IPV6;
            *out++ = MOBILE_HOSTLEN_IPV6;
            memcpy(out, addr6->host, MOBILE_HOSTLEN_IPV6);
            out += MOBILE_HOSTLEN_IPV6;
        } else {
            *out++ = MOBILE_REON_TYPE_IPV4;
            *out++ = MOBILE_HOSTLEN_IPV4;
            memset(out, 0, MOBILE_HOSTLEN_IPV4);
            out += MOBILE_HOSTLEN_IPV4;
        }
        break;
    }

    case MOBILE_REON_OPT_DNS2_PORT: {
        struct mobile_addr dns;
        mobile_config_get_dns(adapter, &dns, MOBILE_DNS2);
        unsigned port = MOBILE_DNS_PORT;
        if (dns.type == MOBILE_ADDRTYPE_IPV4) {
            port = ((struct mobile_addr4 *)&dns)->port;
        } else if (dns.type == MOBILE_ADDRTYPE_IPV6) {
            port = ((struct mobile_addr6 *)&dns)->port;
        }
        *out++ = MOBILE_REON_TYPE_UINT16;
        *out++ = 2;
        *out++ = (port >> 8) & 0xFF;
        *out++ = port & 0xFF;
        break;
    }

    case MOBILE_REON_OPT_RELAY: {
        struct mobile_addr relay;
        mobile_config_get_relay(adapter, &relay);
        if (relay.type == MOBILE_ADDRTYPE_IPV4) {
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&relay;
            *out++ = MOBILE_REON_TYPE_IPV4;
            *out++ = MOBILE_HOSTLEN_IPV4;
            memcpy(out, addr4->host, MOBILE_HOSTLEN_IPV4);
            out += MOBILE_HOSTLEN_IPV4;
        } else if (relay.type == MOBILE_ADDRTYPE_IPV6) {
            struct mobile_addr6 *addr6 = (struct mobile_addr6 *)&relay;
            *out++ = MOBILE_REON_TYPE_IPV6;
            *out++ = MOBILE_HOSTLEN_IPV6;
            memcpy(out, addr6->host, MOBILE_HOSTLEN_IPV6);
            out += MOBILE_HOSTLEN_IPV6;
        } else {
            *out++ = MOBILE_REON_TYPE_IPV4;
            *out++ = MOBILE_HOSTLEN_IPV4;
            memset(out, 0, MOBILE_HOSTLEN_IPV4);
            out += MOBILE_HOSTLEN_IPV4;
        }
        break;
    }

    case MOBILE_REON_OPT_P2P_PORT: {
        unsigned p2p_port;
        mobile_config_get_p2p_port(adapter, &p2p_port);
        *out++ = MOBILE_REON_TYPE_UINT16;
        *out++ = 2;
        *out++ = (p2p_port >> 8) & 0xFF;
        *out++ = p2p_port & 0xFF;
        break;
    }

    case MOBILE_REON_OPT_BAUD_RATE: {
        unsigned baud_rate;
        bool writable;
        if (!mobile_cb_reon_get_baud_rate(adapter, &baud_rate, &writable)) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        *out++ = MOBILE_REON_TYPE_UINT32;
        *out++ = 4;
        *out++ = (baud_rate >> 24) & 0xFF;
        *out++ = (baud_rate >> 16) & 0xFF;
        *out++ = (baud_rate >> 8) & 0xFF;
        *out++ = baud_rate & 0xFF;
        break;
    }

    case MOBILE_REON_OPT_WIFI_AP_LIST: {
        // Array type - need index byte
        if (packet->length < 2) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        unsigned char index = packet->data[1];
        unsigned ap_count;
        if (!mobile_cb_reon_wifi_ap_count(adapter, &ap_count)) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }

        if (index == MOBILE_REON_ARRAY_GET_COUNT) {
            // Return count
            *out++ = MOBILE_REON_TYPE_ARRAY | MOBILE_REON_TYPE_BYTE;
            *out++ = 1;
            *out++ = ap_count;
        } else {
            // Return element
            if (index >= ap_count) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            char ssid[33];
            signed char rssi;
            unsigned char security;
            if (!mobile_cb_reon_wifi_ap_get(adapter, index, ssid, &rssi, &security)) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            // Format: [ssid_len:1] [ssid:N] [rssi:1] [security:1]
            size_t ssid_len = strlen(ssid);
            if (ssid_len > 32) ssid_len = 32;
            *out++ = MOBILE_REON_TYPE_ARRAY | MOBILE_REON_TYPE_BYTE;
            *out++ = 1 + ssid_len + 2;  // ssid_len byte + ssid + rssi + security
            *out++ = ssid_len;
            memcpy(out, ssid, ssid_len);
            out += ssid_len;
            *out++ = (unsigned char)rssi;
            *out++ = security;
        }
        break;
    }

    case MOBILE_REON_OPT_BT_DEVICE_LIST: {
        // Array type - need index byte
        if (packet->length < 2) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        unsigned char index = packet->data[1];
        unsigned bt_count;
        if (!mobile_cb_reon_bt_device_count(adapter, &bt_count)) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }

        if (index == MOBILE_REON_ARRAY_GET_COUNT) {
            // Return count
            *out++ = MOBILE_REON_TYPE_ARRAY | MOBILE_REON_TYPE_BYTE;
            *out++ = 1;
            *out++ = bt_count;
        } else {
            // Return element
            if (index >= bt_count) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            unsigned char mac[6];
            char name[249];
            if (!mobile_cb_reon_bt_device_get(adapter, index, mac, name)) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            // Format: [mac:6] [name_len:1] [name:N]
            size_t name_len = strlen(name);
            if (name_len > 248) name_len = 248;
            *out++ = MOBILE_REON_TYPE_ARRAY | MOBILE_REON_TYPE_BYTE;
            *out++ = 6 + 1 + name_len;  // mac + name_len byte + name
            memcpy(out, mac, 6);
            out += 6;
            *out++ = name_len;
            memcpy(out, name, name_len);
            out += name_len;
        }
        break;
    }

    default:
        // Check for custom options (0x30+)
        if (option_id >= 0x30) {
            // Let the adapter handle custom option value retrieval
            // Buffer format: [type:1] [len:1] [value:N]
            int written = mobile_cb_reon_custom_get_value(adapter, option_id,
                out, MOBILE_MAX_TRANSFER_SIZE - (out - packet->data));
            if (written <= 0) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            out += written;
            break;
        }
        return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
    }

    packet->length = out - packet->data;
    return packet;
}

// SET_REON_CONFIG_VALUE (0x32)
// Request: [option_id:1] [type:1] [value_len:1] [value:N]
// Response: [option_id:1]
struct mobile_packet *command_reon_set_value(struct mobile_adapter *adapter,
                                              struct mobile_packet *packet)
{
    // Note: REON mode check is done at serial layer - commands don't exist when not in REON mode

    // Validate request
    if (packet->length < 3) {
        return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
    }

    unsigned char option_id = packet->data[0];
    unsigned char type = packet->data[1];
    unsigned char value_len = packet->data[2];
    unsigned char *value = packet->data + 3;

    if (packet->length < 3 + value_len) {
        return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
    }

    // Build option list and find the option
    struct mobile_reon_option_desc options[20];
    int total = reon_build_option_list(adapter, options, 20);
    const struct mobile_reon_option_desc *opt = reon_find_option(adapter, option_id, options, total);

    if (!opt) {
        // Custom options (0x30+) won't be in core list - allow them through
        if (option_id < 0x30) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
    } else {
        // Check if read-only (only for core options that we found)
        if (opt->flags & MOBILE_REON_FLAG_READONLY) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_READONLY);
        }
    }

    // Set value based on option ID
    switch (option_id) {
    case MOBILE_REON_OPT_ADAPTER_TYPE: {
        if (type != MOBILE_REON_TYPE_BYTE || value_len != 1) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        enum mobile_adapter_device device;
        bool unmetered;
        mobile_config_get_device(adapter, &device, &unmetered);
        device = (enum mobile_adapter_device)value[0];
        mobile_config_set_device(adapter, device, unmetered);
        break;
    }

    case MOBILE_REON_OPT_UNMETERED: {
        if (type != MOBILE_REON_TYPE_BYTE || value_len != 1) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        enum mobile_adapter_device device;
        bool unmetered;
        mobile_config_get_device(adapter, &device, &unmetered);
        unmetered = (value[0] != 0);
        mobile_config_set_device(adapter, device, unmetered);
        break;
    }

    case MOBILE_REON_OPT_DNS1: {
        struct mobile_addr dns = {0};
        if (type == MOBILE_REON_TYPE_IPV4) {
            if (value_len != MOBILE_HOSTLEN_IPV4) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&dns;
            addr4->type = MOBILE_ADDRTYPE_IPV4;
            addr4->port = MOBILE_DNS_PORT;
            memcpy(addr4->host, value, MOBILE_HOSTLEN_IPV4);
        } else if (type == MOBILE_REON_TYPE_IPV6) {
            if (value_len != MOBILE_HOSTLEN_IPV6) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            struct mobile_addr6 *addr6 = (struct mobile_addr6 *)&dns;
            addr6->type = MOBILE_ADDRTYPE_IPV6;
            addr6->port = MOBILE_DNS_PORT;
            memcpy(addr6->host, value, MOBILE_HOSTLEN_IPV6);
        } else {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        mobile_config_set_dns(adapter, &dns, MOBILE_DNS1);
        break;
    }

    case MOBILE_REON_OPT_DNS1_PORT: {
        if (type != MOBILE_REON_TYPE_UINT16 || value_len != 2) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        // DNS port is stored within the address structure
        // For now, we update the port in the existing DNS address
        struct mobile_addr dns;
        mobile_config_get_dns(adapter, &dns, MOBILE_DNS1);
        unsigned port = (value[0] << 8) | value[1];
        if (dns.type == MOBILE_ADDRTYPE_IPV4) {
            ((struct mobile_addr4 *)&dns)->port = port;
        } else if (dns.type == MOBILE_ADDRTYPE_IPV6) {
            ((struct mobile_addr6 *)&dns)->port = port;
        } else {
            // No address, create empty IPv4 with this port
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&dns;
            addr4->type = MOBILE_ADDRTYPE_IPV4;
            addr4->port = port;
            memset(addr4->host, 0, MOBILE_HOSTLEN_IPV4);
        }
        mobile_config_set_dns(adapter, &dns, MOBILE_DNS1);
        break;
    }

    case MOBILE_REON_OPT_DNS2: {
        struct mobile_addr dns = {0};
        if (type == MOBILE_REON_TYPE_IPV4) {
            if (value_len != MOBILE_HOSTLEN_IPV4) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&dns;
            addr4->type = MOBILE_ADDRTYPE_IPV4;
            addr4->port = MOBILE_DNS_PORT;
            memcpy(addr4->host, value, MOBILE_HOSTLEN_IPV4);
        } else if (type == MOBILE_REON_TYPE_IPV6) {
            if (value_len != MOBILE_HOSTLEN_IPV6) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            struct mobile_addr6 *addr6 = (struct mobile_addr6 *)&dns;
            addr6->type = MOBILE_ADDRTYPE_IPV6;
            addr6->port = MOBILE_DNS_PORT;
            memcpy(addr6->host, value, MOBILE_HOSTLEN_IPV6);
        } else {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        mobile_config_set_dns(adapter, &dns, MOBILE_DNS2);
        break;
    }

    case MOBILE_REON_OPT_DNS2_PORT: {
        if (type != MOBILE_REON_TYPE_UINT16 || value_len != 2) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        struct mobile_addr dns;
        mobile_config_get_dns(adapter, &dns, MOBILE_DNS2);
        unsigned port = (value[0] << 8) | value[1];
        if (dns.type == MOBILE_ADDRTYPE_IPV4) {
            ((struct mobile_addr4 *)&dns)->port = port;
        } else if (dns.type == MOBILE_ADDRTYPE_IPV6) {
            ((struct mobile_addr6 *)&dns)->port = port;
        } else {
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&dns;
            addr4->type = MOBILE_ADDRTYPE_IPV4;
            addr4->port = port;
            memset(addr4->host, 0, MOBILE_HOSTLEN_IPV4);
        }
        mobile_config_set_dns(adapter, &dns, MOBILE_DNS2);
        break;
    }

    case MOBILE_REON_OPT_RELAY: {
        struct mobile_addr relay = {0};
        if (type == MOBILE_REON_TYPE_IPV4) {
            if (value_len != MOBILE_HOSTLEN_IPV4) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            struct mobile_addr4 *addr4 = (struct mobile_addr4 *)&relay;
            addr4->type = MOBILE_ADDRTYPE_IPV4;
            addr4->port = MOBILE_DEFAULT_RELAY_PORT;
            memcpy(addr4->host, value, MOBILE_HOSTLEN_IPV4);
        } else if (type == MOBILE_REON_TYPE_IPV6) {
            if (value_len != MOBILE_HOSTLEN_IPV6) {
                return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
            }
            struct mobile_addr6 *addr6 = (struct mobile_addr6 *)&relay;
            addr6->type = MOBILE_ADDRTYPE_IPV6;
            addr6->port = MOBILE_DEFAULT_RELAY_PORT;
            memcpy(addr6->host, value, MOBILE_HOSTLEN_IPV6);
        } else {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        mobile_config_set_relay(adapter, &relay);
        break;
    }

    case MOBILE_REON_OPT_P2P_PORT: {
        if (type != MOBILE_REON_TYPE_UINT16 || value_len != 2) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        unsigned port = (value[0] << 8) | value[1];
        mobile_config_set_p2p_port(adapter, port);
        break;
    }

    case MOBILE_REON_OPT_BAUD_RATE: {
        if (type != MOBILE_REON_TYPE_UINT32 || value_len != 4) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        unsigned baud = ((unsigned)value[0] << 24) | ((unsigned)value[1] << 16) |
                        ((unsigned)value[2] << 8) | value[3];
        if (!mobile_cb_reon_set_baud_rate(adapter, baud)) {
            return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
        }
        break;
    }

    default:
        // Check for custom options (0x30+)
        if (option_id >= 0x30) {
            // Let the adapter handle custom option value setting
            int result = mobile_cb_reon_custom_set_value(adapter, option_id,
                type, value, value_len);
            if (result != 0) {
                // Return error code from callback
                return mobile_reon_error_packet(packet, result);
            }
            break;
        }
        return mobile_reon_error_packet(packet, MOBILE_REON_ERROR_INVALID_PARAM);
    }

    // Success - return option ID
    packet->data[0] = option_id;
    packet->length = 1;
    return packet;
}

// Helper to create error packet for REON commands
struct mobile_packet *mobile_reon_error_packet(struct mobile_packet *packet,
                                                unsigned char error)
{
    enum mobile_command command = packet->command;
    packet->command = MOBILE_COMMAND_ERROR;
    packet->data[0] = command;
    packet->data[1] = error;
    packet->length = 2;
    return packet;
}
