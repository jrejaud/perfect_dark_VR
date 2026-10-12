#!/usr/bin/env node
// steer-host.mjs — walk the flat-screen host (on Box) up to the VR player, using the
// VR client's --net-trace lines as the position oracle. Test tooling for SC-6660.
//
//   steer-host.mjs <box-window-id> [--stop-dist 500] [--max-steps 40] [--face]
//
// Reads `NETTRACE ... p0 local pos=(x,y,z)` (the Quest player) and `p1 remote ...`
// (the host) from the Quest's logcat, turns the host toward the Quest player with
// left/right holds, walks with W, and sidesteps when it stops making progress.
// --face: only turn to face the Quest player, don't walk.
import { execFileSync } from 'node:child_process';

const args = process.argv.slice(2);
const win = args[0];
if (!win) { console.error('usage: steer-host.mjs <box-window-id> [--stop-dist N] [--max-steps N] [--face]'); process.exit(2); }
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? Number(args[i + 1]) : d; };
const stopDist = opt('--stop-dist', 500);
const maxSteps = opt('--max-steps', 40);
const faceOnly = args.includes('--face');
const SERIAL = process.env.QUEST_SERIAL || execFileSync('adb', ['devices']).toString().split('\n').map((l) => l.split('\t')).find((p) => p[1] === 'device')?.[0];
const TURN_DEG_PER_MS = 0.22; // measured: ~66 deg per 300 ms hold

const sh = (cmd, a) => execFileSync(cmd, a, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
const press = (key, ms) => sh('box-screen', ['peekaboo', 'press', key, '--hold', String(Math.max(30, Math.round(ms))), '--window-id', win, '--foreground']);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function readTrace() {
	const pid = sh('adb', ['-s', SERIAL, 'shell', 'pidof', 'com.perfectdark.port.netplay']).trim();
	const out = sh('adb', ['-s', SERIAL, 'shell', `logcat -d --pid=${pid} -s PerfectDark | grep -E 'NETTRACE.*(p[0-9] |aim )' | tail -6`]);
	const st = {};
	for (const line of out.split('\n')) {
		const m = line.match(/tick=(\d+) p(\d) (local|remote) pos=\((-?\d+),(-?\d+),(-?\d+)\) theta=(-?[\d.]+) verta=(-?[\d.]+) health=([\d.]+) dead=(\d)(?: chrdamage=([\d.-]+)\/([\d.-]+))?/);
		if (m) st[m[3]] = { tick: +m[1], x: +m[4], y: +m[5], z: +m[6], theta: +m[7], health: +m[9], dead: +m[10], dmg: m[11] ?? '?' };
	}
	return st;
}

// PD view convention (bmove): look = (-sin t, ., cos t) for theta t.
const bearing = (from, to) => {
	const dx = to.x - from.x, dz = to.z - from.z;
	let t = Math.atan2(-dx, dz) * 180 / Math.PI;
	return (t + 360) % 360;
};
const wrap = (d) => ((d + 540) % 360) - 180;

let last = null, stuck = 0, side = 'd', best = Infinity, sinceBest = 0;
for (let step = 0; step < maxSteps; step++) {
	await sleep(1100); // trace period is 1 s
	// keep the Quest driving lock (15 min TTL) for the whole run, or another agent takes the
	// headset mid-test and closes the game (2026-10-11)
	try { sh('quest', ['claim']); } catch {}
	const st = readTrace();
	if (!st.local || !st.remote) { console.log('no trace yet'); continue; }
	const host = st.remote, vr = st.local;
	const dist = Math.hypot(vr.x - host.x, vr.z - host.z);
	const want = bearing(host, vr);
	const turn = wrap(want - host.theta);
	console.log(`step ${step} host=(${host.x},${host.z}) th=${host.theta} vr=(${vr.x},${vr.z}) dist=${dist.toFixed(0)} turn=${turn.toFixed(0)} vrhealth=${vr.health} vrdmg=${vr.dmg}`);
	if (Math.abs(turn) > 4) press(turn > 0 ? 'right' : 'left', Math.abs(turn) / TURN_DEG_PER_MS);
	if (faceOnly) { if (Math.abs(turn) <= 4) break; continue; }
	if (dist < stopDist && Math.abs(vr.y - host.y) < 120) { console.log('arrived'); break; }
	if (dist < best - 100) { best = dist; sinceBest = 0; } else if (++sinceBest >= 5) {
		// bouncing between the same walls: wander, then home again
		press(Math.random() < 0.5 ? 'left' : 'right', 300 + Math.random() * 500);
		press('w', 3000);
		sinceBest = 0; best = Infinity; last = null;
		continue;
	}

	if (last && Math.hypot(host.x - last.x, host.z - last.z) < 120) {
		stuck++;
		press('s', 400);
		if (stuck >= 3) {
			// wedged: wander off on a random heading for a bit before homing again
			press(Math.random() < 0.5 ? 'left' : 'right', 250 + Math.random() * 500);
			press('w', 2500);
			stuck = 0;
			last = null;
			continue;
		}
		press(side, 700 + 200 * stuck);
		if (stuck % 2 === 0) side = side === 'd' ? 'a' : 'd';
	} else {
		stuck = 0;
	}
	last = host;
	press('w', Math.min(1600, dist * 0.8));
}
