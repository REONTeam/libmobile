// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// REON Config Protocol definitions

#include <stddef.h>
#include <stdbool.h>

// REON Config Data Types
enum mobile_reon_type {
    MOBILE_REON_TYPE_BYTE = 0x00,
    MOBILE_REON_TYPE_UINT16 = 0x01,
    MOBILE_REON_TYPE_STRING = 0x02,
    MOBILE_REON_TYPE_IPV4 = 0x03,
    MOBILE_REON_TYPE_IPV6 = 0x04,
    MOBILE_REON_TYPE_IP = 0x05,    // Polymorphic: can be IPV4 or IPV6
    MOBILE_REON_TYPE_MAC = 0x06,
    MOBILE_REON_TYPE_UINT32 = 0x07,

    // Array flag (bit 7)
    MOBILE_REON_TYPE_ARRAY = 0x80
};

// REON Config Option Flags
enum mobile_reon_flags {
    MOBILE_REON_FLAG_READONLY = 0x01,
    MOBILE_REON_FLAG_MASKED = 0x02      // Value should be masked in UI (passwords)
};

// REON Config Option IDs
enum mobile_reon_option {
    // Core Options (0x01-0x0B)
    MOBILE_REON_OPT_IMPL_NAME = 0x01,     // Implementation name (readonly)
    MOBILE_REON_OPT_USER_NUMBER = 0x02,   // User's number/friend code (readonly)
    MOBILE_REON_OPT_CURRENT_IP = 0x03,    // Adapter's current IP address (readonly)
    MOBILE_REON_OPT_ADAPTER_TYPE = 0x04,  // Emulated adapter model
    MOBILE_REON_OPT_UNMETERED = 0x05,     // Metered/unmetered connection
    MOBILE_REON_OPT_DNS1 = 0x06,          // Primary DNS server
    MOBILE_REON_OPT_DNS1_PORT = 0x07,     // Primary DNS port
    MOBILE_REON_OPT_DNS2 = 0x08,          // Secondary DNS server
    MOBILE_REON_OPT_DNS2_PORT = 0x09,     // Secondary DNS port
    MOBILE_REON_OPT_RELAY = 0x0A,         // P2P relay server
    MOBILE_REON_OPT_P2P_PORT = 0x0B,      // Direct P2P port
    MOBILE_REON_OPT_BAUD_RATE = 0x0C,     // Serial baud rate (may be readonly)

    // Reserved (0x0D-0x0F)

    // WiFi Options (0x10-0x12)
    MOBILE_REON_OPT_WIFI_SSID = 0x10,
    MOBILE_REON_OPT_WIFI_PASSWORD = 0x11,
    MOBILE_REON_OPT_WIFI_AP_LIST = 0x12,

    // Bluetooth Options (0x13-0x17)
    MOBILE_REON_OPT_BT_DEVICE_NAME = 0x13,
    MOBILE_REON_OPT_BT_PAIRED_DEVICE = 0x14,
    MOBILE_REON_OPT_BT_PAIRED_ADDRESS = 0x15,
    MOBILE_REON_OPT_BT_PIN = 0x16,
    MOBILE_REON_OPT_BT_DEVICE_LIST = 0x17,

    // Reserved (0x18-0x2F)
    // Adapter-specific options start at 0x30
};

// REON Config Option descriptor (used internally)
struct mobile_reon_option_desc {
    unsigned char id;
    unsigned char type;
    unsigned char flags;
    const char *name;
};

// Maximum options to return per page (protocol limit)
#define MOBILE_REON_MAX_OPTIONS_PER_PAGE 10

// Array index to get count
#define MOBILE_REON_ARRAY_GET_COUNT 0xFF

// Error codes for REON commands
#define MOBILE_REON_ERROR_INVALID_PARAM 1
#define MOBILE_REON_ERROR_READONLY 2

// Forward declarations
struct mobile_adapter;
struct mobile_packet;

// REON command handlers
struct mobile_packet *command_reon_get_options(struct mobile_adapter *adapter,
                                                struct mobile_packet *packet);
struct mobile_packet *command_reon_get_value(struct mobile_adapter *adapter,
                                              struct mobile_packet *packet);
struct mobile_packet *command_reon_set_value(struct mobile_adapter *adapter,
                                              struct mobile_packet *packet);

// Helper function
struct mobile_packet *mobile_reon_error_packet(struct mobile_packet *packet,
                                                unsigned char error);
