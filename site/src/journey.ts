import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';

import { createDriverGeometry, sampleGeometry, sampleModel } from './sampler';

/**
 * One page, one journey: an object TRAVELLING through space with the camera
 * chasing it, becoming something else as it goes.
 *
 * THE CAMERA FOLLOWS, IT DOES NOT ORBIT. This is the difference between the
 * previous version and this one, and it is the whole feel of the thing. An
 * orbiting camera circles a subject that never goes anywhere: the background
 * turns, the subject does not move, and after two seconds you know exactly
 * what the rest of the page will do. A chase camera is somewhere BEHIND
 * something that is going somewhere — the stars stream past because you are
 * moving through them, the subject leads and the camera catches up, and
 * every frame is a place you have not been.
 *
 * The lag is what sells it. The camera is placed at the point on the path
 * the traveller occupied a moment AGO and aimed at where it will be a moment
 * from now, then that position is damped on top. Rigidly parented to the
 * traveller it reads as a diagram; lagging, it reads as a camera operator
 * who is nearly keeping up.
 *
 * FIVE OBJECTS, AND THE TRANSITIONS ARE THE POINT. Orbits around a core,
 * the oscillator's own harmonic surface, a double helix, a loudspeaker
 * driver that burns away, and finally the plugin's interface. Nothing ever
 * cuts: the line figure morphs continuously between the first four, the
 * solid driver cross-fades IN over the lines that already have its shape,
 * and the interface fades up inside the rectangle the lines have become. A
 * cut between two objects is two animations; a morph is one.
 */

export interface Journey {
  setScroll(progress: number): void;
  setPointer(x: number, y: number): void;
  resize(): void;
  render(elapsed: number, delta: number): void;
  dispose(): void;
  /** Whether the camera and the scroll have finished easing. See the
      assignment in the render loop for why the screenshot tool needs it. */
  isSettled(): boolean;
  /**
   * The arrival dolly, 1 = far back and 0 = the composition proper.
   *
   * `boot.ts` walks this down from 1 as the boot screen's dial rushes past,
   * so the two are one move: the HUD leaves over your shoulder while the
   * world comes to meet you. It multiplies the framing distance rather than
   * moving the camera directly, so it composes with the per-formation
   * framing instead of fighting it.
   */
  setEntry(amount: number): void;
  /**
   * The figure's build, 0 = strokes scattered and inbound, 1 = assembled.
   *
   * Each stroke flies in along its own line from its own side, on its own
   * slice of the window, so the figure converges from all around rather than
   * expanding out of a point. Driven from the same clock as the page's own
   * pieces in `assemble.ts`, so the two are one event.
   */
  setAssemble(amount: number): void;
}

/** The plugin's own aspect ratio, so the last formation is its shape. */
const PANEL_ASPECT = 1180 / 720;

/** Orbits, wave, helix, driver, panel. */
const FORMS = 5;

/*  THE CHARACTERISTIC RADIUS OF EACH FORMATION, in the same units the GLSL
    above builds them in. Read off the formation functions: the orbits' ring
    reaches 5.4 + 2.6, the wave spans 22 across, the helix is 20 tall, the
    driver's cone reaches 5.75 - though the dust thrown off the dissolve
    carries well past it, so it is entered larger - and the panel is
    13 x 13/aspect.

    The panel's is deliberately its half-HEIGHT rather than its half
    diagonal. It is a flat wide rectangle, so framing its diagonal against
    the frame's height leaves the width two thirds empty and the instrument
    ends up a postage stamp at the one moment the page is asking you to
    look at it. A radius here means "what should fill the frame", which for
    a wide object is not its bounding sphere.

    These exist because the camera's pull-back is DERIVED from them rather
    than being a constant. A constant pull-back frames whatever the formation
    happens to be sized at: the hero filled 45% of the frame and everything
    after it 10-20%, so the journey appeared to be running away from the
    viewer. Same reasoning as the lateral offset below - a composition is a
    fraction of the frustum, never a number of world units. */
const FORM_RADII = [8.0, 11.0, 10.0, 7.2, 5.8] as const;

/*  And their HALF-WIDTHS, which is a different question from the one above.
    The radii say what should fill the frame vertically; these say what must
    not be cut off horizontally. On a wide screen the two rarely disagree,
    but a phone's frustum is far narrower than it is tall, and the last
    formation is the instrument's own 13-wide rectangle: framed by height
    alone it ran off both edges and the thing the page ends on could not be
    read. The distance is solved for BOTH and the further of the two wins,
    so a subject is never clipped by the axis nobody checked. */
const FORM_HALF_WIDTHS = [8.0, 11.0, 4.7, 5.9, 6.5] as const;

/*  How much of the frame's width the subject may span before it is pushed
    back. 0.78 rather than 0.86: at 0.86 the instrument measured 79% of a
    phone's width with about a tenth of the frame either side, and it was
    still reported as running off the edge - a margin that thin has nothing
    left for the line formation still spread around the panel while the morph
    finishes, or for a phone whose aspect is narrower than the ones tested. A
    subject that fits with room to spare reads as placed; one that fits
    exactly reads as cropped the moment anything moves. */
const SUBJECT_WIDTH_FRACTION = 0.78;

/*  Except at the END, where the instrument is the subject and the page has
    stopped travelling. 0.78 is a margin for a shape still moving; the panel
    is stationary by the time it resolves, so it can have the frame. Eased
    across the last transition rather than switched at it. */
const SUBJECT_WIDTH_FRACTION_END = 0.92;

/*  How high the subject rides on a phone, as a fraction of the visible
    height, measured along the camera's own up vector rather than in world
    units - the distance changes with every formation, so a fixed number of
    units means a different amount of screen each time.

    Two values, because the LAST section's layout is genuinely different. Every
    other card is one screen of copy sitting in the upper middle, so the
    subject rides above it. The download card carries the FAQ, is most of a
    screen tall, and is the one people scroll past the end of - so there the
    subject sits just BELOW centre, in the gap the section leaves under the
    card, which is where it stays visible once the card has scrolled up. One
    value cannot do both: the rise that clears a short card puts the subject
    inside the tall one. */
