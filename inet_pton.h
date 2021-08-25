// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <stddef.h>

#define MOBILE_PTON_ANY 0
#define MOBILE_PTON_IPV4 2
#define MOBILE_PTON_IPV6 26

#define MOBILE_PTON_MAXLEN 16

int mobile_pton_length(int af, const char *src, size_t srclen, void *dst);
int mobile_pton(int af, const char *src, void *dst);
