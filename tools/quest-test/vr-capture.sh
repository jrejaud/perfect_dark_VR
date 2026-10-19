#!/bin/sh
# Before/after UI capture for Perfect Dark VR on an off-head Quest (needs the vrsim hooks:
# lpose, kill, menu trace). Installs the given APK, plays one flow, and saves screenshots and a
# left-eye video of it.
#   tools/quest-test/vr-capture.sh <serial> <apk> <flow> <outdir>
#   flow: pause       boot into Defection, take a checkpoint, open the pause menu, move the cursor
#         death-load  title -> Solo Missions -> mission, take a checkpoint, die, Load Last Checkpoint
#         death-quit  same, but choose Quit
#         options     title -> Perfect Menu -> Options -> Extended -> Game
# The fork's save (eeprom.bin) is backed up before and restored after, so test deaths don't
# change the player's profile.
set -e
S=$1; APK=$2; FLOW=$3; OUT=$4
HERE=$(cd "$(dirname "$0")" && pwd)
VR_UI="$HERE/vr-ui.sh"
# every step is echoed, so a run that stops on a timed-out wait shows where
V=step
step() { echo ">> $2 $3 $4" >&2; "$VR_UI" "$@"; }
P=com.perfectdark.port.netplay
D=/sdcard/Android/data/$P/files
mkdir -p "$OUT"
adb -s "$S" shell am force-stop $P
adb -s "$S" install -r "$APK" | tail -1
adb -s "$S" shell "cp $D/eeprom.bin $D/eeprom.bin.vrcapture-backup"

trap 'adb -s "$S" shell "am force-stop $P; cp $D/eeprom.bin.vrcapture-backup $D/eeprom.bin; rm -f $D/eeprom.bin.vrcapture-backup"; "$VR_UI" "$S" rec-stop 2>/dev/null; "$VR_UI" "$S" off' EXIT

# the title screen waits for Start; press it until the profile auto-selects into the Perfect Menu
to_perfect_menu() {
	sleep 30
	for i in 1 2 3 4 5 6 7 8 9 10; do
		adb -s "$S" shell logcat -d | grep -q "uitrace: menu .*Perfect Menu" && return 0
		$V "$S" press menu 1; sleep 6
	done
	echo "never reached the Perfect Menu" >&2; return 1
}

to_mission() {
	$V "$S" boot title
	to_perfect_menu
	sleep 2; $V "$S" press a
	$V "$S" waitlog "uitrace: menu .*Mission Select" 30
	sleep 2; $V "$S" press a
	$V "$S" waitlog "uitrace: menu .*Select Difficulty" 30
	sleep 2; $V "$S" press a
	$V "$S" waitlog "uitrace: menu .*Overview" 30
	sleep 2; adb -s "$S" shell logcat -c; $V "$S" press a
	$V "$S" waitlog "uitrace: stage " 90
	sleep 5; $V "$S" press a; sleep 2; $V "$S" press trigger; sleep 6
}

case "$FLOW" in
pause)
	$V "$S" boot 48
	$V "$S" waitlog "uitrace: stage 48" 120
	sleep 25; $V "$S" press a; sleep 2; $V "$S" press trigger; sleep 8
	$V "$S" press qsave; sleep 3
	$V "$S" rec-start "$OUT/run.mkv" || echo "recorder failed, continuing without video" >&2; sleep 3
	$V "$S" press menu 1
	$V "$S" waitlog "uitrace: menu .*Status" 30
	sleep 3; $V "$S" shot "$OUT/1-pause.png"
	$V "$S" press lsy=-1 1.2; sleep 2; $V "$S" shot "$OUT/2-pause-down.png"
	$V "$S" press lsy=-1 1.2; sleep 2; $V "$S" shot "$OUT/3-pause-down2.png"
	sleep 2; $V "$S" rec-stop
	;;
death-load|death-quit)
	to_mission
	$V "$S" press qsave; sleep 3
	$V "$S" rec-start "$OUT/run.mkv" || echo "recorder failed, continuing without video" >&2; sleep 3
	$V "$S" press kill
	# after a death the game may wait for a button before the endscreen; nudge it until a menu opens
	for i in 1 2 3 4 5 6 7 8; do
		adb -s "$S" shell logcat -d | grep -q "uitrace: menu 1 " && break
		sleep 20; adb -s "$S" shell logcat -d | grep -q "uitrace: menu 1 " && break
		$V "$S" press a
	done
	$V "$S" waitlog "uitrace: menu 1 " 30
	sleep 4; $V "$S" shot "$OUT/1-after-death.png"
	if [ "$FLOW" = death-quit ]; then
		$V "$S" press lsy=-1 1.2; sleep 2; $V "$S" shot "$OUT/2-quit-focused.png"
	fi
	$V "$S" press a; sleep 6; $V "$S" shot "$OUT/3-after-choice.png"
	sleep 4; $V "$S" shot "$OUT/4-later.png"
	$V "$S" rec-stop
	;;
options)
	$V "$S" boot title
	to_perfect_menu
	$V "$S" rec-start "$OUT/run.mkv" || echo "recorder failed, continuing without video" >&2; sleep 3
	$V "$S" press lsx=-1 0.6
	$V "$S" waitlog "uitrace: menu .*Options" 20
	sleep 2; $V "$S" shot "$OUT/1-options.png"
	;;
*)
	echo "unknown flow $FLOW" >&2; exit 2
	;;
esac
adb -s "$S" shell logcat -d | grep -E "uitrace|savestate" | sed 's/.*PerfectDark: //' > "$OUT/trace.log" || true
ls "$OUT"
