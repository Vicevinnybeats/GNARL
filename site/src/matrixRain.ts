/**
 * Glyph rain, behind everything.
 *
 * WHY 2D CANVAS AND NOT MORE PARTICLES. The universe already runs ninety
 * thousand additive points through a bloom pass; adding forty thousand
 * textured glyph quads to that is another texture fetch per fragment on the
 * heaviest pass in the frame, for an effect that is read as TEXTURE rather
 * than as depth. A 2D canvas at a third of the device resolution, drawn at
 * 24 fps, costs a rounding error by comparison and looks the same through
 * the blur it sits under.
 *
 * IT IS DRAWN AT 24 fps DELIBERATELY, not as a concession. Glyph rain that
 * updates every frame at 120 Hz is a grey shimmer - the eye cannot resolve
 * individual characters, so it stops reading as text at all. Stepping it
 * slowly is what makes it legible as falling code, which is the entire
 * effect.
 */

const GLYPHS = 'アカサタナハマヤラワ0123456789ABCDEFGNARLΣΔΞΨ<>/\\|=+*·:';

/*  Device pixels per glyph cell — and it is drawn at a third of device
    resolution, so this is roughly a 40px glyph on screen. Larger than that
    and the rain stops being atmosphere and starts competing with the
    headline: measured in a screenshot at 22, individual characters were
    bigger than the body copy and read as content. */
const CELL = 13;

/** How often a column redraws. See the note above: slow on purpose. */
const STEP_MS = 1000 / 24;

export interface MatrixRain {
  resize(): void;
  render(now: number): void;
  dispose(): void;
}

export function createMatrixRain(canvas: HTMLCanvasElement): MatrixRain {
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const context = canvas.getContext('2d');

  let columns = 0;
  let drops: Float32Array = new Float32Array(0);
  let speeds: Float32Array = new Float32Array(0);
  let last = 0;

  /*  A third of device resolution. The rain lives under a blur and a scrim;
      rendering it sharp would be paying full price for detail that is
      deliberately thrown away one layer up. */
  const SCALE = 0.34;

  function resize(): void {
    const width = Math.max(1, Math.floor(window.innerWidth * SCALE));
    const height = Math.max(1, Math.floor(window.innerHeight * SCALE));

    canvas.width = width;
    canvas.height = height;

    columns = Math.max(1, Math.ceil(width / CELL));

    const next = new Float32Array(columns);
    const nextSpeeds = new Float32Array(columns);

    for (let i = 0; i < columns; i += 1) {
      // Seeded above the top edge, spread out, so the first frame is a
      // field already in motion rather than a line sliding in together.
      next[i] = drops[i] ?? -Math.random() * (height / CELL);
      nextSpeeds[i] = speeds[i] ?? 0.45 + Math.random() * 0.9;
    }

    drops = next;
    speeds = nextSpeeds;

    if (context) {
      context.font = `${CELL - 4}px ui-monospace, monospace`;
      context.textBaseline = 'top';
    }
  }

  resize();

  return {
    resize,

    render(now: number) {
      if (!context || reduced) return;
      if (now - last < STEP_MS) return;

      last = now;

      const { width, height } = canvas;
      const bottom = height / CELL;

      /*  Painted over rather than cleared, which is what leaves each column
          a fading tail instead of a single moving character. The alpha is
          what sets the tail's length: lower fades slower and trails more. */
      context.globalCompositeOperation = 'source-over';
      context.fillStyle = 'rgba(4, 3, 13, 0.22)';
      context.fillRect(0, 0, width, height);

      context.globalCompositeOperation = 'lighter';

      for (let i = 0; i < columns; i += 1) {
        const y = drops[i]!;

        if (y >= 0) {
          const glyph = GLYPHS[Math.floor(Math.random() * GLYPHS.length)] ?? '0';

          // The leading character is bright; everything behind it is the
          // fade left by the wash above.
          context.fillStyle = 'rgba(120, 232, 255, 0.42)';
          context.fillText(glyph, i * CELL, y * CELL);
        }

        drops[i] = y + speeds[i]!;

        // Restart well above the top, at a new speed, so the columns never
        // fall back into step with each other.
        if (drops[i]! > bottom + Math.random() * 20) {
          drops[i] = -Math.random() * 12;
          speeds[i] = 0.45 + Math.random() * 0.9;
        }
      }
    },

    dispose() {
      drops = new Float32Array(0);
      speeds = new Float32Array(0);
    },
  };
}
