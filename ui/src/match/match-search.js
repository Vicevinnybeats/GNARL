// The sound matcher's search (tools/match.py search), plain JavaScript like
// match-core.js: the page and the Node tests load it with new Function.
// `score` renders and judges a batch of genes, in Web Workers in the page.

/* exported createMatcherSearch */
function createMatcherSearch(core) {
  /**
   * A random round with every table getting an equal share, then
   * generations of mutation from the best two of each of the six best
   * tables. Resolves with every scored candidate, best first.
   */
  async function search({ randomCount, generations, children, seed, score, onProgress, cancelled }) {
    const r = core.random(seed);
    const total = randomCount + generations * children;
    let done = 0;
    const everything = [];
    const run = async (genes) => {
      const results = await score(genes);
      genes.forEach((g, i) => everything.push({ d: results[i].d, peak: results[i].peak, genes: g }));
      done += genes.length;
      if (onProgress) onProgress(done, total, Math.min(...everything.map((e) => e.d)));
    };

    const population = [];
    for (let i = 0; i < randomCount; i += 1) {
      const g = core.randomGenes(r);
      g.table = i % core.CHOICES.table.length;
      population.push(g);
    }
    await run(population);

    const parents = () => {
      const best = new Map();
      for (const e of [...everything].sort((a, b) => a.d - b.d)) {
        const list = best.get(e.genes.table) ?? [];
        if (list.length < 2) best.set(e.genes.table, [...list, e]);
      }
      return [...best.values()].sort((a, b) => a[0].d - b[0].d).slice(0, 6).flat();
    };
    for (let gen = 0; gen < generations; gen += 1) {
      if (cancelled && cancelled()) break;
      const sigma = 0.25 * (1 - gen / Math.max(1, generations)) + 0.03;
      const from = parents();
      const kids = [];
      for (let k = 0; k < children; k += 1) kids.push(core.mutate(from[Math.floor(r() * from.length)].genes, r, sigma));
      await run(kids);
    }
    return everything.sort((a, b) => a.d - b.d);
  }
  return { search };
}
