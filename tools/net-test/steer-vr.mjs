#!/usr/bin/env node
// steer-vr.mjs — drive the VR player (Quest, nobody wearing it) to the flat host and attack it,
// through the build's `vrsim` test hook (port/vr/vr_input.cpp). Test tooling for SC-6660.
//
//   steer-vr.mjs [--stop-dist 110] [--max-steps 60] [--attack-secs 20]
//
// Position oracle: the VR client's --net-trace (`p0 local` = VR player, `p1 remote` = host).
// Turning = right thumbstick (sx), walking = left thumbstick (lsy), aiming = controller yaw/pitch,
// firing = trigger. Always removes the vrsim file on exit so the real controllers come back.
import { execFileSync } from 'node:child_process';

const args = process.argv.slice(2);
const opt = (k, d) => { const i = args.indexOf(k); return i >= 0 ? Number(args[i + 1]) : d; };
const stopDist = opt('--stop-dist', 110);
const maxSteps = opt('--max-steps', 60);
const attackSecs = opt('--attack-secs', 20);
const SERIAL = process.env.QUEST_SERIAL || '2G0YC5ZF7S04M8';
const PKG = 'com.perfectdark.port.netplay';
const SIM = `/sdcard/Android/data/${PKG}/files/data/vrsim`;

const sh = (cmd, a) => execFileSync(cmd, a, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
const adb = (c) => sh('adb', ['-s', SERIAL, 'shell', c]);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const sim = (o) => {
	const line = Object.entries(o).map(([k, v]) => `${k}=${Number(v).toFixed(2)}`).join(' ');
	adb(`echo '${line}' > ${SIM}; chmod 666 ${SIM}`);
};
const simOff = () => { try { adb(`rm -f ${SIM}`); } catch {} };
process.on('exit', simOff);
process.on('SIGINT', () => process.exit(130));

function readTrace() {
	const pid = adb(`pidof ${PKG}`).trim();
	const out = adb(`logcat -d --pid=${pid} -s PerfectDark | grep NETTRACE | tail -4`);
	const st = {};
	for (const line of out.split('\n')) {
		const m = line.match(/tick=(\d+) p(\d) (local|remote) pos=\((-?\d+),(-?\d+),(-?\d+)\) theta=([\d.]+) verta=(-?[\d.]+) health=([\d.]+) dead=(\d)/);
		if (m) st[m[3]] = { tick: +m[1], x: +m[4], y: +m[5], z: +m[6], theta: +m[7], health: +m[9], dead: +m[10] };
	}
	return st;
}
// PD convention (bmove): look = (-sin t, ., cos t)
const bearing = (from, to) => (Math.atan2(-(to.x - from.x), to.z - from.z) * 180 / Math.PI + 360) % 360;
const wrap = (d) => ((d + 540) % 360) - 180;

let phase = 'approach';
let attackUntil = 0;
for (let step = 0; step < maxSteps; step++) {
	try { sh('quest', ['claim']); } catch {}
	const st = readTrace();
	if (!st.local || !st.remote) { console.log('no trace yet'); await sleep(2100); continue; }
	const vr = st.local, host = st.remote;
	const dist = Math.hypot(host.x - vr.x, host.z - vr.z);
	const turn = wrap(bearing(vr, host) - vr.theta);
	const pitch = Math.atan2(host.y - vr.y, Math.max(dist, 1)) * 180 / Math.PI;
	console.log(`step ${step} ${phase} vr=(${vr.x},${vr.z}) th=${vr.theta} host=(${host.x},${host.z}) dist=${dist.toFixed(0)} turn=${turn.toFixed(0)} hosthealth=${host.health} hostdead=${host.dead}`);
	if (host.dead) { console.log('HOST KILLED'); break; }

	if (phase === 'approach' && dist < stopDist) { phase = 'attack'; attackUntil = Date.now() + attackSecs * 1000; }
	if (phase === 'attack' && Date.now() > attackUntil) { console.log('attack window over'); break; }

	if (phase === 'approach') {
		// turn with the stick while far off-bearing, walk when roughly facing
		// measured: stick 0.9 turns ~175 deg/s; the hook re-reads the file every 15 frames
		if (Math.abs(turn) > 15) {
			sim({ trigger: 0, yaw: 0, pitch: 0, sx: Math.sign(turn) * 0.9, sy: 0, lsx: 0, lsy: 0 });
			await sleep(Math.max(120, Math.abs(turn) / 175 * 1000 - 80));
		}
		sim({ trigger: 0, yaw: 0, pitch: 0, sx: 0, sy: 0, lsx: 0, lsy: 1 });
		await sleep(Math.min(1500, dist * 0.6));
		sim({ trigger: 0, yaw: 0, pitch: 0, sx: 0, sy: 0, lsx: 0, lsy: 0 });
		await sleep(1800);
	} else {
		// aim the controller straight at the host and pull the trigger in pulses
		const yaw = Math.max(-60, Math.min(60, turn));
		for (let i = 0; i < 4; i++) {
			sim({ trigger: 1, yaw, pitch, sx: 0, sy: 0, lsx: 0, lsy: dist > 80 ? 0.4 : 0 });
			await sleep(250);
			sim({ trigger: 0, yaw, pitch, sx: 0, sy: 0, lsx: 0, lsy: 0 });
			await sleep(250);
		}
	}
}
simOff();
