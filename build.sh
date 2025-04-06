#!/usr/bin/env bash

set -xe

gcc                                 \
    -pipe                           \
    -Wall -Wextra -Werror -pedantic \
        -Wno-analyzer-malloc-leak   \
    -std=c99 -Og -ggdb              \
        -D_FORTIFY_SOURCE=3         \
        -fstack-protector-strong    \
        -fstack-clash-protection    \
        -fcf-protection=full        \
        -fanalyzer                  \
    -o fwuck fwuck.c                \
    -lcurl
