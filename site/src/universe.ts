import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';

import { createStarfield, type Starfield } from './starfield';

/**
 * The universe: a figure drawn in light, turning in a starfield, which keeps
 * becoming something else as the page scrolls.
 *
 * TWO LAYERS, AND THE SEPARATION IS THE WHOLE LOOK.
 *
 * The BACKDROP is a deep starfield — thousands of small, sharp, unequal
 * points at a fixed distance, drifting slowly. It is not part of the figure
 * and never morphs; it is the room the figure is in, and it is what gives
 * every camera move something to move against.
 *
 * The FIGURE is drawn in LINES, not points. This is the correction that
 * mattered most: a first version built it out of ninety thousand soft
 * sprites, and a dense field of soft sprites is a CLOUD — it reads as fog or
 * as a nebula, never as a drawing. Measured in a screenshot it was a white
 * disc with the copy floating on it. The look this is built against is line
 * art: thin bright curves on black, legible because the strokes are thin and
 * the gaps are empty. Segments a pixel wide, run through bloom, are that.
 * Sprites at any size are not.
 *
 * NOTHING IS IN A FIXED PLACE. An earlier version put three lit slabs at
 * three points in a corridor and flew between them, which meant most of the
 * scroll was spent crossing empty space — and a fast flick crossed it in a
 * handful of frames and read as a cut. There is one figure, always centred,
 * always the subject; the scroll changes WHAT IT IS. With nothing to travel
 * to, there is nothing to skip.
 *
 * ALL THE MORPHING IS IN THE VERTEX SHADER. Every vertex knows which curve
 * it belongs to and how far along it sits, and computes each formation from
 * those two numbers; the CPU sends one float saying where between two
 * formations we are.
 */

export interface Universe {
  setScroll(progress: number): void;
  setPointer(x: number, y: number): void;
  resize(): void;
  render(elapsed: number, delta: number): void;
  dispose(): void;
}

/** The plugin's own aspect ratio, so the last formation is its shape. */
const PANEL_ASPECT = 1180 / 720;

/** How many formations the scroll walks. Five sections, five states. */
const FORMS = 5;

/*  The figure is built from this many separate curves, each drawn as a run
    of short segments. Enough that the opening formation reads as a woven
    ball rather than as a handful of hoops. */
const CURVES = 120;

/**
 * Every formation, and nothing else — shared by the one vertex shader.
 *
 * `aCurve` is which of the CURVES this vertex belongs to (0..1) and `aT` is
 * how far along that curve it sits (0..1). A LineSegments geometry pairs
 * consecutive vertices, so the two ends of one segment differ only in `aT` —
 * which means a formation only has to be a function of those two numbers to
 * come out as a continuous, smoothly morphing line.
 */
const FORMATIONS = /* glsl */ `
  const float TAU = 6.28318530718;

  /*  ORBITS. Great circles at scattered orientations: a woven ball of light
      with a bright core, which is what the page opens on. */
  vec3 formOrbits(float c, float t, vec3 s) {
    float a = t * TAU;

    vec3 ring = vec3(cos(a), sin(a), 0.0) * (5.4 + s.x * 2.6);

    float tilt = c * 6.2831853 + s.y * 0.7;
    float spin = c * 7.0 + s.z * 1.3;

    // Written out rather than built as matrices: two rotations is six
    // multiplies, a matrix is nine.
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

  /*  THE OSCILLATOR'S OWN SURFACE, drawn as its rows — which is exactly how
      the plugin's wavetable display draws it, and what a wavetable is: a
      harmonic series, one line per frame, receding. */
  vec3 formWave(float c, float t, vec3 s) {
    float x = (t - 0.5) * 24.0;
    float z = (c - 0.5) * 15.0;

    float pos = 0.5 + 0.5 * sin(uTime * 0.22);
    float y =
        sin(x * 0.55 + uTime * 0.6) * 1.7
      + sin(x * 1.30 - uTime * 0.35) * 0.85 * pos
      + sin(x * 2.10 + uTime * 0.80) * 0.40 * pos * pos;

    // The far rows flatten, so the stack reads as frames going away.
    y *= 1.0 - c * 0.45;

    return vec3(x, y, z);
  }

  /*  MODULATION. A double helix: a mod slot is a source and a destination
      travelling together, so the strands are paired. */
  vec3 formHelix(float c, float t, vec3 s) {
    float strand = step(0.5, fract(c * 8.0));
    float a = t * TAU * 2.2 + strand * 3.14159265 + c * 0.7;
    float r = 4.0 + s.x * 0.7;

    return vec3(cos(a) * r, (t - 0.5) * 22.0, sin(a) * r);
  }

  /*  THE RACK. A vortex wound into a torus, close enough by then that the
      camera is nearly inside it. */
  vec3 formVortex(float c, float t, vec3 s) {
    float a = t * TAU + c * TAU;
    float b = t * TAU * 3.0 + c * 12.0;

    /*  Smaller than it wants to be. At R=8 the vortex is twenty-one units
        across, and at the camera distance it reaches by then it sprawled
        under the text card and the copy had to be read through a wall of
        moving lines. The figure has to fit BESIDE the words, not behind
        them - the same rule the corridor version learned twice. */
    float R = 5.6;
    float r = 1.7 + s.x * 0.7;

    return vec3(
      (R + cos(b) * r) * cos(a),
      sin(b) * r,
      (R + cos(b) * r) * sin(a)
    );
  }

  /*  THE INSTRUMENT. The interface's own rectangle, ruled as a grid, at its
      real aspect ratio — the screenshot then fades up inside it. */
  vec3 formPanel(float c, float t, vec3 s) {
    /*  Narrower than the interface is wide. At 19 the rectangle spans the
        frame and the screenshot that fades up inside it was drawn straight
        across the download card - the copy had knobs behind every line.
        The figure sits BESIDE the words; that is the whole composition. */
    float W = 12.5;
    float H = ${(12.5 / PANEL_ASPECT).toFixed(4)};

    // Half the curves run across and half run down, so it rules itself.
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
    if (id == 3) return formVortex(c, t, s);
    return formPanel(c, t, s);
  }
`;

