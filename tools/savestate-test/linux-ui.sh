#!/bin/sh
# Agent UI capture: builds a checkout headless on Linux (Docker, GCC, Xvfb + llvmpipe), boots it
# into a stage, plays a step file with real key presses, and saves screenshots plus a video of
# the whole run. Run it on the code BEFORE a UI change and AFTER it, then compare the two.
#
#   tools/savestate-test/linux-ui.sh <checkout> <rom.z64> <steps> <outdir> [stage|title] [eeprom.bin]
#
# stage: a stage number to boot straight into (default 48, Defection; no profile is loaded, so
# anything that saves the profile shows a pak error), or "title" for the normal title -> agent
# select flow with profile 0 auto-selected. eeprom.bin: a save file to start from.
#
# Steps file, one per line:
#   wait <seconds>        key <xdotool key name> [hold-seconds]     shot <name>
#   waitlog <regex> [timeout]   block until game.log matches; ui-trace.inc logs every menu change
#                               ("uitrace: menu <root> <title>") and stage ("uitrace: stage <n>")
#   env PD_CHECKPOINT_TEST=<saveframe>,<loadframe>   (must come first; read at boot)
#   env PD_TEST_KILL_FRAME=<frame>                   (must come first; read at boot)
# Writes <outdir>/<name>.png per shot, <outdir>/run.mp4, <outdir>/run.gif and <outdir>/game.log.
# The checkout is mounted read-only; test-hook.inc is compiled into a copy only when the checkout
# has port/src/savestate.c.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$1" && pwd)
ROM=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
STEPS=$(cd "$(dirname "$3")" && pwd)/$(basename "$3")
mkdir -p "$4"
OUT=$(cd "$4" && pwd)
STAGE=${5:-48}
case "$STAGE" in title) ;; *[!0-9]*) echo "stage must be a number or title" >&2; exit 2;; esac
EEMOUNT=""
if [ -n "${6:-}" ]; then
	EEMOUNT="-v $(cd "$(dirname "$6")" && pwd)/$(basename "$6"):/eeprom.bin:ro"
fi

docker build -q -t pd-linux-test -f "$HERE/Dockerfile.linux" "$HERE" >/dev/null
ENVS=$(grep '^env ' "$STEPS" | sed 's/^env /-e /' | tr '\n' ' ')
# shellcheck disable=SC2086
docker run --rm --network none -v "$SRC":/src:ro -v "$HERE":/tools:ro -v "$ROM":/rom.z64:ro \
	-v "$STEPS":/steps.txt:ro -v "$OUT":/out $EEMOUNT $ENVS -e STAGE="$STAGE" pd-linux-test sh -c '
set -e
cp -r /src /w && rm -rf /w/build
if [ -f /w/port/src/savestate.c ]; then
	mkdir -p /w/tools/savestate-test && cp /tools/test-hook.inc /w/tools/savestate-test/
	perl -0pi -e "s/(void savestateTick\\(void\\)\\n\\{\\n)/\$1#include \"..\\/..\\/tools\\/savestate-test\\/test-hook.inc\"\\n/" /w/port/src/savestate.c
	sed -i "1i #include <stdio.h>" /w/port/src/savestate.c
fi
mkdir -p /w/tools/savestate-test && cp /tools/ui-trace.inc /w/tools/savestate-test/
perl -0pi -e "s/(void mainTick\\(void\\)\\n\\{.*?)(\\t\\tframetimeCalculate\\(\\);)/\$1#include \"..\\/..\\/tools\\/savestate-test\\/ui-trace.inc\"\\n\$2/s" /w/port/src/pdmain.c
grep -q ui-trace.inc /w/port/src/pdmain.c || { echo "ui-trace not inserted"; exit 1; }
sed -i "1i #include <stdio.h>\n#include <stdlib.h>" /w/port/src/pdmain.c
cmake -S /w -B /b -G Ninja >/dev/null
cmake --build /b -j"$(nproc)" >/b/build.log 2>&1 || { grep -E "error" /b/build.log | head; exit 1; }
BIN=$(ls /b/pd.* | head -1)
mkdir -p /r/data && cp /rom.z64 /r/data/pd.ntsc-final.z64
[ -f /eeprom.bin ] && cp /eeprom.bin /r/eeprom.bin
BOOT="--boot-stage $STAGE"
[ "$STAGE" = title ] && BOOT=""
printf "[Video]\nDefaultFullscreen=0\nDefaultWidth=960\nDefaultHeight=720\n[Game]\nSkipIntro=1\n" > /r/pd.ini
Xvfb :7 -screen 0 960x720x24 >/dev/null 2>&1 &
export DISPLAY=:7
sleep 2
cd /r
SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 stdbuf -oL -eL "$BIN" --basedir /r/data --savedir /r $BOOT --profile 0 --log > /out/game.log 2>&1 &
GAME=$!
ffmpeg -loglevel error -y -f x11grab -framerate 15 -video_size 960x720 -i :7 -c:v libx264 -preset veryfast -pix_fmt yuv420p /out/run.mp4 &
REC=$!
win() { xdotool search --sync --onlyvisible --class pd 2>/dev/null | head -1 || xdotool search --onlyvisible --name "Perfect Dark" | head -1; }
sleep 4
while read -r verb a b; do
	case "$verb" in
	wait) sleep "$a" ;;
	waitlog)
		# waitlog <grep -E pattern without spaces> [timeout seconds]: block until the game logs it
		t=0
		until grep -qE "$a" /out/game.log 2>/dev/null; do
			sleep 1; t=$((t + 1))
			[ "$t" -ge "${b:-240}" ] && { echo "waitlog timed out: $a"; break; }
		done
		;;
	key)
		W=$(win); [ -n "$W" ] && xdotool windowactivate --sync "$W" 2>/dev/null || true
		xdotool keydown "$a"; sleep "${b:-0.15}"; xdotool keyup "$a"
		;;
	shot) import -window root "/out/$a.png" ;;
	esac
done < /steps.txt
kill -INT $REC 2>/dev/null || true; wait $REC 2>/dev/null || true
kill $GAME 2>/dev/null || true
ffmpeg -loglevel error -y -i /out/run.mp4 -vf "fps=10,scale=640:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse" /out/run.gif
grep -E "savestate|unlock|FATAL|Segmentation" /out/game.log | tail -12 || true
ls /out
'