const MOBILE_RISE = 0.17;
const MOBILE_RISE_END = -0.05;

/** Clamped, so an index off either end frames rather than throwing. */
function formRadius(index: number): number {
  return FORM_RADII[Math.max(0, Math.min(FORM_RADII.length - 1, index))] ?? FORM_RADII[0];
}

function formHalfWidth(index: number): number {
  const i = Math.max(0, Math.min(FORM_HALF_WIDTHS.length - 1, index));
  return FORM_HALF_WIDTHS[i] ?? FORM_HALF_WIDTHS[0];
}

/*  How much of the frame's HEIGHT the subject spans. Height rather than
    width because it is the dimension that does not move with the aspect
    ratio. On a phone the card owns the lower half, so the subject is about a
    quarter of the frame and rides above it; beside a card on desktop it can
    be most of the height. */
const SUBJECT_FRACTION_DESKTOP = 0.80;
const SUBJECT_FRACTION_MOBILE = 0.34;

/*  How much further back the camera sits at the start of the arrival dolly,
    as a multiple of the framing distance. 2.4 puts the hero at roughly a
    third of its final size, which is far enough to read as travel and close
    enough that it is recognisably the same object throughout - much beyond
    this and the orbits arrive as a dot, which reads as a fade-in rather than
    as a move. */
const ENTRY_PUSH = 2.4;

/** Curves in the line figure. Enough that the orbits read as a woven ball. */
const CURVES = 120;

/**
 * A drop-in slot for a model, declared in the page rather than assumed.
 * `<meta name="gnarl-model" content="./models/figure.glb">`. Absent tag, no
 * request — a hard-coded path that is usually missing means every default
 * build logs a 404, which trains you to ignore the console.
 */
function modelUrl(): string | null {
  const content = document
    .querySelector('meta[name="gnarl-model"]')
    ?.getAttribute('content')
    ?.trim();

  return content ? content : null;
}

/**
 * Value noise, shared by the dissolve's surface and its dust.
 *
 * The two MUST agree exactly. If the mesh used one noise and the particles
 * another, dust would appear where the surface was still solid and the whole
 * illusion — that the object is becoming the dust — comes apart. One string,
 * included in both programs, is the only way to be sure they cannot drift.
 */
const NOISE = /* glsl */ `
  vec3 hash3(vec3 p) {
    p = vec3(
      dot(p, vec3(127.1, 311.7, 74.7)),
      dot(p, vec3(269.5, 183.3, 246.1)),
      dot(p, vec3(113.5, 271.9, 124.6))
    );
    return fract(sin(p) * 43758.5453123);
  }

  float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);

    // Smoothstep rather than the raw fraction: linear interpolation leaves
    // the cell boundaries visible as a lattice, which on a dissolve reads as
    // a grid of square holes.
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash3(i + vec3(0.0, 0.0, 0.0)).x;
    float n100 = hash3(i + vec3(1.0, 0.0, 0.0)).x;
    float n010 = hash3(i + vec3(0.0, 1.0, 0.0)).x;
    float n110 = hash3(i + vec3(1.0, 1.0, 0.0)).x;
    float n001 = hash3(i + vec3(0.0, 0.0, 1.0)).x;
    float n101 = hash3(i + vec3(1.0, 0.0, 1.0)).x;
    float n011 = hash3(i + vec3(0.0, 1.0, 1.0)).x;
    float n111 = hash3(i + vec3(1.0, 1.0, 1.0)).x;

    return mix(
      mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
      mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y),
      u.z
    );
  }

  /*  Two octaves. One is too smooth - the holes open as a few big round
      blobs, which reads as melting rather than burning. */
  float dissolveField(vec3 p) {
    return valueNoise(p * 1.7) * 0.68 + valueNoise(p * 4.3) * 0.32;
  }
`;

