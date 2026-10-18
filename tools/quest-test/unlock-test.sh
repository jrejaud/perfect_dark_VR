#!/bin/sh
# Installs the netplay debug build on the Quest, restores a save, drops an unlock-missions
# file, boots with profile 0, presses Start through the title via the vrsim hook, and prints
# the unlock log lines. Run twice: the second run must report the best times as already there.
#   tools/quest-test/unlock-test.sh <serial> <eeprom.bin|keep> <mission number> [--no-install]
set -e
S=$1; EE=$2; N=$3; FLAG=${4:-}
P=com.perfectdark.port.netplay
D=/sdcard/Android/data/$P/files
SIM=$D/data/vrsim
APK=$(cd "$(dirname "$0")/../.." && pwd)/android/app/build/outputs/apk/debug/pd-vr-ntsc-debug.apk
OUT=${TMPDIR:-/tmp}/pd-unlock-test.log

adb -s "$S" shell am force-stop $P
[ "$FLAG" = "--no-install" ] || adb -s "$S" install -r "$APK" | tail -1
[ "$EE" = "keep" ] || adb -s "$S" push "$EE" $D/eeprom.bin >/dev/null
adb -s "$S" shell "chmod 666 $D/eeprom.bin; echo $N > $D/data/unlock-missions; chmod 666 $D/data/unlock-missions; logcat -c"
adb -s "$S" shell "am start -n $P/com.perfectdark.port.MainActivity --ez autostart true --es net_mode off --es game_args '--profile 0'" >/dev/null
sleep 35
for i in 1 2 3 4 5 6 7 8; do
	adb -s "$S" shell "echo 'menu=1' > $SIM; chmod 666 $SIM"
	sleep 1.2
	adb -s "$S" shell "echo 'menu=0' > $SIM"
	sleep 6
	adb -s "$S" shell "logcat -d" > "$OUT"
	grep -qE "unlock-missions: (missions|saving)" "$OUT" && break
done
adb -s "$S" shell "rm -f $SIM"
grep -E "unlock-missions" "$OUT" | sed 's/.*PerfectDark: //'
adb -s "$S" shell "ls $D/data | grep unlock; md5sum $D/eeprom.bin"
