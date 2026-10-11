#!/bin/bash
# box-peer.sh — run a flat-screen port-net build as a net play test peer on a Mac,
# inside its logged-in GUI session (needed for a window + GL when started over ssh).
#
#   box-peer.sh start <name> [game args...]   e.g. start host --host --log
#   box-peer.sh stop  <name>
#   box-peer.sh log   <name> [lines]
#
# Expects ~/Development/pd-netpeer/{pd.arm64,data/pd.ntsc-final.z64}. Runs as the
# transient launchd job com.jrejaud.pdnet-<name>; the game's output goes to
# ~/Development/pd-netpeer/<name>.log.
set -euo pipefail
dir=~/Development/pd-netpeer
cmd="${1:-}"; name="${2:-}"
[ -n "$cmd" ] && [ -n "$name" ] || { echo "usage: box-peer.sh start|stop|log <name> [args...]" >&2; exit 2; }
shift 2
plist="$dir/com.jrejaud.pdnet-$name.plist"

case "$cmd" in
start)
	{
		printf '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n'
		printf '<plist version="1.0"><dict><key>Label</key><string>com.jrejaud.pdnet-%s</string>' "$name"
		printf '<key>ProgramArguments</key><array><string>%s/pd.arm64</string>' "$dir"
		for a in "$@"; do printf '<string>%s</string>' "$a"; done
		printf '</array><key>WorkingDirectory</key><string>%s</string>' "$dir"
		printf '<key>StandardOutPath</key><string>%s/%s.log</string>' "$dir" "$name"
		printf '<key>StandardErrorPath</key><string>%s/%s.log</string>' "$dir" "$name"
		printf '<key>LimitLoadToSessionType</key><string>Aqua</string><key>RunAtLoad</key><true/>'
		printf '<key>ProcessType</key><string>Interactive</string></dict></plist>\n'
	} > "$plist"
	launchctl bootout "gui/$(id -u)/com.jrejaud.pdnet-$name" 2>/dev/null || true
	: > "$dir/$name.log"
	launchctl bootstrap "gui/$(id -u)" "$plist"
	echo "started com.jrejaud.pdnet-$name"
	;;
stop)
	launchctl bootout "gui/$(id -u)/com.jrejaud.pdnet-$name" 2>/dev/null && echo "stopped" || echo "not running"
	;;
log)
	tail -n "${1:-40}" "$dir/$name.log"
	;;
*)
	echo "unknown command $cmd" >&2; exit 2
	;;
esac
