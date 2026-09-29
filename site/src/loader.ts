/**
 * The boot screen.
 *
 * A HUD dial that fills while the page actually loads, with the wordmark
 * inside it, drawn entirely in three.js - no DOM, no font file, no image.
 * Every glyph is a polyline, which is the same decision the figure in
 * `journey.ts` rests on: line art is legible because the strokes are thin and
 * the gaps are empty, and it costs nothing to fetch because it is arithmetic
 * rather than an asset.
 *
 * Two things here are deliberate and worth keeping:
 *
 * THE NUMBER IS THE REAL LOAD. It is driven by the stages boot.ts reports and
 * by three's own loading manager, and it never reaches 100 before the page is
 * genuinely ready. A progress bar on a timer is a lie that tells the viewer
 * nothing, and the one time it matters - a slow connection - it is confidently
 * wrong.
 *
 * AND IT ENDS ON A CLICK, rather than dismissing itself the instant the last
 * byte lands. The whole page is driven by scroll, so an automatic dismissal
 * drops the viewer into the opening shot mid-gesture if they were already
 * scrolling. A click is also the gesture a browser requires before a page may
 * make any sound, so if this page ever opens with a growl the way the plugin
 * does, the permission is already in hand rather than costing a console error
 * on every load.
 */
import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';

export interface Loader {
  /** Real progress, 0..1. Never decreases, and never reaches 1 on its own. */
  setProgress(value: number): void;
  /** Everything is loaded: swap the prompt and let the viewer in. */
  ready(): void;
  /** Called once, when the viewer clicks through. */
  onEnter(callback: () => void): void;
  /**
   * Called once, when the dismissal has finished DRAWING itself.
   *
   * Not a timer. The fade advances per frame, so a wall-clock wait for it is
   * a guess at the frame rate: at a few frames a second the renderer was
   * being disposed a third of the way through and the canvas kept its last
   * drawn image - a half-faded loader sitting over the page for good.
   */
  onDismissed(callback: () => void): void;
  /**
   * How far through the exit it is, 0 until the click and 1 when the dial has
   * gone. `boot.ts` drives the page's own zoom from this so the two halves
   * are one continuous move rather than two animations that happen to
   * overlap — and it is a frame-driven number for the same reason everything
   * else here is.
   */
  entryProgress(): number;
  render(now: number, delta: number): void;
  resize(): void;
  dispose(): void;
}

/*  A STROKE FONT, as polylines in a 0..0.9 by 0..1 box. Only the letters this
    screen says, because an unused glyph is bytes in every visitor's download.
    Add one when the copy needs it rather than typing the alphabet out now. */
type Stroke = readonly (readonly [number, number])[];

const GLYPHS: Record<string, readonly Stroke[]> = {
  A: [[[0, 0], [0.45, 1], [0.9, 0]], [[0.16, 0.35], [0.74, 0.35]]],
  C: [[[0.9, 0.8], [0.6, 1], [0.25, 1], [0, 0.75], [0, 0.25], [0.25, 0], [0.6, 0], [0.9, 0.2]]],
  D: [[[0, 0], [0, 1], [0.5, 1], [0.9, 0.7], [0.9, 0.3], [0.5, 0], [0, 0]]],
  E: [[[0.9, 1], [0, 1], [0, 0], [0.9, 0]], [[0, 0.5], [0.65, 0.5]]],
  G: [[[0.9, 0.8], [0.6, 1], [0.2, 1], [0, 0.75], [0, 0.25], [0.2, 0], [0.6, 0], [0.9, 0.2], [0.9, 0.45], [0.55, 0.45]]],
  I: [[[0.45, 0], [0.45, 1]], [[0.15, 1], [0.75, 1]], [[0.15, 0], [0.75, 0]]],
  K: [[[0, 0], [0, 1]], [[0.85, 1], [0.05, 0.45]], [[0.3, 0.62], [0.9, 0]]],
  L: [[[0, 1], [0, 0], [0.85, 0]]],
  N: [[[0, 0], [0, 1]], [[0, 1], [0.9, 0]], [[0.9, 0], [0.9, 1]]],
  O: [[[0.25, 1], [0.65, 1], [0.9, 0.75], [0.9, 0.25], [0.65, 0], [0.25, 0], [0, 0.25], [0, 0.75], [0.25, 1]]],
  R: [[[0, 0], [0, 1]], [[0, 1], [0.6, 1], [0.9, 0.8], [0.9, 0.6], [0.6, 0.45], [0, 0.45]], [[0.35, 0.45], [0.9, 0]]],
  T: [[[0, 1], [0.9, 1]], [[0.45, 1], [0.45, 0]]],
};