/** Every formation the line figure walks. */
const FORMATIONS = /* glsl */ `
  const float TAU = 6.28318530718;

  /*  ORBITS. Great circles at scattered orientations: a woven ball of light
      with a bright core, which is what the page opens on. */
  vec3 formOrbits(float c, float t, vec3 s) {
    float a = t * TAU;
    vec3 ring = vec3(cos(a), sin(a), 0.0) * (5.4 + s.x * 2.6);

    float tilt = c * 6.2831853 + s.y * 0.7;
    float spin = c * 7.0 + s.z * 1.3;

    vec3 p = vec3(
      ring.x,
      ring.y * cos(tilt) - ring.z * sin(tilt),
      ring.y * sin(tilt) + ring.z * cos(tilt)
    );

    return vec3(
      p.x * cos(spin) + p.z * sin(spin),
      p.y,
      -p.x * sin(spin) + p.z * cos(spin)
    );
  }

  /*  THE OSCILLATOR'S OWN SURFACE, drawn as its rows - which is what a
      wavetable is, and how the plugin's own display draws it. */
  vec3 formWave(float c, float t, vec3 s) {
    float x = (t - 0.5) * 22.0;
    float z = (c - 0.5) * 14.0;

    float pos = 0.5 + 0.5 * sin(uTime * 0.22);
    float y =
        sin(x * 0.55 + uTime * 0.6) * 1.7
      + sin(x * 1.30 - uTime * 0.35) * 0.85 * pos
      + sin(x * 2.10 + uTime * 0.80) * 0.40 * pos * pos;

    y *= 1.0 - c * 0.45;

    return vec3(x, y, z);
  }

  /*  MODULATION. A double helix: a mod slot is a source and a destination
      travelling together, so the strands are paired. */
  vec3 formHelix(float c, float t, vec3 s) {
    float strand = step(0.5, fract(c * 8.0));
    float a = t * TAU * 2.2 + strand * 3.14159265 + c * 0.7;
    float r = 4.0 + s.x * 0.7;

    return vec3(cos(a) * r, (t - 0.5) * 20.0, sin(a) * r);
  }

  /*  THE DRIVER, as concentric rings - the same cone the solid mesh is, so
      the solid can cross-fade IN over lines that already have its shape.
      That is what makes the fourth transition a morph rather than a cut. */
  vec3 formDriver(float c, float t, vec3 s) {
    float a = t * TAU;
    float r = 0.35 + c * 5.4;

    // The lathe's profile, roughly: dust cap forward, cone falling away,
    // surround rolling back up at the rim.
    float depth = 1.9 - r * 0.62 + smoothstep(4.6, 5.4, r) * 0.9;

    return vec3(cos(a) * r, sin(a) * r, depth);
  }

  /*  THE INSTRUMENT. Its own rectangle, ruled as a grid, at its real aspect
      ratio - the screenshot then fades up inside it. */
  vec3 formPanel(float c, float t, vec3 s) {
    float W = 13.0;
    float H = ${(13 / PANEL_ASPECT).toFixed(4)};

    float across = step(0.5, fract(c * 2.0));
    float lane = floor(c * 30.0) / 29.0;

    vec2 p = across > 0.5
      ? vec2((t - 0.5) * W, (lane - 0.5) * H)
      : vec2((lane - 0.5) * W, (t - 0.5) * H);

    return vec3(p, (s.z - 0.5) * 0.25);
  }

  vec3 formation(int id, float c, float t, vec3 s) {
    if (id <= 0) return formOrbits(c, t, s);
    if (id == 1) return formWave(c, t, s);
    if (id == 2) return formHelix(c, t, s);
    if (id == 3) return formDriver(c, t, s);
    return formPanel(c, t, s);
  }
`;

