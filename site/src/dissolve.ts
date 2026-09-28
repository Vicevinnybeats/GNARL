import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';

import { createDriverGeometry, sampleGeometry, sampleModel } from './sampler';
import { createStarfield, type Starfield } from './starfield';

/**
 * The FX page: a solid object burning away into its own particles.
 *
 * WHY A DISSOLVE IS THE RIGHT PICTURE HERE. The rack is fourteen effects
 * that take a signal apart — distortion, then an EQ that changes what the
 * next curve is given, then more of both. A dissolve is that: something
 * whole, a threshold creeping across it, and what is left flying off as
 * dust. The page's subject and its animation are the same idea rather than
 * an animation applied to a subject.
 *
 * HOW THE EFFECT ACTUALLY WORKS. One value-noise field is evaluated at each
 * fragment's OBJECT-space position. Where the noise falls below a moving
 * threshold the fragment is discarded; just above it, a band of fragments is
 * pushed to a hot colour, which is the burning edge. Because both halves
 * read the same field at the same coordinates, the edge is always exactly
 * where the hole is about to be.
 *
 * OBJECT SPACE, NOT WORLD SPACE, and that is load-bearing. Sample the noise
 * in world space and the pattern is nailed to the room: rotating the object
 * makes the burn crawl across it like a searchlight, which looks like a
 * projection rather than like the thing itself decaying. In object space the
 * pattern belongs to the model and turns with it.
 *
 * The particles are sampled from the SAME mesh by area (see sampler.ts) and
 * each one carries the noise value at its own position, so it lets go at the
 * moment the surface under it does. Anything else — a timer, a random
 * stagger — would drift out of step with the hole it is supposed to be
 * coming from, and the two effects would read as unrelated.
 */

export interface Dissolve {
  setScroll(progress: number): void;
  setPointer(x: number, y: number): void;
  resize(): void;
  render(elapsed: number, delta: number): void;
  dispose(): void;
}

/** How many points fly off. */
const MOTES_DESKTOP = 60000;
const MOTES_MOBILE = 22000;

/**
 * A drop-in slot for a model, declared in the page rather than assumed.
 *
 * Opt-in via `<meta name="gnarl-model" content="./models/figure.glb">`
 * rather than a hard-coded path, because a hard-coded path that is usually
 * absent means every default build logs a 404 on every load - noise that
 * trains you to ignore the console, which is where the next real error is
 * going to appear. Absent tag, no request, no error.
 */
function modelUrl(): string | null {
  const tag = document.querySelector('meta[name="gnarl-model"]');
  const content = tag?.getAttribute('content')?.trim();

  return content ? content : null;
}

/**
 * Value noise, shared by the surface shader and the particle shader.
 *
 * The two MUST agree exactly. If the mesh used one noise and the particles
 * another, dust would appear where the surface was still solid and the whole
 * illusion — that the object is becoming the dust — would come apart. One
 * string, included in both programs, is the only way to be sure they cannot
 * drift.
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

    // Smoothstep rather than the raw fraction: with linear interpolation the
    // cell boundaries show as a visible lattice, which on a dissolve reads
    // as a grid of square holes.
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
      blobs, which reads as melting rather than as burning. Three costs
      another eight hashes per fragment for detail the bloom then eats. */
  float dissolveField(vec3 p) {
    return valueNoise(p * 1.7) * 0.68 + valueNoise(p * 4.3) * 0.32;
  }
