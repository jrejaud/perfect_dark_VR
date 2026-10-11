#!/usr/bin/env node
// Resolve git conflict hunks in a file by index.
// usage: resolve.mjs <file> <spec>...   spec = <n|all>:<ours|theirs|both|both-rev>
// Hunks are numbered from 1 in file order; specs apply to the ORIGINAL numbering.
import fs from 'node:fs';

const [file, ...specs] = process.argv.slice(2);
if (!file || specs.length === 0) {
	console.error('usage: resolve.mjs <file> <n|all>:<ours|theirs|both|both-rev> ...');
	process.exit(2);
}

const choice = new Map();
let fallback = null;
for (const s of specs) {
	const [n, how] = s.split(':');
	if (!['ours', 'theirs', 'both', 'both-rev'].includes(how)) throw new Error(`bad spec ${s}`);
	if (n === 'all') fallback = how; else choice.set(Number(n), how);
}

const lines = fs.readFileSync(file, 'utf8').split('\n');
const out = [];
let hunk = 0, state = null, ours = [], theirs = [], left = 0;
for (const line of lines) {
	if (state === null && line.startsWith('<<<<<<< ')) { state = 'ours'; hunk++; ours = []; theirs = []; continue; }
	if (state === 'ours' && line.startsWith('||||||| ')) { state = 'base'; continue; }
	if ((state === 'ours' || state === 'base') && line === '=======') { state = 'theirs'; continue; }
	if (state === 'theirs' && line.startsWith('>>>>>>> ')) {
		const how = choice.get(hunk) ?? fallback;
		if (!how) { out.push('<<<<<<< HEAD', ...ours, '=======', ...theirs, line); left++; }
		else if (how === 'ours') out.push(...ours);
		else if (how === 'theirs') out.push(...theirs);
		else if (how === 'both') out.push(...ours, ...theirs);
		else out.push(...theirs, ...ours);
		state = null; continue;
	}
	if (state === 'ours') ours.push(line);
	else if (state === 'theirs') theirs.push(line);
	else if (state === null) out.push(line);
}
fs.writeFileSync(file, out.join('\n'));
console.log(`${file}: ${hunk} hunks, ${left} left unresolved`);