const GLYPH_ADVANCE = 1.28;

/** Turns a word into line-segment pairs, centred on the origin. */
function textGeometry(text: string, height: number, tracking = 0): THREE.BufferGeometry {
  const points: number[] = [];
  let pen = 0;

  for (const character of text) {
    if (character === ' ') {
      pen += (GLYPH_ADVANCE * 0.55 + tracking) * height;
      continue;
    }

    const glyph = GLYPHS[character];

    if (glyph) {
      for (const stroke of glyph) {
        for (let i = 0; i < stroke.length - 1; i += 1) {
          const a = stroke[i];
          const b = stroke[i + 1];
          if (!a || !b) continue;

          points.push(pen + a[0] * height, a[1] * height, 0);
          points.push(pen + b[0] * height, b[1] * height, 0);
        }
      }
    }

    pen += (GLYPH_ADVANCE + tracking) * height;
  }

  const width = pen - (GLYPH_ADVANCE + tracking) * height + 0.9 * height;

  // Centre it, so callers position by the middle and never by a corner.
  for (let i = 0; i < points.length; i += 3) {
    points[i] = (points[i] ?? 0) - width / 2;
    points[i + 1] = (points[i + 1] ?? 0) - height / 2;
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.Float32BufferAttribute(points, 3));
  return geometry;
}

/*  Seven-segment digits, because a percentage that re-tessellates a stroke
    font three times a frame is a lot of work to say "47". The segments are
    built once and switched on and off through an attribute. */
const SEGMENTS: readonly (readonly [number, number, number, number])[] = [
  [0, 1, 0.6, 1],      // a  top
  [0.6, 1, 0.6, 0.5],  // b  upper right
  [0.6, 0.5, 0.6, 0],  // c  lower right
  [0, 0, 0.6, 0],      // d  bottom
  [0, 0.5, 0, 0],      // e  lower left
  [0, 1, 0, 0.5],      // f  upper left
  [0, 0.5, 0.6, 0.5],  // g  middle
];

const DIGIT_SEGMENTS: readonly number[] = [
  0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
  0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111,
];

