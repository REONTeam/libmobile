// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Header containing utility functions for dealing with IP addresses
// May be used by any program using the library

#include <stddef.h>

#define MOBILE_INET_PTON_ANY 0
#define MOBILE_INET_PTON_IPV4 2
#define MOBILE_INET_PTON_IPV6 26

#define MOBILE_INET_PTON_MAXLEN 16

int mobile_inet_pton_length(int af, const char *src, size_t srclen, void *dst);
int mobile_inet_pton(int af, const char *src, void *dst);