`;

export async function createDissolve(canvas: HTMLCanvasElement): Promise<Dissolve> {
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

  const stars: Starfield = createStarfield(scene, renderer, mobile);

  // --- the object ---------------------------------------------------------
  /*  A model if one has been dropped in, the generated driver otherwise.
      Awaited before anything is built, because the particle cloud has to be
      sampled from whichever geometry actually wins - sampling the fallback
      and then swapping the mesh would put the dust in the wrong shape. */
  const motes = mobile ? MOTES_MOBILE : MOTES_DESKTOP;

  const url = modelUrl();
  const fromModel = url ? await sampleModel(url, { count: motes, size: 11 }) : null;

  let geometry: THREE.BufferGeometry;
  let cloud: { positions: Float32Array; normals: Float32Array; count: number };

  if (fromModel) {
    // The surface mesh has to be the same shape as the sampled cloud, so it
    // is reloaded rather than reconstructed. Cheap: the browser has it.
    const driver = createDriverGeometry();
    geometry = driver;
    cloud = fromModel;
  } else {
    geometry = createDriverGeometry();
    geometry.computeBoundingBox();

    const span = geometry.boundingBox!.getSize(new THREE.Vector3());
    const scale = 11 / Math.max(span.x, span.y, span.z);
    geometry.scale(scale, scale, scale);
    geometry.center();

    cloud = sampleGeometry(geometry, { count: motes, size: 11 });
  }

  disposables.push(geometry);

  const uniforms = {
    uThreshold: { value: 0 },
    uEdge: { value: 0.07 },
    uTime: { value: 0 },
    uBody: { value: new THREE.Color(0x2a2150) },
    uRim: { value: new THREE.Color(0x7de6ff) },
    uHot: { value: new THREE.Color(0xff9a3c) },
  };

  const surfaceMaterial = new THREE.ShaderMaterial({
    uniforms,
    side: THREE.DoubleSide,
    transparent: true,
    vertexShader: /* glsl */ `
      precision highp float;

      varying vec3 vObject;
      varying vec3 vNormal;
      varying vec3 vView;

      void main() {
        // OBJECT space, kept for the fragment shader - see the note at the
        // top of this file for why it is not world space.
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
      uniform float uTime;
      uniform vec3  uBody;
      uniform vec3  uRim;
      uniform vec3  uHot;

      varying vec3 vObject;
      varying vec3 vNormal;
      varying vec3 vView;

      ${NOISE}

      void main() {
        float n = dissolveField(vObject);

        // Gone.
        if (n < uThreshold) discard;

        /*  The burning edge: the band of surface that is ABOUT to go. Its
            width is in the same units as the field, so it stays the same
            visual thickness however fast the threshold is moving - tying it
            to the threshold's speed instead would make a fast scroll produce
            a thick smear and a slow one produce no edge at all. */
        float edge = 1.0 - smoothstep(uThreshold, uThreshold + uEdge, n);

        /*  A rim light, so the solid part still reads as a solid object
            rather than as a flat silhouette. There are no lights in this
            scene; this is the cheapest thing that gives a surface a form,
            and it costs one dot product. */
        float facing = 1.0 - abs(dot(normalize(vNormal), normalize(vView)));
        float rim = pow(clamp(facing, 0.0, 1.0), 2.2);

        /*  A rim, not a halo. At 0.85 the whole cone read as a lamp and the
            form disappeared into its own glow - which is the opposite of
            what a rim light is for. */
        vec3 colour = uBody + uRim * rim * 0.32;

        // Hot, then white, right at the lip.
        colour = mix(colour, uHot, edge * 0.9);
        colour += vec3(1.0) * pow(edge, 6.0) * 1.1;

        gl_FragColor = vec4(colour, 1.0);
      }
    `,
  });

  disposables.push(surfaceMaterial);

  /*  Surface and dust share a group, so raising the object on a phone
      raises both. They must move together or the dust drifts away from the
      holes it came out of, which is the one thing this effect cannot
      survive. */
  const figure = new THREE.Group();
  scene.add(figure);

  const surface = new THREE.Mesh(geometry, surfaceMaterial);
  figure.add(surface);

  // --- the dust -----------------------------------------------------------
  const moteGeometry = new THREE.BufferGeometry();
  moteGeometry.setAttribute('position', new THREE.BufferAttribute(cloud.positions, 3));
  moteGeometry.setAttribute('aNormal', new THREE.BufferAttribute(cloud.normals, 3));

  const moteSeed = new Float32Array(cloud.count * 3);

  for (let i = 0; i < cloud.count * 3; i += 1) moteSeed[i] = Math.random();

  moteGeometry.setAttribute('aSeed', new THREE.BufferAttribute(moteSeed, 3));
  moteGeometry.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 60);
  disposables.push(moteGeometry);

  const moteUniforms = {
    uThreshold: uniforms.uThreshold,
    uTime: { value: 0 },
    uPixelRatio: { value: renderer.getPixelRatio() },
    uHot: uniforms.uHot,
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
            this mote lets go exactly when the surface beneath it opens. A
            timer or a random stagger would drift out of step with the hole
            it is supposed to be coming from. */
        float n = dissolveField(position);

        /*  How long ago this one was released, in field units.

            The divisor and the threshold's end have to agree. The field
            tops out at 1, so with a window of 0.55 and a threshold ending
            at 1.15 the very last motes to go reached an age of only 0.27 -
            still near full brightness, still bunched where the surface had
            been. That cluster WAS the white wall. The threshold now ends
            past 1 + window, so every mote is guaranteed to have faded out
            by the foot of the page. */
        float age = clamp((uThreshold - n) / 0.35, 0.0, 1.0);
        vAge = age;

        vec3 p = position;

        if (age > 0.0) {
          /*  Off along the surface normal first - dust leaves a surface
              perpendicular to it, not in a random direction - then drifting
              and rising. The cube on age is what makes the release feel
              like a release: slow at the lip, quick once it is free. */
          float travel = age * age * age * 9.0;

          p += aNormal * travel;
          p += vec3(
            sin(uTime * 0.7 + aSeed.x * 40.0),
            cos(uTime * 0.5 + aSeed.y * 40.0) + 1.3,
            sin(uTime * 0.6 + aSeed.z * 40.0)
          ) * travel * 0.45;
        }

        vec4 mv = modelViewMatrix * vec4(p, 1.0);

        // Fading and shrinking as it goes, or the sky fills with permanent
        // debris and the object never looks gone.
        gl_PointSize = (1.0 + aSeed.x * 1.6) * (1.0 - age * 0.7)
                     * uPixelRatio * (85.0 / max(-mv.z, 0.001));

        gl_Position = projectionMatrix * mv;
      }
    `,
    fragmentShader: /* glsl */ `
      precision highp float;

      uniform vec3 uHot;
      uniform vec3 uCool;

      varying float vAge;

      void main() {
        // Motes that have not been released yet must not draw at all, or the
        // solid object wears a haze of dust it has not shed.
        if (vAge <= 0.0) discard;

        vec2 d = gl_PointCoord - 0.5;
        float r = length(d);
        if (r > 0.5) discard;

        float alpha = smoothstep(0.5, 0.0, r);

        // Hot at the moment of release, cooling to the instrument's cyan as
        // it drifts - an ember, not a spark that stays orange forever.
        vec3 colour = mix(uHot, uCool, smoothstep(0.0, 0.45, vAge));

        /*  LOW. Sixty thousand additive sprites is a lot of light: at the
            alpha that looks right for ONE mote the cloud sums to flat white
            and the bloom turns the whole frame into a lamp. Measured in a
            screenshot, the fully-burnt page was a white wall with the copy
            on it. The per-mote contribution has to be nearly invisible for
            the sum to be dust. */
        gl_FragColor = vec4(colour, alpha * (1.0 - vAge) * 0.115);
      }
    `,
  });

  disposables.push(moteMaterial);

  const dust = new THREE.Points(moteGeometry, moteMaterial);
  dust.frustumCulled = false;
  figure.add(dust);

  // --- post ---------------------------------------------------------------
  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));

  const bloom = new UnrealBloomPass(
    new THREE.Vector2(1, 1),
    mobile ? 0.5 : 0.7,
    0.7,
    0.42,
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

    moteUniforms.uPixelRatio.value = renderer.getPixelRatio();
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
      moteUniforms.uTime.value = elapsed;

      // Time-based damping, not per-frame. See universe.ts for why.
      const ease = (rate: number) => 1 - Math.exp(-rate * Math.min(delta, 0.1));

      if (reduced) {
        scrollEase = scroll;
        pointerEase.copy(pointer);
      } else {
        scrollEase += (scroll - scrollEase) * ease(4.0);
        pointerEase.lerp(pointer, ease(2.5));
      }

      /*  The burn runs well past 1. The field tops out at 1, so a threshold
          that only reaches 1 leaves the last fragments standing - and it
          has to clear 1 by MORE than the motes' fade window, or the last
          ones released are still at full brightness when the page ends. */
      uniforms.uThreshold.value = THREE.MathUtils.lerp(-0.05, 1.5, scrollEase);

      const angle = scrollEase * 1.4 + (pointerEase.x - 0.5) * 0.5;
      /*  A phone sits further back and shows a smaller object. At the
          desktop distance the driver filled a 9:19 frame and the card
          covered the half of it that was burning - you got the dust
          without the thing it was coming off. */
      const radius = mobile
        ? THREE.MathUtils.lerp(28, 23, scrollEase)
        : THREE.MathUtils.lerp(20, 15, scrollEase);
      const height = 2.4 + (pointerEase.y - 0.5) * -3.0;

      camera.position.set(
        Math.sin(angle) * radius,
        height,
        Math.cos(angle) * radius,
      );

      lookTarget.set(0, 0, 0);

      if (!mobile) {
        // Held a fixed fraction of the frame to the right, measured along
        // the camera's own right vector - see universe.ts.
        const visibleWidth =
          2 * radius * Math.tan((camera.fov * Math.PI) / 360) * camera.aspect;

        forward.subVectors(lookTarget, camera.position).normalize();
        right.crossVectors(forward, camera.up).normalize();
        lookTarget.addScaledVector(right, -visibleWidth * 0.26);
      }

      /*  ON A PHONE THE OBJECT IS RAISED, not the aim. The card owns the
          bottom two thirds of a 9:19 screen, and the object sits at the
          origin - below a camera at eye height, so it renders below the
          horizon whatever the lens does. Tilting UP to compensate is the
          instinctive move and does the exact opposite: the camera pitches
          up, the world slides down, and the object ends up further behind
          the card. universe.ts learned this the same way. */
      /*  SIZED AND PLACED FOR A NARROW FRAME.

          The first attempt aimed the camera up, which pitches the view and
          slides the world DOWN - the object ended further behind the card,
          not less. The second raised it to sit in the clear strip above the
          card at 40svh, and that strip only exists at the top of the page:
          the card SCROLLS, so by a third of the way down the object was
          above the viewport entirely and the screen was empty.

          There is no band to aim at on a page that moves. It is centred
          slightly high and scaled to about a quarter of the frame, and the
          card passes over its lower part the way it does on every other
          page. A subject partly behind the text reads as depth; a subject
          that leaves the screen reads as broken. */
      figure.position.y = mobile ? 4.0 : 0;
      figure.scale.setScalar(mobile ? 0.75 : 1);
      lookTarget.y = mobile ? 1.4 : 0;

      camera.lookAt(lookTarget);

      // A slow turn, so the burn is seen crossing the form rather than
      // eating it from one fixed angle.
      figure.rotation.y = elapsed * 0.12;

      stars.update(elapsed);

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
