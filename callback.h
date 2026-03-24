// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "mobile.h"

#ifdef MOBILE_LIBCONF_USE
#include <mobile_config.h>
#endif

enum mobile_timers {
    MOBILE_TIMER_SERIAL,
    MOBILE_TIMER_COMMAND,
    MOBILE_TIMER_RESERVED3,
    MOBILE_TIMER_RESERVED4,
    _MOBILE_MAX_TIMERS
};

struct mobile_adapter_callback {
#ifndef MOBILE_ENABLE_IMPL_WEAK
    mobile_func_debug_log debug_log;
    mobile_func_serial_disable serial_disable;
    mobile_func_serial_enable serial_enable;
    mobile_func_config_read config_read;
    mobile_func_config_write config_write;
    mobile_func_time_latch time_latch;
    mobile_func_time_check_ms time_check_ms;
    mobile_func_sock_open sock_open;
    mobile_func_sock_close sock_close;
    mobile_func_sock_connect sock_connect;
    mobile_func_sock_listen sock_listen;
    mobile_func_sock_accept sock_accept;
    mobile_func_sock_send sock_send;
    mobile_func_sock_recv sock_recv;
    mobile_func_update_number update_number;
    mobile_func_reon_impl_name reon_impl_name;
    mobile_func_reon_get_number reon_get_number;
    mobile_func_reon_get_current_ip reon_get_current_ip;
    mobile_func_reon_get_baud_rate reon_get_baud_rate;
    mobile_func_reon_set_baud_rate reon_set_baud_rate;
    mobile_func_reon_wifi_ap_count reon_wifi_ap_count;
    mobile_func_reon_wifi_ap_get reon_wifi_ap_get;
    mobile_func_reon_bt_device_count reon_bt_device_count;
    mobile_func_reon_bt_device_get reon_bt_device_get;
    mobile_func_reon_custom_count reon_custom_count;
    mobile_func_reon_custom_get_desc reon_custom_get_desc;
    mobile_func_reon_custom_get_value reon_custom_get_value;
    mobile_func_reon_custom_set_value reon_custom_set_value;
#endif
};
void mobile_callback_init(struct mobile_adapter *adapter);

#ifdef MOBILE_ENABLE_IMPL_WEAK
#define mobile_cb(name, adapter, ...) \
    mobile_impl_ ## name(adapter->user, ##__VA_ARGS__)
#else
#define mobile_cb(name, adapter, ...) \
    adapter->callback.name(adapter->user, ##__VA_ARGS__)
#endif

// Help MSVC expand __VA_ARGS__
#define _mobile_cb_e(x) x
#define _mobile_cb(name, ...) _mobile_cb_e(mobile_cb(name, __VA_ARGS__))

#define mobile_cb_debug_log(...) _mobile_cb(debug_log, __VA_ARGS__)
#define mobile_cb_serial_disable(...) _mobile_cb(serial_disable, __VA_ARGS__)
#define mobile_cb_serial_enable(...) _mobile_cb(serial_enable, __VA_ARGS__)
#define mobile_cb_config_read(...) _mobile_cb(config_read, __VA_ARGS__)
#define mobile_cb_config_write(...) _mobile_cb(config_write, __VA_ARGS__)
#define mobile_cb_time_latch(...) _mobile_cb(time_latch, __VA_ARGS__)
#define mobile_cb_time_check_ms(...) _mobile_cb(time_check_ms, __VA_ARGS__)
#define mobile_cb_sock_open(...) _mobile_cb(sock_open, __VA_ARGS__)
#define mobile_cb_sock_close(...) _mobile_cb(sock_close, __VA_ARGS__)
#define mobile_cb_sock_connect(...) _mobile_cb(sock_connect, __VA_ARGS__)
#define mobile_cb_sock_listen(...) _mobile_cb(sock_listen, __VA_ARGS__)
#define mobile_cb_sock_accept(...) _mobile_cb(sock_accept, __VA_ARGS__)
#define mobile_cb_sock_send(...) _mobile_cb(sock_send, __VA_ARGS__)
#define mobile_cb_sock_recv(...) _mobile_cb(sock_recv, __VA_ARGS__)
#define mobile_cb_update_number(...) _mobile_cb(update_number, __VA_ARGS__)
#define mobile_cb_reon_impl_name(...) _mobile_cb(reon_impl_name, __VA_ARGS__)
#define mobile_cb_reon_get_number(...) _mobile_cb(reon_get_number, __VA_ARGS__)
#define mobile_cb_reon_get_current_ip(...) _mobile_cb(reon_get_current_ip, __VA_ARGS__)
#define mobile_cb_reon_get_baud_rate(...) _mobile_cb(reon_get_baud_rate, __VA_ARGS__)
#define mobile_cb_reon_set_baud_rate(...) _mobile_cb(reon_set_baud_rate, __VA_ARGS__)
#define mobile_cb_reon_wifi_ap_count(...) _mobile_cb(reon_wifi_ap_count, __VA_ARGS__)
#define mobile_cb_reon_wifi_ap_get(...) _mobile_cb(reon_wifi_ap_get, __VA_ARGS__)
#define mobile_cb_reon_bt_device_count(...) _mobile_cb(reon_bt_device_count, __VA_ARGS__)
#define mobile_cb_reon_bt_device_get(...) _mobile_cb(reon_bt_device_get, __VA_ARGS__)
#define mobile_cb_reon_custom_count(...) _mobile_cb(reon_custom_count, __VA_ARGS__)
#define mobile_cb_reon_custom_get_desc(...) _mobile_cb(reon_custom_get_desc, __VA_ARGS__)
#define mobile_cb_reon_custom_get_value(...) _mobile_cb(reon_custom_get_value, __VA_ARGS__)
#define mobile_cb_reon_custom_set_value(...) _mobile_cb(reon_custom_set_value, __VA_ARGS__)
