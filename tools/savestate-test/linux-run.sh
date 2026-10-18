#!/bin/sh
# Runs the game headless on Linux (Docker, GCC, Xvfb + Mesa llvmpipe) with a test trigger
# compiled into a COPY of the source, and prints the checkpoint log lines.
#   tools/savestate-test/linux-run.sh <rom.z64> <saveframe>,<loadframe> [seconds]
# The source tree is mounted read-only; nothing in the repo is modified.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
ROM=$1
FRAMES=$2
SECS=${3:-280}
[ -f "$ROM" ] && [ -n "$FRAMES" ] || { echo "usage: linux-run.sh <rom.z64> <saveframe>,<loadframe> [seconds]" >&2; exit 2; }
case "$FRAMES" in *[!0-9,]*) echo "frames must be <n>,<n>" >&2; exit 2;; esac
case "$SECS" in *[!0-9]*) echo "seconds must be a number" >&2; exit 2;; esac
ROMABS=$(cd "$(dirname "$ROM")" && pwd)/$(basename "$ROM")
docker build -q -t pd-linux-test -f "$ROOT/tools/savestate-test/Dockerfile.linux" "$ROOT/tools/savestate-test" >/dev/null
docker run --rm --network none -v "$ROOT":/src:ro -v "$ROMABS":/rom.z64:ro -e PD_CHECKPOINT_TEST="$FRAMES" -e SECS="$SECS" pd-linux-test sh -c '
set -e
cp -r /src /w && rm -rf /w/build
perl -0pi -e "s/(void savestateTick\\(void\\)\\n\\{\\n)/\$1#include \"..\\/..\\/tools\\/savestate-test\\/test-hook.inc\"\\n/" /w/port/src/savestate.c
sed -i "1i #include <stdio.h>" /w/port/src/savestate.c
grep -q test-hook.inc /w/port/src/savestate.c || { echo "hook not inserted"; exit 1; }
cmake -S /w -B /b -G Ninja >/dev/null
cmake --build /b -j"$(nproc)" >/b/build.log 2>&1 || { grep -E "error" /b/build.log | head; exit 1; }
mkdir -p /r/data && cp /rom.z64 /r/data/pd.ntsc-final.z64 && cd /r
rc=0
BIN=$(ls /b/pd.* | head -1); SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 640x480x24" timeout "$SECS" stdbuf -oL -eL "$BIN" --basedir /r/data --savedir /r --boot-stage 48 --profile 0 --log > /r/out.log 2>&1 || rc=$?
grep -E "savestate|boot stage|Segmentation|FATAL|Assert" /r/out.log | tail -20
echo "exit=$rc (124 = stopped by the time limit)"
'
