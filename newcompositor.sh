#!/bin/sh

LD_PRELOAD="$(dirname "$0")/../lib64/libnewcompositorhacks.so" "$(dirname "$0")/newcompositor.bin" "$@"
