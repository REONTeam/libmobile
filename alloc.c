// SPDX-License-Identifier: LGPL-3.0-or-later

#include <stddef.h>

#include "mobile_data.h"
#include "compat.h"

#ifdef MOBILE_LIBCONF_USE
#include <mobile_config.h>
#endif

const size_t mobile_sizeof PROGMEM = sizeof(struct mobile_adapter);

#ifndef MOBILE_ENABLE_NOALLOC
#include <stdlib.h>

struct mobile_adapter *mobile_new(void *user)
{
    struct mobile_adapter *adapter = malloc(sizeof(struct mobile_adapter));
    mobile_init(adapter, user);
    return adapter;
}
#endif
