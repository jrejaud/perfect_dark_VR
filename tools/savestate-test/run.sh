#!/bin/sh
# Builds and runs the checkpoint arena host test. Usage: tools/savestate-test/run.sh
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${TMPDIR:-/tmp}/pd-arena-test
cc -std=c11 -D_LANGUAGE_C=1 -DPLATFORM_64BIT=1 -O1 -g -fsanitize=address,undefined -Wall -I"$ROOT/include" -I"$ROOT/port/include" \
	"$ROOT/port/src/savestate_arena.c" "$ROOT/tools/savestate-test/arena_test.c" -o "$OUT"
"$OUT"
# The Switch has no mmap/VirtualAlloc path; CI skips that job on forks, so at least make
# sure the arena still compiles with it defined.
cc -std=c11 -fsyntax-only -Wall -D_LANGUAGE_C=1 -DPLATFORM_64BIT=1 -D__SWITCH__ -I"$ROOT/include" -I"$ROOT/port/include" \
	"$ROOT/port/src/savestate_arena.c" && echo "arena (__SWITCH__): compiles"
