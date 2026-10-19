#!/bin/sh
# Agent UI capture for Perfect Dark VR on an off-head Quest, through the vrsim test hook.
# Every vrsim line also carries lpose=1, which holds the left controller (and so the menu panel
# the game attaches to it) in front of the face, so menus are in the headset's view.
#   vr-ui.sh <serial> boot <stage|title> [game args]   force-stop and launch (title = normal boot, profile 0)
#   vr-ui.sh <serial> press <button> [hold-seconds]    a b x y menu lgrip rgrip trigger ltrig, or lsy=-1 etc
#   vr-ui.sh <serial> waitlog <regex> [timeout]        wait until logcat shows it
#   vr-ui.sh <serial> shot <out.png>                   full-res quest screenshot
#   vr-ui.sh <serial> rec-start <out.mkv> | rec-stop   left-eye video via quest-vr-record
#   vr-ui.sh <serial> idle                             keep only lpose=1 in the sim file
#   vr-ui.sh <serial> off                              remove the sim file (real controllers back)
set -e
S=$1; CMD=$2; shift 2
P=com.perfectdark.port.netplay
SIM=/sdcard/Android/data/$P/files/data/vrsim
put() { adb -s "$S" shell "echo 'lpose=1 $1' > $SIM; chmod 666 $SIM"; }

case "$CMD" in
boot)
	STAGE=$1; shift
	ARGS="--profile 0 $*"
	[ "$STAGE" = title ] || ARGS="--boot-stage $STAGE $ARGS"
	# a launch right after a force-stop sometimes stalls before VR starts; retry until the
	# game logs that it initialised
	for try in 1 2 3; do
		adb -s "$S" shell "am force-stop $P; logcat -c"
		put ""
		adb -s "$S" shell "am start -n $P/com.perfectdark.port.MainActivity --ez autostart true --es net_mode off --es game_args '$ARGS'" >/dev/null
		t=0
		while [ "$t" -lt 45 ]; do
			adb -s "$S" shell logcat -d | grep -q "gameInit complete" && exit 0
			sleep 3; t=$((t + 3))
		done
		echo "boot: no gameInit after 45 s (try $try), relaunching" >&2
		sleep 3
	done
	echo "boot: game never initialised" >&2; exit 1
	;;
press)
	case "$1" in *=*) KV=$1 ;; *) KV="$1=1" ;; esac
	put "$KV"
	sleep "${2:-0.5}"
	put ""
	;;
waitlog)
	t=0
	until adb -s "$S" shell logcat -d | grep -qE "$1"; do
		sleep 2; t=$((t + 2))
		[ "$t" -ge "${2:-180}" ] && { echo "waitlog timed out: $1"; exit 1; }
	done
	;;
shot)
	sleep 1
	quest -s "$S" screenshot "$1" >/dev/null
	echo "$1"
	;;
rec-start)
	~/.claude/skills/quest/scripts/quest-vr-record start "$1" left >/dev/null
	;;
rec-stop)
	~/.claude/skills/quest/scripts/quest-vr-record stop >/dev/null
	;;
idle)
	put ""
	;;
off)
	adb -s "$S" shell "rm -f $SIM"
	;;
*)
	echo "usage: vr-ui.sh <serial> boot|press|waitlog|shot|rec-start|rec-stop|idle|off ..." >&2; exit 2
	;;
esac
