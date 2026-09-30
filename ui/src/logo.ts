/*
 * The corner mark: GNARL's W, drawn the way the website draws its figures -
 * thin additive lines on nothing, so it reads as light rather than as a solid
 * object pasted on the panel. It turns slowly about its vertical axis.
 *
 * Rotation is a function of TIME, not of frames (CLAUDE.md §10): the angle is
 * computed from the clock each frame, so it turns at the same speed at 144 fps
 * and at 20.
 */

const BRAND = '#64e6ff'; // the site's --accent, as the plugin's pinned logo colour

type V3 = [number, number, number];

/** The W's centre line, in -1..1. */
const SPINE: readonly [number, number][] = [
  [-1, 0.72],
  [-0.5, -0.72],
  [0, 0.3],
  [0.5, -0.72],
  [1, 0.72],
];
const HALF_WIDTH = 0.15;
const HALF_DEPTH = 0.16;

/** The stroke's outline: the spine offset both ways with mitred corners. */
function outline(): [number, number][] {
  const n = SPINE.length;
  const normals: [number, number][] = [];
  for (let i = 0; i < n - 1; i += 1) {
    const [x0, y0] = SPINE[i]!;
    const [x1, y1] = SPINE[i + 1]!;
    const len = Math.hypot(x1 - x0, y1 - y0);
    normals.push([-(y1 - y0) / len, (x1 - x0) / len]);
  }
  const offset = (side: 1 | -1): [number, number][] =>
    SPINE.map(([x, y], i) => {
      const a = normals[Math.max(0, i - 1)]!;
      const b = normals[Math.min(n - 2, i)]!;
      let mx = a[0] + b[0];
      let my = a[1] + b[1];
      const ml = Math.hypot(mx, my);
      mx /= ml;
      my /= ml;
      // Miter length: half-width over cos of half the turn.
      const scale = HALF_WIDTH / Math.max(0.35, mx * b[0] + my * b[1]);
      return [x + side * mx * scale, y + side * my * scale];
    });
  return [...offset(1), ...offset(-1).reverse()];
}

const RING = outline();

/** Every edge of the extruded outline: front face, back face, and the ribs. */
function edges(): [V3, V3][] {
  const out: [V3, V3][] = [];
  const m = RING.length;
  for (let i = 0; i < m; i += 1) {
    const [x0, y0] = RING[i]!;
    const [x1, y1] = RING[(i + 1) % m]!;
    out.push([[x0, y0, HALF_DEPTH], [x1, y1, HALF_DEPTH]]);
    out.push([[x0, y0, -HALF_DEPTH], [x1, y1, -HALF_DEPTH]]);
    out.push([[x0, y0, HALF_DEPTH], [x0, y0, -HALF_DEPTH]]);
  }
  return out;
}

const EDGES = edges();

export function mountLogo(canvas: HTMLCanvasElement): (t: number) => void {
  const ctx = canvas.getContext('2d');
  if (!ctx) return () => undefined;
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  return (t: number): void => {
    const w = canvas.clientWidth;
    const h = canvas.clientHeight;
    const density = canvas.width / Math.max(1, w);
    ctx.setTransform(density, 0, 0, density, 0, 0);
    ctx.clearRect(0, 0, w, h);

    // One turn every 9 seconds; a still three-quarter view with reduced motion.
    const yaw = reduced ? 0.55 : t * ((2 * Math.PI) / 9);
    const pitch = -0.22;
    const cy = Math.cos(yaw);
    const sy = Math.sin(yaw);
    const cp = Math.cos(pitch);
    const sp = Math.sin(pitch);
    const scale = Math.min(w, h) * 0.36;
    const camera = 4.2;

    const project = ([x, y, z]: V3): [number, number, number] => {
      const x1 = x * cy + z * sy;
      const z1 = -x * sy + z * cy;
      const y2 = y * cp - z1 * sp;
      const z2 = y * sp + z1 * cp;
      const k = camera / (camera - z2);
      return [w / 2 + x1 * scale * k, h / 2 - y2 * scale * k, z2];
    };

    // Additive, like the site: overlapping strokes add up to brighter light.
    ctx.globalCompositeOperation = 'lighter';
    ctx.lineCap = 'round';
    for (const [a, b] of EDGES) {
      const pa = project(a);
      const pb = project(b);
      // Nearer edges brighter: depth read from light alone, no shading.
      const near = ((pa[2] + pb[2]) / 2 + 1) / 2;
      ctx.strokeStyle = BRAND;
      ctx.globalAlpha = 0.28 + 0.5 * near;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(pa[0], pa[1]);
      ctx.lineTo(pb[0], pb[1]);
      ctx.stroke();
    }
    ctx.globalAlpha = 1;
    ctx.globalCompositeOperation = 'source-over';
  };
}

/** The small flat W beside the wordmark in the header. */
export function headerMark(): SVGSVGElement {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', '-1.2 -1 2.4 2');
  svg.setAttribute('class', 'brand__mark');
  const path = document.createElementNS('http://www.w3.org/2000/svg', 'polyline');
  path.setAttribute('points', SPINE.map(([x, y]) => `${x},${-y}`).join(' '));
  svg.append(path);
  return svg;
}
