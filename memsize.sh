#!/bin/sh
# SPDX-License-Identifier: LGPL-3.0-or-later
set -e

# This program outputs the size of "struct mobile_adapter" for the given HOST

name="${1:-mobile_adapter}"

${HOST}cc -fshort-enums -o memsize.o -c -xc - << EOF
#define MOBILE_ENABLE_IMPL_WEAK
#include "mobile_data.h"
struct $name adapter;
EOF
size="$(${HOST}nm -S memsize.o | grep 'adapter$' | awk '{print strtonum("0x"$2)}')"
rm -f memsize.o
echo "Size: $size"