export async function createJourney(canvas: HTMLCanvasElement): Promise<Journey> {
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  let mobile = window.innerWidth < 860;

  /*  MSAA OFF ON A PHONE. Multisampling costs fill rate on every pass, and
      fill rate is what a phone runs out of first here - the scene is a
      bloomed full-screen composite, not a polygon count. The bloom's own
      blur softens the line edges anyway, which is most of what the
      multisampling was buying. */
  const renderer = new THREE.WebGLRenderer({
    canvas,
    antialias: !mobile,
    powerPreference: 'high-performance',
  });

  /*  1.25 rather than 1.5. A modern phone reports a device pixel ratio of 3,
      so even 1.5 is rendering 2.25x the pixels of the panel it will be shown
      on, through a bloom chain that touches every one of them several times.
      The difference is invisible through the glow and it is the single
      largest lever on whether this runs at 60. */
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, mobile ? 1.25 : 2));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.0;

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x03030a);

  const camera = new THREE.PerspectiveCamera(52, 1, 0.1, 900);

  const disposables: Array<{ dispose(): void }> = [];

  // --- the path -----------------------------------------------------------
  /*  THE ROUTE THE TRAVELLER TAKES. A gentle S through a long stretch of
      space rather than a straight line, because a straight run gives the
      camera nothing to bank into and the stars stream past in a dead
      symmetrical tunnel. Catmull-Rom so the tangent is continuous - a
      polyline would make the camera flick at every corner. */
  const path = new THREE.CatmullRomCurve3(
    [
      new THREE.Vector3(0, 0, 0),
      new THREE.Vector3(22, 8, -60),
      new THREE.Vector3(-14, -6, -130),
      new THREE.Vector3(18, 10, -205),
      new THREE.Vector3(-8, 2, -280),
      new THREE.Vector3(6, -4, -350),
    ],
    false,
    'catmullrom',
    0.5,
  );

  // --- the stars ----------------------------------------------------------
  /*  SCATTERED ALONG THE PATH, not on a shell around the origin. The camera
      travels three hundred and fifty units; a shell would be behind it after
      the first section and the rest of the page would be flown in a void.
      Each star is dropped near a random point on the route, so there is sky
      the whole way and it STREAMS past, which is what says you are moving. */
  const STARS = mobile ? 4500 : 12000;

  const starGeometry = new THREE.BufferGeometry();
  const starPos = new Float32Array(STARS * 3);
  const starSeed = new Float32Array(STARS * 2);

  const scratch = new THREE.Vector3();

  for (let i = 0; i < STARS; i += 1) {
    path.getPointAt(Math.random(), scratch);

    // A shell around that point, far enough out that the traveller never
    // flies into one and near enough to show parallax.
    const r = 45 + Math.random() * 130;
    const theta = Math.random() * Math.PI * 2;
    const phi = Math.acos(1 - 2 * Math.random());

    starPos[i * 3] = scratch.x + r * Math.sin(phi) * Math.cos(theta);
    starPos[i * 3 + 1] = scratch.y + r * Math.cos(phi);
    starPos[i * 3 + 2] = scratch.z + r * Math.sin(phi) * Math.sin(theta);

    starSeed[i * 2] = Math.random();
    starSeed[i * 2 + 1] = Math.random();
  }

  starGeometry.setAttribute('position', new THREE.BufferAttribute(starPos, 3));
  starGeometry.setAttribute('aSeed', new THREE.BufferAttribute(starSeed, 2));
  disposables.push(starGeometry);

  const starUniforms = {
    uTime: { value: 0 },
    uPixelRatio: { value: renderer.getPixelRatio() },
  };

  const starMaterial = new THREE.ShaderMaterial({
    uniforms: starUniforms,
    transparent: true,
    depthWrite: false,
    blending: THREE.AdditiveBlending,
    vertexShader: /* glsl */ `
      precision highp float;

      attribute vec2 aSeed;
      uniform float uTime;
      uniform float uPixelRatio;

      varying float vBright;
      varying float vWarm;

      void main() {
        vec4 mv = modelViewMatrix * vec4(position, 1.0);

        /*  Unequal, and steeply so. A field of equally bright dots reads as
            noise; a few bright among many faint reads as a sky. */
        float b = aSeed.x * aSeed.x * aSeed.x * aSeed.x;
        b *= 0.65 + 0.35 * sin(uTime * 0.6 + aSeed.y * 90.0);

        vBright = b;
        vWarm = aSeed.y;

        // Attenuated with distance, or the far half of a volume this deep
        // is the same size as the near half and the depth disappears.
        gl_PointSize = (0.6 + b * 3.0) * uPixelRatio
                     * (90.0 / max(-mv.z, 1.0));

        gl_Position = projectionMatrix * mv;
      }
    `,
    fragmentShader: /* glsl */ `
      precision highp float;

      varying float vBright;
      varying float vWarm;

      void main() {
        vec2 d = gl_PointCoord - 0.5;
        float r = length(d);
        if (r > 0.5) discard;

        float alpha = smoothstep(0.5, 0.05, r);

        vec3 cool = vec3(0.62, 0.78, 1.00);
        vec3 warm = vec3(1.00, 0.82, 0.58);
        vec3 colour = mix(cool, warm, smoothstep(0.55, 1.0, vWarm));

        gl_FragColor = vec4(colour, alpha * (0.16 + vBright * 0.9));
      }
    `,
  });

  disposables.push(starMaterial);

  const stars = new THREE.Points(starGeometry, starMaterial);
  stars.frustumCulled = false;
  scene.add(stars);

  // --- the traveller ------------------------------------------------------
  /*  Everything that moves along the path lives in here, so the journey is
      one transform rather than five things kept in step by hand. */
  const traveller = new THREE.Group();
  scene.add(traveller);

  // ... the line figure ....................................................
  const SEGMENTS = mobile ? 34 : 56;
  const VERTS = CURVES * SEGMENTS * 2;

  const figureGeometry = new THREE.BufferGeometry();

  const aCurve = new Float32Array(VERTS);
  const aT = new Float32Array(VERTS);
  const aSeed = new Float32Array(VERTS * 3);

  let v = 0;

  for (let c = 0; c < CURVES; c += 1) {
    const curve = c / (CURVES - 1);

    /*  ONE SEED PER CURVE, not per vertex. The seed places and orients the
        whole stroke, so every vertex on it has to agree about what it is -
        per-vertex randomness would make a scribble rather than a line. */
    const s0 = Math.random();
    const s1 = Math.random();
    const s2 = Math.random();

    for (let seg = 0; seg < SEGMENTS; seg += 1) {
      for (const t of [seg / SEGMENTS, (seg + 1) / SEGMENTS]) {
        aCurve[v] = curve;
        aT[v] = t;
        aSeed[v * 3] = s0;
        aSeed[v * 3 + 1] = s1;
        aSeed[v * 3 + 2] = s2;
        v += 1;
      }
    }
  }

  figureGeometry.setAttribute(
    'position',
    new THREE.BufferAttribute(new Float32Array(VERTS * 3), 3),
  );
  figureGeometry.setAttribute('aCurve', new THREE.BufferAttribute(aCurve, 1));
  figureGeometry.setAttribute('aT', new THREE.BufferAttribute(aT, 1));
  figureGeometry.setAttribute('aSeed', new THREE.BufferAttribute(aSeed, 3));
  figureGeometry.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 40);
  disposables.push(figureGeometry);

  const figureUniforms = {
    uForm: { value: 0 },
    uTime: { value: 0 },
    uFade: { value: 1 },
    // 0 = scattered and inbound, 1 = assembled. Starts scattered, because
    // the scene is built behind the boot screen and must not be found
    // already finished the moment the loader clears.
    uAssemble: { value: 0 },
    uAccent: { value: new THREE.Color(0x7de6ff) },
    uAccent2: { value: new THREE.Color(0xb98cff) },
  };

  const figureMaterial = new THREE.ShaderMaterial({
    uniforms: figureUniforms,
    transparent: true,
    depthWrite: false,
    blending: THREE.AdditiveBlending,
    vertexShader: /* glsl */ `
      precision highp float;

      attribute float aCurve;
      attribute float aT;
      attribute vec3  aSeed;

      uniform float uForm;
      uniform float uTime;
      uniform float uAssemble;

      varying float vGlow;
      varying float vDepth;
      varying float vBuild;

      ${FORMATIONS}

      /*  WHERE EACH STROKE COMES IN FROM. A direction off the seed rather
          than a scatter of absolute positions, so every stroke flies in
          along its own line from its own side and the figure converges from
          all around instead of expanding out of a point - which is what
          makes it read as being BUILT rather than zoomed. */
      vec3 approach(vec3 s) {
        float theta = s.x * 6.2831853;
        float phi   = s.y * 3.14159265;

        return vec3(
          sin(phi) * cos(theta),
          cos(phi),
          sin(phi) * sin(theta)
        ) * (26.0 + s.z * 46.0);
      }

      void main() {
        float f = clamp(uForm, 0.0, ${(FORMS - 1).toFixed(1)});
        int   a = int(floor(f));
        int   b = int(min(floor(f) + 1.0, ${(FORMS - 1).toFixed(1)}));
        float t = fract(f);

        /*  Staggered by curve, and eased. Blending every vertex at once
            turns the figure into a smear for the middle of the transition;
            staggering means curves leave and arrive at different moments,
            so it reorganises itself rather than being squashed. */
        float stagger = aSeed.x * 0.35;
        float te = smoothstep(0.0, 1.0, clamp((t - stagger) / 0.65, 0.0, 1.0));

        vec3 p = mix(
          formation(a, aCurve, aT, aSeed),
          formation(b, aCurve, aT, aSeed),
          te
        );

        /*  THE BUILD. Each stroke has its own slice of the window, keyed off
            its seed, so they land over a period rather than all at once - a
            figure whose every stroke arrives on the same frame is a figure
            that pops into being, which is the thing this exists to avoid.
            The last strokes are still settling as the first are already
            holding, and that overlap is the whole effect. */
        float bDelay = aSeed.y * 0.42 + aSeed.z * 0.12;
        float bt = clamp((uAssemble - bDelay) / 0.46, 0.0, 1.0);

        // Cubic ease out: fast out of the dark, decelerating onto the mark.
        float be = 1.0 - pow(1.0 - bt, 3.0);

        vec3 from = p + approach(aSeed);

        // A little spin on the way in, unwinding to nothing as it lands.
        float swirl = (1.0 - be) * (aSeed.x - 0.5) * 2.4;
        float cs = cos(swirl);
        float sn = sin(swirl);
        from = vec3(from.x * cs - from.z * sn, from.y, from.x * sn + from.z * cs);

        p = mix(from, p, be);

        vec4 mv = modelViewMatrix * vec4(p, 1.0);

        vGlow  = 1.0 - abs(te * 2.0 - 1.0);
        vDepth = clamp((length(p) - 2.0) / 12.0, 0.0, 1.0);
        vBuild = be;

        gl_Position = projectionMatrix * mv;
      }
    `,
    fragmentShader: /* glsl */ `
      precision highp float;

      uniform vec3  uAccent;
      uniform vec3  uAccent2;
      uniform float uFade;

      varying float vGlow;
      varying float vDepth;
      varying float vBuild;

      void main() {
        if (uFade <= 0.001) discard;

        // Cyan at the core, violet at the edges - the plugin's own two
        // accents, in the roles they hold there: cyan is what is live.
        vec3 colour = mix(uAccent, uAccent2, vDepth) + vGlow * 0.7;

        /*  A stroke still travelling is brighter and thinner than one that
            has landed, so the incoming ones read as live and the settled
            ones sit back. Without this the build is only movement; with it
            the figure looks like it is being drawn. */
        float arriving = 1.0 - vBuild;
        colour += arriving * 0.55;

        gl_FragColor = vec4(colour, (0.26 + vGlow * 0.34) * uFade * (0.35 + vBuild * 0.65));
      }
    `,
  });

  disposables.push(figureMaterial);

  const figure = new THREE.LineSegments(figureGeometry, figureMaterial);
  figure.frustumCulled = false;
  traveller.add(figure);

  // ... the core ...........................................................
  const coreMaterial = new THREE.SpriteMaterial({
    map: createGlowTexture(),
    color: 0xbfe9ff,
    transparent: true,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
  });
  disposables.push(coreMaterial);

  const core = new THREE.Sprite(coreMaterial);
  traveller.add(core);

  // ... the driver, which burns ............................................
  const motes = mobile ? 22000 : 55000;
  const url = modelUrl();
  const fromModel = url ? await sampleModel(url, { count: motes, size: 11 }) : null;

  const driverGeometry = createDriverGeometry();
  driverGeometry.computeBoundingBox();

  const span = driverGeometry.boundingBox!.getSize(new THREE.Vector3());
  const fit = 11 / Math.max(span.x, span.y, span.z);
  driverGeometry.scale(fit, fit, fit);
  driverGeometry.center();
  disposables.push(driverGeometry);

  const cloud = fromModel ?? sampleGeometry(driverGeometry, { count: motes, size: 11 });

  const dissolveUniforms = {
    uThreshold: { value: -0.2 },
    uEdge: { value: 0.07 },
    uFade: { value: 0 },
    uBody: { value: new THREE.Color(0x2a2150) },
    uRim: { value: new THREE.Color(0x7de6ff) },
    uHot: { value: new THREE.Color(0xff9a3c) },
  };

  const driverMaterial = new THREE.ShaderMaterial({
    uniforms: dissolveUniforms,
    side: THREE.DoubleSide,
    transparent: true,
    vertexShader: /* glsl */ `
      precision highp float;

      varying vec3 vObject;
      varying vec3 vNormal;
      varying vec3 vView;

      void main() {
        // OBJECT space: in world space the noise is nailed to the room and
        // rotating the mesh makes the burn crawl across it like a
        // searchlight rather than like the thing itself decaying.
        vObject = position;
        vNormal = normalize(normalMatrix * normal);

        vec4 mv = modelViewMatrix * vec4(position, 1.0);
        vView = -mv.xyz;

        gl_Position = projectionMatrix * mv;
      }
    `,
    fragmentShader: /* glsl */ `
      precision highp float;

      uniform float uThreshold;
      uniform float uEdge;
      uniform float uFade;
      uniform vec3  uBody;
      uniform vec3  uRim;
      uniform vec3  uHot;

      varying vec3 vObject;
      varying vec3 vNormal;
      varying vec3 vView;

      ${NOISE}

      void main() {
        if (uFade <= 0.001) discard;

        float n = dissolveField(vObject);
        if (n < uThreshold) discard;

        /*  The band of surface ABOUT to go. Its width is in field units, so
            it stays the same visual thickness however fast the threshold is
            moving - tied to the threshold's speed instead, a fast scroll
            would smear and a slow one would show no edge at all. */
        float edge = 1.0 - smoothstep(uThreshold, uThreshold + uEdge, n);

        /*  A rim light. There are no lights in this scene; this is the
            cheapest thing that gives a surface a form, and it costs one dot
            product. At any more than a third it becomes a halo and the form
            disappears into its own glow. */
        float facing = 1.0 - abs(dot(normalize(vNormal), normalize(vView)));
        vec3 colour = uBody + uRim * pow(clamp(facing, 0.0, 1.0), 2.2) * 0.32;

        colour = mix(colour, uHot, edge * 0.9);
        colour += vec3(1.0) * pow(edge, 6.0) * 1.1;

        gl_FragColor = vec4(colour, uFade);
      }
    `,
  });

  disposables.push(driverMaterial);

  const driver = new THREE.Mesh(driverGeometry, driverMaterial);
  traveller.add(driver);

  // ... its dust ...........................................................
  const moteGeometry = new THREE.BufferGeometry();
  moteGeometry.setAttribute('position', new THREE.BufferAttribute(cloud.positions, 3));
  moteGeometry.setAttribute('aNormal', new THREE.BufferAttribute(cloud.normals, 3));

  const moteSeed = new Float32Array(cloud.count * 3);
  for (let i = 0; i < cloud.count * 3; i += 1) moteSeed[i] = Math.random();

  moteGeometry.setAttribute('aSeed', new THREE.BufferAttribute(moteSeed, 3));
  moteGeometry.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 60);
  disposables.push(moteGeometry);

  const moteUniforms = {
    uThreshold: dissolveUniforms.uThreshold,
    uFade: dissolveUniforms.uFade,
    uTime: { value: 0 },
    uPixelRatio: { value: renderer.getPixelRatio() },
    uHot: dissolveUniforms.uHot,
    uCool: { value: new THREE.Color(0x7de6ff) },
  };

  const moteMaterial = new THREE.ShaderMaterial({
    uniforms: moteUniforms,
    transparent: true,
    depthWrite: false,
    blending: THREE.AdditiveBlending,
    vertexShader: /* glsl */ `
      precision highp float;

      attribute vec3 aNormal;
      attribute vec3 aSeed;

      uniform float uThreshold;
      uniform float uTime;
      uniform float uPixelRatio;

      varying float vAge;

      ${NOISE}

      void main() {
        /*  THE SAME FIELD AT THE SAME POINT the surface shader reads, so
            this mote lets go exactly when the surface beneath it opens. */
        float n = dissolveField(position);

        /*  The window and the threshold's end have to agree. The field tops
            out at 1, so the threshold must clear 1 by MORE than this window
            or the last motes released are still at full brightness when the
            page ends - and sixty thousand of those bunched together is a
            white wall, which is exactly what it was. */
        float age = clamp((uThreshold - n) / 0.35, 0.0, 1.0);
        vAge = age;

        vec3 p = position;

        if (age > 0.0) {
          // Off along the normal first - dust leaves a surface
          // perpendicular to it - then drifting and rising. The cube is what
          // makes the release feel like one: slow at the lip, quick once free.
          float travel = age * age * age * 9.0;

          p += aNormal * travel;
          p += vec3(
            sin(uTime * 0.7 + aSeed.x * 40.0),
            cos(uTime * 0.5 + aSeed.y * 40.0) + 1.3,
            sin(uTime * 0.6 + aSeed.z * 40.0)
          ) * travel * 0.45;
        }

        vec4 mv = modelViewMatrix * vec4(p, 1.0);

        gl_PointSize = (1.0 + aSeed.x * 1.6) * (1.0 - age * 0.7)
                     * uPixelRatio * (85.0 / max(-mv.z, 0.001));

        gl_Position = projectionMatrix * mv;
      }
    `,
    fragmentShader: /* glsl */ `
      precision highp float;

      uniform vec3  uHot;
      uniform vec3  uCool;
      uniform float uFade;

      varying float vAge;

      void main() {
        // Motes not yet released must not draw, or the solid object wears a
        // haze of dust it has not shed.
        if (vAge <= 0.0 || uFade <= 0.001) discard;

        vec2 d = gl_PointCoord - 0.5;
        float r = length(d);
        if (r > 0.5) discard;

        float alpha = smoothstep(0.5, 0.0, r);

        // Hot at release, cooling to the instrument's cyan as it drifts.
        vec3 colour = mix(uHot, uCool, smoothstep(0.0, 0.45, vAge));

        /*  LOW. Fifty-five thousand additive sprites is a lot of light: at
            the alpha that looks right for ONE the cloud sums to flat white
            and the bloom turns the frame into a lamp. */
        gl_FragColor = vec4(colour, alpha * (1.0 - vAge) * 0.115 * uFade);
      }
    `,
  });

  disposables.push(moteMaterial);

  const dust = new THREE.Points(moteGeometry, moteMaterial);
  dust.frustumCulled = false;
  traveller.add(dust);

  // ... the interface ......................................................
  const loader = new THREE.TextureLoader();
  const panelTexture = loader.load('./ui-osc.webp');
  panelTexture.colorSpace = THREE.SRGBColorSpace;
  disposables.push(panelTexture);

  const panelMaterial = new THREE.MeshBasicMaterial({
    map: panelTexture,
    transparent: true,
    opacity: 0,
    toneMapped: false,
    depthWrite: false,
  });
  disposables.push(panelMaterial);

  const panel = new THREE.Mesh(
    new THREE.PlaneGeometry(13, 13 / PANEL_ASPECT),
    panelMaterial,
  );
  disposables.push(panel.geometry);
  traveller.add(panel);

  // --- post ---------------------------------------------------------------
  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));

  /*  The threshold is the important number, not the strength. Low enough
      that the thin lines cross it - they are the thing meant to glow - and
      high enough that the faint half of the starfield does not, which would
      turn the sky into milk. */
  const bloom = new UnrealBloomPass(
    new THREE.Vector2(1, 1),
    mobile ? 0.6 : 0.82,
    0.66,
    0.28,
  );
  composer.addPass(bloom);
  composer.addPass(new OutputPass());

  // --- state --------------------------------------------------------------
  const pointer = new THREE.Vector2(0.5, 0.5);
  const pointerEase = new THREE.Vector2(0.5, 0.5);
  let scroll = 0;
  let scrollEase = 0;

  const here = new THREE.Vector3();
  const behind = new THREE.Vector3();
  const ahead = new THREE.Vector3();
  const desired = new THREE.Vector3();
  let settled = false;

  /*  Starts FAR. The scene is built while the boot screen is still up, so
      its first drawn frame is already at the end of the arrival dolly rather
      than jumping there once the loader clears. */
  let entry = 1;
  const lookTarget = new THREE.Vector3();
  const forward = new THREE.Vector3();
  const back = new THREE.Vector3();
  const right = new THREE.Vector3();
  const upVector = new THREE.Vector3();

  // Seeded from the path so the first frame is not a lurch from the origin.
  path.getPointAt(0, camera.position);
  camera.position.z += 26;

  function resize(): void {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;

    mobile = window.innerWidth < 860;

    renderer.setSize(width, height, false);
    composer.setSize(width, height);

    /*  The bloom runs at HALF resolution on a phone. It is a blur: its whole
        output is low frequency, so the mip chain it builds from a half-size
        target is very nearly the same picture for a quarter of the pixels
        touched. This and the pixel ratio above are the two numbers that
        decide whether a phone holds 60. */
    const bloomScale = mobile ? 0.5 : 1;
    bloom.setSize(width * bloomScale, height * bloomScale);

    starUniforms.uPixelRatio.value = renderer.getPixelRatio();
    moteUniforms.uPixelRatio.value = renderer.getPixelRatio();

    camera.aspect = width / height;
    camera.fov = mobile ? 66 : 52;
    camera.updateProjectionMatrix();
  }

  return {
    setScroll(progress: number) {
      scroll = progress;
    },

    setPointer(x: number, y: number) {
      pointer.set(x, y);
    },

    isSettled() {
      return settled;
    },

    setEntry(amount: number) {
      entry = Math.max(0, Math.min(1, amount));
    },

    setAssemble(amount: number) {
      figureUniforms.uAssemble.value = Math.max(0, Math.min(1, amount));
    },

    resize,

    render(elapsed: number, delta: number) {
      figureUniforms.uTime.value = elapsed;
      starUniforms.uTime.value = elapsed;
      moteUniforms.uTime.value = elapsed;

      /*  DAMPING IS A FUNCTION OF TIME, NOT OF FRAMES. `x += (t - x) * k`
          advances k per FRAME: half a second to settle at 60 fps, two
          seconds at 15 - and each frame jumps k times whatever distance a
          fast flick opened, which is what made an earlier version appear to
          teleport. Same exponential, sampled correctly. */
      const ease = (rate: number) => 1 - Math.exp(-rate * Math.min(delta, 0.1));

      if (reduced) {
        scrollEase = scroll;
        pointerEase.copy(pointer);
      } else {
        scrollEase += (scroll - scrollEase) * ease(4.0);
        pointerEase.lerp(pointer, ease(2.5));
      }

      /*  Already normalised by boot.ts against the LAST SECTION's centre
          rather than the bottom of the document, so u = 1 is the download
          section arriving and not the foot of the page. */
      const u = Math.min(1, Math.max(0, scrollEase));

      // --- where the traveller is, and where the camera is chasing from --
      path.getPointAt(u, here);
      path.getPointAt(Math.max(0, u - 0.055), behind);
      path.getPointAt(Math.min(1, u + 0.02), ahead);

      traveller.position.copy(here);

      /*  The camera sits where the traveller WAS and aims at where it is
          GOING. Parenting it rigidly would read as a diagram; this reads as
          somebody following. The pointer leans the offset, which is the
          whole of the interactivity - it is the viewer nudging a camera
          that is already busy, not driving it. */
      /*  The DIRECTION the camera hangs in: back along the chord from ahead
          to behind, lifted, and leaned by the pointer. A chord rather than
          the exact tangent on purpose - on a bend it points slightly wide,
          which is what makes the camera swing outside a corner the way one
          being carried does. */
      back.subVectors(behind, ahead).normalize().multiplyScalar(0.86);
      back.y += 0.21 + (pointerEase.y - 0.5) * -0.16;
      back.x += (pointerEase.x - 0.5) * 0.24;
      back.normalize();

      /*  And the DISTANCE, derived so the subject spans the same fraction of
          the frame whichever formation it currently is. Interpolated on the
          same uForm the shader morphs on, so the framing eases through a
          transition exactly as the shape does rather than stepping at it. */
      const form = u * (FORMS - 1);
      const lower = Math.min(FORMS - 1, Math.floor(form));
      const upper = Math.min(FORMS - 1, lower + 1);
      const radius = THREE.MathUtils.lerp(
        formRadius(lower),
        formRadius(upper),
        form - lower,
      );

      const halfWidth = THREE.MathUtils.lerp(
        formHalfWidth(lower),
        formHalfWidth(upper),
        form - lower,
      );

      const fraction = mobile ? SUBJECT_FRACTION_MOBILE : SUBJECT_FRACTION_DESKTOP;
      const halfFov = Math.tan((camera.fov * Math.PI) / 360);

      // visibleHeight = 2 d tan(fov/2), and we want 2r = fraction x that.
      const forHeight = radius / (fraction * halfFov);

      /*  visibleWidth = visibleHeight x aspect, so the same solve with the
          aspect divided back out. The FURTHER of the two wins: whichever
          axis would clip decides the distance. */
      const widthFraction = THREE.MathUtils.lerp(
        SUBJECT_WIDTH_FRACTION,
        SUBJECT_WIDTH_FRACTION_END,
        THREE.MathUtils.smoothstep(form, 3.2, 4.0),
      );

      const forWidth =
        halfWidth / (widthFraction * halfFov * Math.max(0.1, camera.aspect));

      const base = Math.max(forHeight, forWidth);

      /*  The arrival dolly MULTIPLIES that distance rather than adding to it,
          so it scales with whatever the current formation needs and cannot
          shove the camera inside a small one or leave a large one a speck. */
      const framing = base * (1 + entry * ENTRY_PUSH);

      desired.copy(here).addScaledVector(back, framing);

      /*  Damped on top of the lag. Without this the camera is rigidly tied
          to a point on the curve and inherits every wobble in the spline;
          with it, it swings wide on the corners the way a chase camera
          does. */
      camera.position.lerp(desired, reduced ? 1 : ease(2.2));

      /*  Whether everything has finished easing. This exists for
          `tools/screenshot_site.mjs`, and it is not a debugging leftover: a
          fixed wait before a shot is a GUESS at the frame rate, and under
          software GL in CI the page runs at a few frames a second. The delta
          clamp below means easing then advances at most 0.1s per frame, so a
          2.6s wait bought five frames of a two-second ease and every picture
          was taken with the camera still closing on its mark. The tool asks
          instead of assuming. */
      settled =
        Math.abs(scroll - scrollEase) < 0.0005 &&
        camera.position.distanceTo(desired) < 0.05 &&
        // The arrival dolly and the figure's build BOTH count as unsettled,
        // or the screenshot tool photographs the page mid-arrival and calls
        // it the composition. The build outlasts the dolly, so leaving it
        // out here would let a shot through with strokes still inbound.
        entry < 0.001 &&
        figureUniforms.uAssemble.value > 0.999;

      lookTarget.copy(here);

      if (!mobile) {
        /*  The subject is held a fixed fraction of the frame to the RIGHT,
            because the cards sit on the left. Measured along the camera's
            own right vector, not world x: the camera turns through the
            whole journey, and world x becomes depth halfway round. */
        const distance = camera.position.distanceTo(here);
        const visibleWidth =
          2 * distance * Math.tan((camera.fov * Math.PI) / 360) * camera.aspect;

        forward.subVectors(here, camera.position).normalize();
        right.crossVectors(forward, camera.up).normalize();
        lookTarget.addScaledVector(right, -visibleWidth * 0.2);
      } else {
        /*  On a phone the card owns the lower part of the screen, so the
            subject rides above it - as a FRACTION of the frustum, measured
            along the camera's own up vector. It was a flat `y -= 6.0`, which
            is a fixed number of world units against a distance that changes
            with every formation: the same offset that lifted the orbits
            clear of the card barely moved the instrument, which sits three
            times further away. */
        const distance = camera.position.distanceTo(here);
        const visibleHeight = 2 * distance * Math.tan((camera.fov * Math.PI) / 360);

        forward.subVectors(here, camera.position).normalize();
        right.crossVectors(forward, camera.up).normalize();
        upVector.crossVectors(right, forward).normalize();

        // Eased across the last transition rather than switched at it, or the
        // subject would jump the moment the form index crossed 3.
        const rise = THREE.MathUtils.lerp(
          MOBILE_RISE,
          MOBILE_RISE_END,
          THREE.MathUtils.smoothstep(form, 3.0, 4.0),
        );

        lookTarget.addScaledVector(upVector, -visibleHeight * rise);
      }

      camera.lookAt(lookTarget);


      // --- which object, and the cross-fades between them ----------------
      figureUniforms.uForm.value = form;

      /*  NOTHING EVER CUTS. The solid driver fades in over the line rings
          that already have its shape, burns, and the interface fades up
          inside the rectangle the lines have become. Each overlap is wide
          enough that both are visible together for a moment - which is what
          makes it read as one object changing rather than two swapping. */
      const solid = THREE.MathUtils.smoothstep(u, 0.60, 0.70);
      const gone = THREE.MathUtils.smoothstep(u, 0.80, 0.88);

      dissolveUniforms.uFade.value = solid * (1 - gone);

      // The burn runs well past 1: the field tops out there, and it must
      // clear it by more than the motes' fade window.
      dissolveUniforms.uThreshold.value = THREE.MathUtils.lerp(
        -0.05,
        1.5,
        THREE.MathUtils.smoothstep(u, 0.68, 0.90),
      );

      // The lines dim while the solid owns the frame, and come back to rule
      // the rectangle the interface arrives in.
      figureUniforms.uFade.value = 1 - solid * (1 - gone) * 0.75;

      const resolve = THREE.MathUtils.smoothstep(u, 0.88, 0.99);
      panelMaterial.opacity = resolve * 0.95;
      panel.visible = resolve > 0.001;

      // The core belongs to the opening formation and fades with it.
      const starness = 1 - THREE.MathUtils.smoothstep(u, 0.0, 0.22);
      coreMaterial.opacity = 0.10 + starness * 0.52;
      core.scale.setScalar(3.0 + starness * 2.8);

      /*  Everything that faces the viewer does so as a group. The panel and
          the core are flat, and on a path that turns they would otherwise
          edge-on themselves into invisibility. */
      panel.quaternion.copy(camera.quaternion);
      panel.position.set(0, 0, 0);

      // A slow turn, so the burn is seen crossing the form.
      driver.rotation.y = elapsed * 0.12;
      dust.rotation.y = driver.rotation.y;

      composer.render();
    },

    dispose() {
      for (const item of disposables) item.dispose();
      composer.dispose();
      renderer.dispose();
    },
  };
}

/** A soft radial sprite, drawn rather than fetched — it is a gradient. */
function createGlowTexture(): THREE.Texture {
  const size = 128;
  const canvas = document.createElement('canvas');
  canvas.width = size;
  canvas.height = size;

  const context = canvas.getContext('2d');

  if (context) {
    const gradient = context.createRadialGradient(
      size / 2, size / 2, 0,
      size / 2, size / 2, size / 2,
    );
    gradient.addColorStop(0, 'rgba(255,255,255,1)');
    gradient.addColorStop(0.18, 'rgba(255,255,255,0.65)');
    gradient.addColorStop(0.45, 'rgba(255,255,255,0.16)');
    gradient.addColorStop(1, 'rgba(255,255,255,0)');
    context.fillStyle = gradient;
    context.fillRect(0, 0, size, size);
  }

  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  return texture;
}
