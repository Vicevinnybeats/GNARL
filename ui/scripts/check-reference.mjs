/**
 * Checks the TypeScript mirrors against reference values dumped from C++.
 *
 * WHAT THIS IS FOR. Two modules deliberately duplicate engine logic, because
 * the UI has to show what the engine does and the engine's copy is on the
 * audio thread in C++:
 *
 *   - bridge/formatters.ts mirrors the formatters in ParameterRanges.h
 *   - bridge/warp.ts is a port of WarpProcessor.h
 *
 * Duplication is only a liability if it can drift unnoticed. warp.ts claimed
 * in its own header comment to be "checked against reference values dumped
 * from the C++ implementation" - and there was no dumper, no reference file
 * and no test. This is the check that makes the claim true.
 *
 * NO TEST FRAMEWORK, on purpose. The project has no JS test runner and adding
 * one to run two comparisons would be a dependency for its own sake. The
 * modules being checked are import-free by design, so tsc can compile them
 * standalone and node can import the result.
 *
 * Regenerate the reference after ANY change to the C++ side:
 *   cmake --build build --target GnarlDumpReference
 *   ./build/tests/GnarlDumpReference ui/tests/referenceVectors.json
 */
import { execFileSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const uiRoot = join(here, '..');
const repoRoot = join(uiRoot, '..');

const referencePath = join(uiRoot, 'tests', 'referenceVectors.json');
const defaultsPath = join(uiRoot, 'src', 'bridge', 'parameterDefaults.json');

const sources = [
  join(uiRoot, 'src', 'bridge', 'formatters.ts'),
  join(uiRoot, 'src', 'bridge', 'warp.ts'),
];

// --- Compile the mirrors ----------------------------------------------------

const outDir = mkdtempSync(join(tmpdir(), 'gnarl-reference-'));

try {
  execFileSync(
    join(uiRoot, 'node_modules', '.bin', 'tsc'),
    [
      ...sources,
      '--outDir', outDir,
      '--target', 'es2022',
      '--module', 'es2022',
      '--moduleResolution', 'bundler',
      '--strict',
      '--skipLibCheck',
    ],
    { stdio: 'inherit' },
  );
} catch {
  console.error('check-reference: the mirrors did not compile.');
  process.exit(1);
}

const formatters = await import(join(outDir, 'formatters.js'));
const warp = await import(join(outDir, 'warp.js'));

const reference = JSON.parse(readFileSync(referencePath, 'utf8'));
const defaults = JSON.parse(readFileSync(defaultsPath, 'utf8'));

let failures = 0;
let checkedFormat = 0;
let skipped = 0;
const skippedIds = [];

const fail = (message) => {
  if (failures < 20) console.error('  ' + message);
  failures += 1;
};

// --- Formatters -------------------------------------------------------------

for (const [id, samples] of Object.entries(reference.formatting)) {
  const meta = defaults[id];

  if (!meta) {
    fail(`${id}: in the reference but not in parameterDefaults.json - one of the two is stale`);
    continue;
  }

  const kind = formatters.pickFormatter({
    textAtMin: meta.textAtMin ?? '',
    textAtMax: meta.textAtMax ?? '',
    textAtDefault: meta.textAtDefault ?? '',
    min: meta.min ?? 0,
    max: meta.max ?? 1,
  });

  if (kind === null) {
    // A choice, a boolean or a counted integer. Those are formatted from the
    // choice list, which ParameterMirrorTests already guards.
    skipped += 1;
    skippedIds.push(id);
    continue;
  }

  for (const sample of samples) {
    const value = formatters.snapToInterval(
      formatters.denormalise(sample.n, meta.min ?? 0, meta.max ?? 1, meta.skew ?? 1),
      meta.interval ?? 0,
    );

    const ours = formatters.format(kind, value);

    checkedFormat += 1;

    if (ours !== sample.text) {
      fail(
        `${id} @ ${sample.n.toFixed(2)} (${kind}): C++ "${sample.text}" vs TS "${ours}"`,
      );
    }
  }
}

// --- Warps ------------------------------------------------------------------

let checkedWarp = 0;

// A phase is a fraction of a cycle; a tenth of a cent of one is far below
// anything a display can show, and float-vs-double arithmetic will differ by
// a few ULPs whatever we do.
const PHASE_TOLERANCE = 1e-5;

/*  CIRCULAR, because a phase is a position on a cycle and 0.9999999 and 0 are
    a ten-millionth apart rather than almost a whole cycle. At phase exactly
    1.0 the C++ wraps to zero and the port does not, and comparing those
    linearly reported a difference of 1.0 for two values that name the same
    point - which is a bug in the comparison, not in either implementation.
    Both are read as "evaluate the wavetable here", and both read the same
    sample. */
const phaseDistance = (a, b) => {
  const difference = Math.abs(a - b) % 1;
  return Math.min(difference, 1 - difference);
};

for (const entry of reference.warp) {
  for (const point of entry.points) {
    const ours = warp.applyWarp(point.phase, entry.mode, entry.amount);

    checkedWarp += 1;

    if (!Number.isFinite(ours) || phaseDistance(ours, point.out) > PHASE_TOLERANCE) {
      fail(
        `warp mode ${entry.mode} amount ${entry.amount} phase ${point.phase.toFixed(3)}: ` +
          `C++ ${point.out} vs TS ${ours}`,
      );
    }
  }

  const bandwidth = warp.warpBandwidthExpansion(entry.mode, entry.amount);

  if (Math.abs(bandwidth - entry.bandwidth) > 1e-5) {
    fail(
      `warp bandwidth mode ${entry.mode} amount ${entry.amount}: ` +
        `C++ ${entry.bandwidth} vs TS ${bandwidth}`,
    );
  }
}

rmSync(outDir, { recursive: true, force: true });

// --- Result -----------------------------------------------------------------

console.log(
  `check-reference: ${checkedFormat} formatter points, ${checkedWarp} warp points, ` +
    `${skipped} parameters formatted from choice lists instead.`,
);

if (failures > 0) {
  if (failures > 20) console.error(`  ...and ${failures - 20} more.`);
  console.error(`check-reference: ${failures} mismatches. The mirrors have drifted.`);
  process.exit(1);
}

console.log('check-reference: the TypeScript mirrors match C++.');