export interface UniverseOptions {
  /*  WHICH SLICE OF THE UNIVERSE THIS PAGE WALKS. All five formations are
      compiled into the one shader on every page; a page differs only by the
      stretch of them its scroll moves through. So the site is one continuous
      transformation cut into five, and arriving on ENGINE picks up exactly
      where HOME left off instead of resetting to something unrelated. */
  readonly from: number;
  readonly to: number;
}

export function createUniverse(
  canvas: HTMLCanvasElement,
  options: UniverseOptions,
): Universe {
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  let mobile = window.innerWidth < 860;

  const renderer = new THREE.WebGLRenderer({
    canvas,
    antialias: true,
    powerPreference: 'high-performance',
  });

  renderer.setPixelRatio(Math.min(window.devicePixelRatio, mobile ? 1.5 : 2));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.0;

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x03030a);

  const camera = new THREE.PerspectiveCamera(50, 1, 0.1, 600);

  const disposables: Array<{ dispose(): void }> = [];

  /*  The sky is shared with every other page (starfield.ts). It is the one
      thing all five have in common, which is what makes them read as five
      views of one place rather than five pages with the same stylesheet. */
  const stars: Starfield = createStarfield(scene, renderer, mobile);

  // --- the figure ---------------------------------------------------------
  /*  Segments per curve. Enough that a great circle is smooth at the size it
      is drawn; past that you are paying for vertices inside one pixel. */
  const SEGMENTS = mobile ? 34 : 56;
  const VERTS = CURVES * SEGMENTS * 2;

  const figureGeometry = new THREE.BufferGeometry();

  const aCurve = new Float32Array(VERTS);
  const aT = new Float32Array(VERTS);
  const aSeed = new Float32Array(VERTS * 3);

  let v = 0;

  for (let c = 0; c < CURVES; c += 1) {
    const curve = c / (CURVES - 1);

    /*  ONE SEED PER CURVE, not per vertex. A curve whose vertices each had
        their own randomness would be a scribble rather than a line - the
        seed is used to place and orient the whole stroke, so every vertex
        on it has to agree about what it is. */
    const s0 = Math.random();
    const s1 = Math.random();
    const s2 = Math.random();

    for (let seg = 0; seg < SEGMENTS; seg += 1) {
      const t0 = seg / SEGMENTS;
      const t1 = (seg + 1) / SEGMENTS;

      for (const t of [t0, t1]) {
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

  const uniforms = {
    uForm: { value: 0 },
    uTime: { value: 0 },
    uAccent: { value: new THREE.Color(0x7de6ff) },
    uAccent2: { value: new THREE.Color(0xb98cff) },
  };

  const figureMaterial = new THREE.ShaderMaterial({
    uniforms,
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

      varying float vGlow;
      varying float vDepth;

      ${FORMATIONS}

      void main() {
        float f = clamp(uForm, 0.0, ${(FORMS - 1).toFixed(1)});
        int   a = int(floor(f));
        int   b = int(min(floor(f) + 1.0, ${(FORMS - 1).toFixed(1)}));
        float t = fract(f);

        /*  Staggered by curve, and eased. Blending every vertex at once
            turns the figure into a smear for the middle of the transition;
            staggering means curves leave and arrive at different moments,
            so the thing reorganises itself rather than being squashed. */
        float stagger = aSeed.x * 0.35;
        float te = smoothstep(0.0, 1.0, clamp((t - stagger) / 0.65, 0.0, 1.0));

        vec3 pa = formation(a, aCurve, aT, aSeed);
        vec3 pb = formation(b, aCurve, aT, aSeed);
        vec3 p  = mix(pa, pb, te);

        vec4 mv = modelViewMatrix * vec4(p, 1.0);

        // Curves in flight burn brighter: the transition is the moment worth
        // looking at, so it is the moment that lights up.
        vGlow  = 1.0 - abs(te * 2.0 - 1.0);
        vDepth = clamp((length(p) - 2.0) / 12.0, 0.0, 1.0);

        gl_Position = projectionMatrix * mv;
      }
    `,
    fragmentShader: /* glsl */ `
      precision highp float;

      uniform vec3 uAccent;
      uniform vec3 uAccent2;

      varying float vGlow;
      varying float vDepth;

      void main() {
        /*  Cyan at the core, violet at the edges — the plugin's own two
            accents, in the roles they hold there: cyan is what is live. */
        vec3 colour = mix(uAccent, uAccent2, vDepth);
        colour += vGlow * 0.7;

        gl_FragColor = vec4(colour, 0.26 + vGlow * 0.34);
      }
    `,
  });

  disposables.push(figureMaterial);

  const figure = new THREE.LineSegments(figureGeometry, figureMaterial);
  figure.frustumCulled = false;
  scene.add(figure);

  /*  THE CORE. One bright sprite at the centre, which is what makes the
      opening formation read as a star with orbits around it rather than as
      a wireframe ball. It fades as the figure stops being a sphere. */
  const coreMaterial = new THREE.SpriteMaterial({
    map: createGlowTexture(),
    color: 0xbfe9ff,
    transparent: true,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
  });
  disposables.push(coreMaterial);

  const core = new THREE.Sprite(coreMaterial);
  core.scale.setScalar(9);
  scene.add(core);

  // --- the instrument, resolving out of the last formation ----------------
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
    new THREE.PlaneGeometry(12.5, 12.5 / PANEL_ASPECT),
    panelMaterial,
  );
  disposables.push(panel.geometry);
  scene.add(panel);

  // --- post ---------------------------------------------------------------
  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));

  /*  The threshold is the important number, not the strength. Low enough
      that the thin lines cross it — they are the thing that is supposed to
      glow — and not so low that the faint half of the starfield blooms too,
      which turns the sky into milk. */
  const bloom = new UnrealBloomPass(
    new THREE.Vector2(1, 1),
    mobile ? 0.62 : 0.88,
    0.62,
    0.22,
  );
  composer.addPass(bloom);
  composer.addPass(new OutputPass());

  // --- state --------------------------------------------------------------
  const pointer = new THREE.Vector2(0.5, 0.5);
  const pointerEase = new THREE.Vector2(0.5, 0.5);
  let scroll = 0;
  let scrollEase = 0;

  const lookTarget = new THREE.Vector3();
  const forward = new THREE.Vector3();
  const right = new THREE.Vector3();

  function resize(): void {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;

    mobile = window.innerWidth < 860;

    renderer.setSize(width, height, false);
    composer.setSize(width, height);
    bloom.setSize(width, height);

    stars.resize(renderer);

    camera.aspect = width / height;
    camera.fov = mobile ? 64 : 50;
    camera.updateProjectionMatrix();
  }

  return {
    setScroll(progress: number) {
      scroll = progress;
    },

    setPointer(x: number, y: number) {
      pointer.set(x, y);
    },

    resize,

    render(elapsed: number, delta: number) {
      uniforms.uTime.value = elapsed;

      /*  DAMPING IS A FUNCTION OF TIME, NOT OF FRAMES.
          This is the bug that made the previous version teleport, and it is
          the third time this project has hit the same shape of mistake — the
          meter that read the block size, the decode that counted frames, and
          this.

          `x += (target - x) * 0.075` advances 7.5% PER FRAME. At 60 fps that
          settles in about half a second. At 15 fps — which heavy bloom on a
          real machine will hand you — it settles in two seconds AND each
          frame jumps 7.5% of whatever distance a fast flick just opened up.
          You see four or five discrete positions and the page appears to cut
          between them. The motion was a function of the frame rate rather
          than of the clock.

          1 - exp(-rate * dt) is the same exponential sampled correctly: the
          same half-life at any frame rate, and it composes exactly, which is
          the identical argument the plugin's meter ballistics rest on. */
      const ease = (rate: number) => 1 - Math.exp(-rate * Math.min(delta, 0.1));

      if (reduced) {
        scrollEase = scroll;
        pointerEase.copy(pointer);
      } else {
        scrollEase += (scroll - scrollEase) * ease(4.0);
        pointerEase.lerp(pointer, ease(2.5));
      }

      /*  THE MOVE FINISHES BEFORE THE PAGE DOES. Mapped straight from
          progress, the formation is still arriving at the top of the last
          section - so the part you are meant to READ is the part showing a
          half-finished morph. It completes at 0.78 and holds. Scroll
          progress reaches 1 only at the very bottom of the document, which
          is never where the last thing worth looking at is. */
      const walked = Math.min(1, scrollEase / 0.78);
      uniforms.uForm.value = THREE.MathUtils.lerp(options.from, options.to, walked);

      /*  THE CAMERA ORBITS AND ZOOMS, continuously, and never arrives
          anywhere. With no destinations there is no distance to cover in a
          hurry and nothing to reach late.

          It also cannot lose the subject off the side of a narrow frame,
          which is the entire problem the previous corridor needed two failed
          attempts and a head-turning camera to solve. */
      const angle = scrollEase * 2.2 + (pointerEase.x - 0.5) * 0.45;
      const radius = THREE.MathUtils.lerp(30, 20, scrollEase);
      const height = THREE.MathUtils.lerp(4.5, 1.2, scrollEase)
        + (pointerEase.y - 0.5) * -3.5;

      camera.position.set(
        Math.sin(angle) * radius,
        height,
        Math.cos(angle) * radius,
      );

      /*  THE FIGURE IS HELD A FIXED FRACTION OF THE FRAME TO THE RIGHT,
          because the cards sit on the left — the one composition rule this
          project keeps relearning.

          The offset is derived from the FRUSTUM, not set as a world
          distance. The camera dollies from 30 units out to 20, so a fixed
          offset is a different fraction of the screen at each end of the
          scroll: tuned to clear the card at the hero it let the resolved
          interface slide back over the download copy, which is exactly what
          it did. A fraction cannot drift, because it is measured against
          the thing it has to clear. */
      const visibleWidth =
        2 * radius * Math.tan((camera.fov * Math.PI) / 360) * camera.aspect;

      lookTarget.set(0, mobile ? 3.2 : 0.2, 0);

      if (!mobile) {
        /*  SHIFTED ALONG THE CAMERA'S OWN RIGHT, not along world x.
            This one hid for two attempts. The camera ORBITS, so by the
            download section it is about a hundred degrees round the arc and
            looking roughly down the world's x axis - at which point moving
            the look target in world x is moving it towards or away from the
            camera, not sideways. The figure obediently stayed in the middle
            of the frame and sat on the copy, and every fix that treated it
            as "not far enough" made it worse in a different place.

            Screen-space right is what the composition is actually about,
            so that is what the offset is measured along. */
        forward.subVectors(lookTarget, camera.position).normalize();
        right.crossVectors(forward, camera.up).normalize();
        lookTarget.addScaledVector(right, -visibleWidth * 0.26);
      }

      camera.lookAt(lookTarget);

      // The core belongs to the opening formation and fades with it.
      const starness = 1 - THREE.MathUtils.smoothstep(scrollEase, 0.0, 0.3);
      /*  SMALL AND TIGHT. At twice this size the core is a headlight that
          swallows the middle of the weave, and the orbits stop reading as
          orbits because you cannot see where they cross. It is meant to be
          the thing the lines are drawn AROUND, not the subject. */
      coreMaterial.opacity = 0.10 + starness * 0.52;
      core.scale.setScalar(3.0 + starness * 2.8);

      // The sky drifts, so a page nobody is touching still moves.
      stars.update(elapsed);

      /*  The interface fades up inside the last formation, so the universe
          RESOLVES into the product rather than cutting to it. It faces the
          camera, because the ruled rectangle making its shape does too. */
      /*  Only the page that actually ENDS on the interface resolves it. On
          the others uForm never reaches the last formation, so fading a
          screenshot up would be fading it up over a helix. */
      const resolve = options.to >= FORMS - 1
        ? THREE.MathUtils.smoothstep(scrollEase, 0.68, 0.80)
        : 0;
      panelMaterial.opacity = resolve * 0.95;
      panel.visible = resolve > 0.001;
      panel.quaternion.copy(camera.quaternion);

      composer.render();
    },

    dispose() {
      for (const item of disposables) item.dispose();
      stars.dispose();
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
