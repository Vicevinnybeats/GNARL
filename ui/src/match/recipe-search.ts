/*
 * MATCH A SOUND's search, inside the riddim recipe (docs/design/phase4-05-
 * riddim-recipe.md, phase4-04-match-in-app.md). The first matcher searched
 * its own genes - one wob, an LFO that ran once - and every match was a
 * static tone after one sweep: the producer's "screech instead of a wob".
 * This one searches only sounds the AI makes (generate.ts: Vinny Bass 2's
 * four LFO routes kept), so every candidate wobs, and the log-mel measure
 * chooses among wobs.
 *
 * A random round stratified by rhythm (each of 1/8, 1/8T, 1/4, 1/16, 1/4T
 * an equal share), then generations of children of the best, the steps
 * shrinking as they go. Pure: `score` renders and judges a batch of patch
 * texts (the page's workers, or tools/match_web.mjs on one thread).
 */

import { evolvePatch, generatePatch, RHYTHMS } from '../generate.ts';

export interface RecipeCandidate {
  d: number;
  peak: number;
  patch: string;
  rhythm: string;
}

export interface RecipeSearch {
  bases: Readonly<Record<string, string>>;
  randomCount: number;
  generations: number;
  children: number;
  seed: number;
  score(patches: string[]): Promise<{ d: number; peak: number }[]>;
  onProgress?(done: number, total: number, best: number): void;
  cancelled?(): boolean;
}

/** LFO 1's rhythm, as RHYTHMS names it ('' for anything else). */
export function rhythmOf(patch: string): string {
  const s = (JSON.parse(patch) as { settings: Record<string, number> }).settings;
  return RHYTHMS.find((x) => x.sync === s.lfo_1_sync && x.tempo === s.lfo_1_tempo)?.label ?? '';
}

/** Every candidate scored, best first. */
export async function searchRecipe(job: RecipeSearch): Promise<RecipeCandidate[]> {
  const total = job.randomCount + job.generations * job.children;
  const everything: RecipeCandidate[] = [];
  let done = 0;
  const run = async (patches: string[]): Promise<void> => {
    const results = await job.score(patches);
    patches.forEach((patch, i) => {
      const r = results[i] ?? { d: Infinity, peak: 0 };
      everything.push({ d: r.d, peak: r.peak, patch, rhythm: rhythmOf(patch) });
    });
    done += patches.length;
    job.onProgress?.(done, total, Math.min(...everything.map((e) => e.d)));
  };

  const labels = RHYTHMS.map((x) => x.label);
  await run(Array.from({ length: job.randomCount }, (_, i) =>
    generatePatch(job.bases, job.seed * 100_003 + i, labels[i % labels.length]).patch));

  for (let gen = 0; gen < job.generations; gen += 1) {
    if (job.cancelled?.()) break;
    // The best two of each rhythm among the best four rhythms: the search
    // keeps more than one rhythm alive until the end.
    const byRhythm = new Map<string, RecipeCandidate[]>();
    for (const e of [...everything].sort((a, b) => a.d - b.d)) {
      const list = byRhythm.get(e.rhythm) ?? [];
      if (list.length < 2) byRhythm.set(e.rhythm, [...list, e]);
    }
    const parents = [...byRhythm.values()].sort((a, b) => (a[0]?.d ?? Infinity) - (b[0]?.d ?? Infinity))
      .slice(0, 4).flat();
    const strength = 0.7 * (1 - gen / Math.max(1, job.generations)) + 0.2;
    const kids = Array.from({ length: job.children }, (_, k) => {
      const parent = parents[k % parents.length];
      return parent ? evolvePatch(parent.patch, job.seed * 7919 + gen * 101 + k, strength, 'matched').patch : '';
    }).filter((p) => p.length > 0);
    await run(kids);
  }
  return everything.filter((e) => Number.isFinite(e.d)).sort((a, b) => a.d - b.d);
}
