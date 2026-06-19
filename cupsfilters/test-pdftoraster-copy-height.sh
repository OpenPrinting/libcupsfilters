#!/usr/bin/env bash
#
# Test that pdftoraster copy_height fix works correctly.
# This reproduces the off-by-one logic without needing ASan.
set -euo pipefail

SRC="$(dirname "$0")/test-pdftoraster-copy-height.c"
BIN="/tmp/test-pdftoraster-copy-height"

# Compile if needed
if [ ! -x "$BIN" ] || [ "$SRC" -nt "$BIN" ]; then
  "${CC:-cc}" -std=c11 -O0 -g -Wall -Wextra \
    "$SRC" -o "$BIN"
fi

exec "$BIN"