export function createLoader(canvas: HTMLCanvasElement): Loader {
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true });

  /*  CLEARED TRANSPARENT, with the backdrop in CSS. A clear colour here is
      written in LINEAR space and OutputPass converts the whole frame to sRGB
      on the way out, which lifts a near-black like 0x05030e into a visible
      violet-grey - the first version's "dark" field was several stops
      brighter than the site it introduces. CSS is not in that pipeline, so
      the colour there is the colour on screen. */
  renderer.setClearColor(0x000000, 0);

  const scene = new THREE.Scene();

  /*  An ORTHOGRAPHIC camera whose frustum is two units tall whatever the
      window is. Every position below is therefore a fraction of the screen
      and the composition holds at any size - the same reason the interface
      in `ui/` scales rather than reflows. */
  const camera = new THREE.OrthographicCamera(-1, 1, 1, -1, -10, 10);
  camera.position.z = 4;

  const rig = new THREE.Group();
  scene.add(rig);

  /*  A VERTICAL STACK: the dial up top, then the readout, the bar and the
      status line under it. The first version put all four at the origin and
      the bar ran straight through the ring's lower arc while the wordmark
      overflowed its own circle - both invisible in the code and unmissable
      in a screenshot, which is the same lesson the plugin's tab layout keeps
      teaching one floor up. */
  const dial = new THREE.Group();
  dial.position.y = 0.2;
  rig.add(dial);

  const DIAL_RADIUS = 0.3;

  const ACCENT = new THREE.Color(0x64e6ff);
  const DONE = new THREE.Color(0xb48cff);

  const disposables: Array<{ dispose(): void }> = [];

  // --- the dial's ticks ---------------------------------------------------
  /*  96 radial ticks. Which ones are lit is decided in the SHADER from one
      uniform, so filling the dial costs no CPU work per frame and no buffer
      upload - each tick knows where it sits and compares itself. */
  const TICKS = 96;
  const tickPositions: number[] = [];
  const tickFractions: number[] = [];

  for (let i = 0; i < TICKS; i += 1) {
    const fraction = i / TICKS;
    const angle = fraction * Math.PI * 2 - Math.PI / 2;
    const major = i % 8 === 0;
    const inner = DIAL_RADIUS + (major ? 0.02 : 0.04);
    const outer = DIAL_RADIUS + 0.07;

    tickPositions.push(Math.cos(angle) * inner, Math.sin(angle) * inner, 0);
    tickPositions.push(Math.cos(angle) * outer, Math.sin(angle) * outer, 0);
    tickFractions.push(fraction, fraction);
  }

  const tickGeometry = new THREE.BufferGeometry();
  tickGeometry.setAttribute('position', new THREE.Float32BufferAttribute(tickPositions, 3));
  tickGeometry.setAttribute('aFraction', new THREE.Float32BufferAttribute(tickFractions, 1));
  disposables.push(tickGeometry);

  const dialUniforms = {
    uProgress: { value: 0 },
    uTime: { value: 0 },
    uAccent: { value: ACCENT.clone() },
    uOpacity: { value: 1 },
  };

  const tickMaterial = new THREE.ShaderMaterial({
    uniforms: dialUniforms,
    transparent: true,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
    vertexShader: /* glsl */ `
      attribute float aFraction;
      uniform float uProgress;
      uniform float uTime;
      varying float vLit;

      void main() {
        // Filled behind the head, dark ahead of it, and a soft leading edge
        // so the dial reads as travelling rather than as switching.
        float filled = step(aFraction, uProgress);
        float edge = smoothstep(0.06, 0.0, uProgress - aFraction) * filled;

        // A slow sweep over the unfilled part: the dial is alive while it
        // waits, without anything moving fast enough to hurry the viewer.
        float sweep = pow(max(0.0, sin((aFraction - uTime * 0.11) * 6.2831853)), 22.0);

        vLit = filled * 0.78 + edge * 1.5 + (1.0 - filled) * (0.13 + sweep * 0.34);
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }
    `,
    fragmentShader: /* glsl */ `
      uniform vec3 uAccent;
      uniform float uOpacity;
      varying float vLit;

      void main() {
        gl_FragColor = vec4(uAccent * vLit, vLit * uOpacity);
      }
    `,
  });
  disposables.push(tickMaterial);
  dial.add(new THREE.LineSegments(tickGeometry, tickMaterial));

  // --- the arc that fills -------------------------------------------------
  const ARC = 240;
  const arcPositions: number[] = [];
  const arcFractions: number[] = [];

  for (let i = 0; i < ARC; i += 1) {
    const a = i / ARC;
    const b = (i + 1) / ARC;
    const aa = a * Math.PI * 2 - Math.PI / 2;
    const ab = b * Math.PI * 2 - Math.PI / 2;

    arcPositions.push(Math.cos(aa) * DIAL_RADIUS, Math.sin(aa) * DIAL_RADIUS, 0);
    arcPositions.push(Math.cos(ab) * DIAL_RADIUS, Math.sin(ab) * DIAL_RADIUS, 0);
    arcFractions.push(a, b);
  }

  const arcGeometry = new THREE.BufferGeometry();
  arcGeometry.setAttribute('position', new THREE.Float32BufferAttribute(arcPositions, 3));
  arcGeometry.setAttribute('aFraction', new THREE.Float32BufferAttribute(arcFractions, 1));
  disposables.push(arcGeometry);

  const arcMaterial = new THREE.ShaderMaterial({
    uniforms: dialUniforms,
    transparent: true,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
    vertexShader: /* glsl */ `
      attribute float aFraction;
      uniform float uProgress;
      varying float vLit;

      void main() {
        vLit = step(aFraction, uProgress) * 0.9
             + smoothstep(0.03, 0.0, uProgress - aFraction) * 2.2;
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }
    `,
    fragmentShader: /* glsl */ `
      uniform vec3 uAccent;
      uniform float uOpacity;
      varying float vLit;

      void main() {
        if (vLit < 0.01) discard;
        gl_FragColor = vec4(uAccent * vLit, vLit * uOpacity);
      }
    `,
  });
  disposables.push(arcMaterial);
  dial.add(new THREE.LineSegments(arcGeometry, arcMaterial));

  // --- a free-running inner ring, for the sense of a machine idling -------
  const idleGeometry = new THREE.BufferGeometry();
  const idlePositions: number[] = [];

  for (let i = 0; i < 64; i += 1) {
    // Gapped: three arcs rather than a circle, which is what makes it read
    // as an instrument rather than as a spinner.
    if (i % 8 > 4) continue;
    const a = (i / 64) * Math.PI * 2;
    const b = ((i + 1) / 64) * Math.PI * 2;
    const r = DIAL_RADIUS - 0.045;
    idlePositions.push(Math.cos(a) * r, Math.sin(a) * r, 0);
    idlePositions.push(Math.cos(b) * r, Math.sin(b) * r, 0);
  }

  idleGeometry.setAttribute('position', new THREE.Float32BufferAttribute(idlePositions, 3));
  disposables.push(idleGeometry);

  const idleMaterial = new THREE.LineBasicMaterial({
    color: ACCENT,
    transparent: true,
    opacity: 0.3,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
  });
  disposables.push(idleMaterial);

  const idleRing = new THREE.LineSegments(idleGeometry, idleMaterial);
  dial.add(idleRing);

  // --- the wordmark -------------------------------------------------------
  // 0.070, not the 0.105 it started at: five glyphs at that size measured
  // 0.735 across inside a ring 0.60 wide, so the L sat on the arc. Measured
  // off the screenshot rather than guessed a second time.
  const wordGeometry = textGeometry('GNARL', 0.07, 0.1);
  disposables.push(wordGeometry);

  const wordMaterial = new THREE.LineBasicMaterial({
    color: 0xffffff,
    transparent: true,
    opacity: 0.95,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
  });
  disposables.push(wordMaterial);

  const word = new THREE.LineSegments(wordGeometry, wordMaterial);
  dial.add(word);

  // --- the percentage -----------------------------------------------------
  const DIGITS = 3;
  const digitHeight = 0.085;
  const digitPitch = 0.075;
  const digitBaseline = -0.345;
  const digitPositions = new Float32Array(DIGITS * SEGMENTS.length * 2 * 3);
  const digitLit = new Float32Array(DIGITS * SEGMENTS.length * 2);

  for (let d = 0; d < DIGITS; d += 1) {
    const originX = (d - (DIGITS - 1) / 2) * digitPitch - 0.017;

    for (let s = 0; s < SEGMENTS.length; s += 1) {
      const segment = SEGMENTS[s];
      if (!segment) continue;

      const base = (d * SEGMENTS.length + s) * 6;
      digitPositions[base + 0] = originX + segment[0] * digitHeight * 0.62;
      digitPositions[base + 1] = digitBaseline + segment[1] * digitHeight;
      digitPositions[base + 3] = originX + segment[2] * digitHeight * 0.62;
      digitPositions[base + 4] = digitBaseline + segment[3] * digitHeight;
    }
  }

  const digitGeometry = new THREE.BufferGeometry();
  digitGeometry.setAttribute('position', new THREE.BufferAttribute(digitPositions, 3));

  const digitLitAttribute = new THREE.BufferAttribute(digitLit, 1);
  digitLitAttribute.setUsage(THREE.DynamicDrawUsage);
  digitGeometry.setAttribute('aLit', digitLitAttribute);
  disposables.push(digitGeometry);

  const digitUniforms = {
    uAccent: { value: ACCENT.clone() },
    uOpacity: { value: 1 },
  };

  const digitMaterial = new THREE.ShaderMaterial({
    uniforms: digitUniforms,
    transparent: true,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
    vertexShader: /* glsl */ `
      attribute float aLit;
      varying float vLit;
      void main() {
        vLit = aLit;
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }
    `,
    fragmentShader: /* glsl */ `
      uniform vec3 uAccent;
      uniform float uOpacity;
      varying float vLit;
      void main() {
        // The unlit segments stay faintly visible, the way a real readout's
        // do. A digit that vanishes entirely reads as a different display.
        float lit = max(vLit, 0.05);
        gl_FragColor = vec4(uAccent * lit, lit * uOpacity);
      }
    `,
  });
  disposables.push(digitMaterial);
  rig.add(new THREE.LineSegments(digitGeometry, digitMaterial));

  // --- the segmented bar --------------------------------------------------
  const BARS = 44;
  const barPositions: number[] = [];
  const barFractions: number[] = [];

  for (let i = 0; i < BARS; i += 1) {
    const fraction = i / BARS;
    const x = -0.4 + fraction * 0.8;
    barPositions.push(x, -0.515, 0, x, -0.487, 0);
    barFractions.push(fraction, fraction);
  }

  const barGeometry = new THREE.BufferGeometry();
  barGeometry.setAttribute('position', new THREE.Float32BufferAttribute(barPositions, 3));
  barGeometry.setAttribute('aFraction', new THREE.Float32BufferAttribute(barFractions, 1));
  disposables.push(barGeometry);

  const barMaterial = new THREE.ShaderMaterial({
    uniforms: dialUniforms,
    transparent: true,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
    vertexShader: /* glsl */ `
      attribute float aFraction;
      uniform float uProgress;
      uniform float uTime;
      varying float vLit;

      void main() {
        float filled = step(aFraction, uProgress);
        float pulse = 0.82 + 0.18 * sin(uTime * 3.0 - aFraction * 14.0);
        vLit = filled * pulse + (1.0 - filled) * 0.07;
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }
    `,
    fragmentShader: /* glsl */ `
      uniform vec3 uAccent;
      uniform float uOpacity;
      varying float vLit;
      void main() { gl_FragColor = vec4(uAccent * vLit, vLit * uOpacity); }
    `,
  });
  disposables.push(barMaterial);
  rig.add(new THREE.LineSegments(barGeometry, barMaterial));

  // --- the status line ----------------------------------------------------
  const statusMaterial = new THREE.LineBasicMaterial({
    color: ACCENT,
    transparent: true,
    opacity: 0.6,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
  });
  disposables.push(statusMaterial);

  const loadingGeometry = textGeometry('LOADING', 0.027, 0.5);
  const enterGeometry = textGeometry('CLICK TO ENTER', 0.027, 0.5);
  disposables.push(loadingGeometry, enterGeometry);

  const status = new THREE.LineSegments(loadingGeometry, statusMaterial);
  status.position.y = -0.61;
  rig.add(status);

  // --- dust, so the field is not an empty black rectangle -----------------
  const DUST = 420;
  const dustPositions = new Float32Array(DUST * 3);
  const dustSeeds = new Float32Array(DUST);

  for (let i = 0; i < DUST; i += 1) {
    dustPositions[i * 3] = (Math.random() - 0.5) * 3.4;
    dustPositions[i * 3 + 1] = (Math.random() - 0.5) * 2.2;
    dustPositions[i * 3 + 2] = -1;
    dustSeeds[i] = Math.random();
  }

  const dustGeometry = new THREE.BufferGeometry();
  dustGeometry.setAttribute('position', new THREE.BufferAttribute(dustPositions, 3));
  dustGeometry.setAttribute('aSeed', new THREE.BufferAttribute(dustSeeds, 1));
  disposables.push(dustGeometry);

  const dustUniforms = { uTime: { value: 0 }, uOpacity: { value: 1 } };

  const dustMaterial = new THREE.ShaderMaterial({
    uniforms: dustUniforms,
    transparent: true,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
    vertexShader: /* glsl */ `
      attribute float aSeed;
      uniform float uTime;
      varying float vFade;
      void main() {
        vFade = 0.25 + 0.75 * pow(abs(sin(uTime * 0.35 + aSeed * 9.2)), 3.0);
        gl_PointSize = 1.0 + aSeed * 1.6;
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }
    `,
    fragmentShader: /* glsl */ `
      uniform float uOpacity;
      varying float vFade;
      void main() {
        // Additive light SUMS: four hundred of these at a per-point alpha
        // that looks right alone is a grey wash. See journey.ts.
        gl_FragColor = vec4(vec3(0.55, 0.74, 0.95) * vFade, vFade * 0.16 * uOpacity);
      }
    `,
  });
  disposables.push(dustMaterial);

  /*  Its own group, so the dismissal can sweep it past WIDER than the dial.
      Nearer things move faster across the eye; without that difference the
      whole screen scales as one flat card. */
  const dustGroup = new THREE.Group();
  dustGroup.add(new THREE.Points(dustGeometry, dustMaterial));
  scene.add(dustGroup);

  // --- post ---------------------------------------------------------------
  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));

  const bloom = new UnrealBloomPass(new THREE.Vector2(1, 1), 0.62, 0.7, 0.58);
  composer.addPass(bloom);
  composer.addPass(new OutputPass());

  function resize() {
    const width = window.innerWidth;
    const height = window.innerHeight;
    const aspect = width / height;

    camera.left = -aspect;
    camera.right = aspect;
    camera.updateProjectionMatrix();

    /*  THE COMPOSER HAS ITS OWN COPY OF THE PIXEL RATIO, and it takes it
        ONCE, in its constructor - which runs above this function, before any
        of this has been set. So it held the renderer's default of 1 while
        the canvas was being presented at the device's 2 or 3, and every pass
        in the chain rendered the dial, the wordmark and the digits at HALF
        the resolution they were then stretched to. That is the blur: not the
        bloom, not the line width, and nothing a stroke weight could fix.

        `setSize` does not re-read it either - it multiplies by whatever was
        captured - so the ratio has to be pushed in explicitly, after the
        renderer is told and before the targets are sized. The journey does
        not have this bug only by accident of ordering: there the renderer is
        configured before its composer is built. */
    const ratio = Math.min(window.devicePixelRatio, 2);

    renderer.setPixelRatio(ratio);
    renderer.setSize(width, height, false);
    composer.setPixelRatio(ratio);
    composer.setSize(width, height);
    bloom.setSize(width * ratio, height * ratio);
  }

  resize();

  // --- state --------------------------------------------------------------
  let target = 0;
  let shown = 0;
  let isReady = false;
  let dismissing = 0;
  let entered = false;
  let enterCallback: (() => void) | null = null;
  let dismissedCallback: (() => void) | null = null;
  let announcedDismissal = false;

  const pointer = new THREE.Vector2(0.5, 0.5);
  const pointerEase = new THREE.Vector2(0.5, 0.5);

  const onPointerMove = (event: PointerEvent) => {
    pointer.set(event.clientX / window.innerWidth, event.clientY / window.innerHeight);
  };

  const onClick = () => {
    /*  The callback is checked, not optional-called. Accepting a click with
        nothing listening would set `entered`, play the dismissal, and leave
        the page scroll-locked forever with no way back - the one failure on
        this screen that strands the visitor rather than merely looking
        wrong. */
    if (!isReady || entered || !enterCallback) return;
    entered = true;
    enterCallback();
  };

  const onKey = (event: KeyboardEvent) => {
    // A loader you can only leave with a mouse is a loader some people
    // cannot leave. Enter and Space do what the click does.
    if (event.key === 'Enter' || event.key === ' ') onClick();
  };

  window.addEventListener('pointermove', onPointerMove, { passive: true });
  window.addEventListener('pointerdown', onClick);
  window.addEventListener('keydown', onKey);

  function writeDigits(value: number) {
    const clamped = Math.max(0, Math.min(100, Math.round(value * 100)));
    // Leading ZEROES, not blanks. "007" reads as an instrument holding a
    // fixed-width field; " 7" reads as a number that drifted left.
    const text = String(clamped).padStart(DIGITS, '0');

    for (let d = 0; d < DIGITS; d += 1) {
      const character = text[d] ?? '0';
      const mask = DIGIT_SEGMENTS[Number(character)] ?? 0;

      for (let s = 0; s < SEGMENTS.length; s += 1) {
        const lit = (mask >> s) & 1 ? 1 : 0;
        const base = (d * SEGMENTS.length + s) * 2;
        digitLit[base] = lit;
        digitLit[base + 1] = lit;
      }
    }

    digitLitAttribute.needsUpdate = true;
  }

  writeDigits(0);

  return {
    setProgress(value: number) {
      // Monotonic: a bar that goes backwards says the page is confused.
      target = Math.max(target, Math.max(0, Math.min(1, value)));
    },

    ready() {
      isReady = true;
      target = 1;
      // The prompt does NOT swap here. The readout is still easing up, and a
      // screen that says CLICK TO ENTER above the number 085 is a screen
      // arguing with itself - it waits for the count to arrive.
    },

    onEnter(callback: () => void) {
      enterCallback = callback;
    },

    onDismissed(callback: () => void) {
      dismissedCallback = callback;
    },

    entryProgress() {
      return dismissing;
    },

    render(now: number, delta: number) {
      const step = Math.min(delta, 0.1);
      const ease = (rate: number) => 1 - Math.exp(-rate * step);

      /*  Time-based, like everything else here and in journey.ts. A per-frame
          fraction would fill the dial at whatever rate the machine happens to
          render, which on the slow machine - the one that actually sees this
          screen - is the wrong one. */
      shown += (target - shown) * (reduced ? 1 : ease(3.2));
      pointerEase.lerp(pointer, reduced ? 1 : ease(2.6));

      dialUniforms.uProgress.value = shown;
      dialUniforms.uTime.value = now;
      dustUniforms.uTime.value = now;

      writeDigits(shown);

      if (!reduced) {
        idleRing.rotation.z = -now * 0.35;

        // The whole rig leans towards the pointer. Small: it should feel
        // like the panel is aware of you, not like it is being dragged.
        rig.rotation.y = (pointerEase.x - 0.5) * 0.24;
        rig.rotation.x = (pointerEase.y - 0.5) * 0.18;
      }

      if (isReady && shown > 0.995 && status.geometry !== enterGeometry) {
        status.geometry = enterGeometry;
      }

      if (isReady) {
        // Cyan while it works, violet once it is yours to dismiss - the two
        // accents the Dream theme already uses, in the same roles.
        dialUniforms.uAccent.value.lerp(DONE, ease(2.0));
        digitUniforms.uAccent.value.lerp(DONE, ease(2.0));
        statusMaterial.color.lerp(DONE, ease(2.0));
        statusMaterial.opacity = 0.55 + 0.35 * Math.sin(now * 2.4);
      }

      if (entered) {
        dismissing = Math.min(1, dismissing + step * (reduced ? 4.0 : 1.35));

        /*  THE DIAL RUSHES PAST THE CAMERA rather than fading on the spot.
            Cubic, so it barely moves for the first instant and then goes -
            a linear scale reads as a thing being resized, an accelerating
            one reads as a thing coming at you. The journey behind it is
            dollying in on the same number (boot.ts drives both from
            `entryProgress`), so the two halves are one move: the HUD flies
            over your shoulder as the world arrives. */
        const rush = dismissing * dismissing * dismissing;
        rig.scale.setScalar(1 + rush * 7.5);

        // The dust drifts past WIDER than the dial, which is the parallax
        // that sells the depth: nearer things sweep by faster.
        dustGroup.scale.setScalar(1 + rush * 11.0);

        /*  Brightness holds through the first third and then goes quickly.
            Fading it in step with the scale makes it evaporate politely in
            place instead of leaving the frame. */
        const fade = 1 - THREE.MathUtils.smoothstep(dismissing, 0.3, 1.0);

        dialUniforms.uOpacity.value = fade;
        digitUniforms.uOpacity.value = fade;
        dustUniforms.uOpacity.value = fade;
        wordMaterial.opacity = 0.95 * fade;
        statusMaterial.opacity = Math.min(statusMaterial.opacity, fade);
        idleMaterial.opacity = 0.3 * fade;

        // The backdrop clears sooner than the geometry, so the page is
        // already showing through while the dial is still on its way out.
        canvas.style.opacity = String(
          1 - THREE.MathUtils.smoothstep(dismissing, 0.18, 0.82),
        );

        if (dismissing >= 1 && !announcedDismissal) {
          announcedDismissal = true;
          canvas.style.display = 'none';

          // Only now is it safe to tear the renderer down; anything sooner
          // freezes the canvas on a half-drawn frame.
          dismissedCallback?.();
        }
      }

      composer.render();
    },

    resize,

    dispose() {
      // Belt and braces: a disposed renderer leaves whatever it last drew on
      // the canvas, so hide it here too rather than trusting the caller to
      // have waited for the dismissal.
      canvas.style.display = 'none';

      window.removeEventListener('pointermove', onPointerMove);
      window.removeEventListener('pointerdown', onClick);
      window.removeEventListener('keydown', onKey);

      for (const item of disposables) item.dispose();
      composer.dispose();
      renderer.dispose();
    },
  };
}
