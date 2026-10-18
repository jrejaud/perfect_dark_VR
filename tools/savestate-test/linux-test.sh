#!/bin/sh
# Builds the port with GCC on Linux (Docker) and checks the checkpoint layout that
# port/src/savestate.c relies on: the game's globals must sit between the link-order
# markers (port/savestate/begin.c, end.c) and the port's own state outside them.
# Usage: tools/savestate-test/linux-test.sh     (exit 0 = PASS)
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
docker build -q -t pd-linux-test -f "$ROOT/tools/savestate-test/Dockerfile.linux" "$ROOT/tools/savestate-test" >/dev/null
docker run --rm -v "$ROOT":/src:ro pd-linux-test sh -c '
set -e
cmake -S /src -B /b -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build /b -j"$(nproc)" >/b/build.log 2>&1 || { grep -E "error|FAILED" /b/build.log; exit 1; }
BIN=$(ls /b/pd.* | head -1)
nm "$BIN" | awk "
	{ addr[\$3] = \$1 }  # fixed-width hex, so string order is address order
	function inside(s, b, e) { return (s in addr) && addr[s] > addr[b] && addr[s] < addr[e] }
	function check(ok, what) { print (ok ? \"ok   \" : \"FAIL \") what; if (!ok) bad = 1 }
	END {
		check(inside(\"g_ObjectiveLastIndex\", \"g_SavestateDataBegin\", \"g_SavestateDataEnd\"), \"game .data inside the markers\")
		check(inside(\"g_Vars\", \"g_SavestateBssBegin\", \"g_SavestateBssEnd\"), \"game .bss (g_Vars) inside the markers\")
		check(inside(\"g_Rooms\", \"g_SavestateBssBegin\", \"g_SavestateBssEnd\"), \"game .bss (g_Rooms) inside the markers\")
		check(!inside(\"g_MempHeap\", \"g_SavestateBssBegin\", \"g_SavestateBssEnd\"), \"port state (g_MempHeap) outside\")
		check(!inside(\"settings\", \"g_SavestateBssBegin\", \"g_SavestateBssEnd\"), \"port state (config settings) outside\")
		check(!inside(\"g_Sched\", \"g_SavestateBssBegin\", \"g_SavestateBssEnd\"), \"port state (g_Sched) outside\")
		print (bad ? \"linux-test: FAIL\" : \"linux-test: PASS\")
		exit bad
	}"
'
