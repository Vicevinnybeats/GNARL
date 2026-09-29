import * as THREE from 'three';

/**
 * The world, continuing, behind the checkout.
 *
 * The landing page trucks its camera sideways before navigating here, so this
 * page has to open somewhere that reads as the SAME PLACE a moment later -
 * otherwise the move that was meant to hide the cut merely delays it.
 *
 * WHAT IT IS NOT is the journey. No formations, no morph, no scroll, no
 * bloom, no figure: everything the landing page renders is there to make
 * somebody want this, and none of it helps somebody who has decided and is
 * being asked for money. A bloomed full-screen composite behind a payment
 * form is fill rate spent on distraction, and on a phone it is spent while
 * somebody types a card number.
 *
 * What survives is the STAR FIELD, drifting laterally - the same shader,
 * the same two colours, the same uneven brightness distribution, because a
 * field that looked different here would say "different place" louder than
 * the camera move said "same world". It is one draw call of points with
 * additive blending and no post-processing.
 *
 * The drift continues the truck's direction, decelerating: the camera
 * arrives rather than stops, which is what makes the two documents feel like
 * one move instead of two.
 */

const STAR_COUNT = 1400;     // a tenth of the journey's field: this is a backdrop
const RADIUS = 120;

export interface CheckoutScene {
  dispose(): void;
}

export function createCheckoutScene(canvas: HTMLCanvasElement): CheckoutScene | null {
  let renderer: THREE.WebGLRenderer;

  try {
    renderer = new THREE.WebGLRenderer({ canvas, antialias: false, alpha: true });
  } catch {
    //  No WebGL. The page is a form and reads fine on the CSS gradient alone.
    return null;
  }

  const mobile = window.innerWidth < 860;

  /*  Capped harder than the journey's 1.25. This is a static backdrop behind
      text; there is nothing here whose edges reward extra pixels. */
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, mobile ? 1 : 1.5));
  renderer.setSize(window.innerWidth, window.innerHeight, false);
  renderer.setClearAlpha(0);

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(58, window.innerWidth / window.innerHeight, 0.1, 400);
  camera.position.set(0, 0, 0);

  const positions = new Float32Array(STAR_COUNT * 3);
  const seeds = new Float32Array(STAR_COUNT * 2);

  for (let i = 0; i < STAR_COUNT; i++) {
    //  Uniform inside a sphere: cbrt on the radius, and acos on the polar
    //  angle. Without both, points bunch at the centre and at the poles.
    const r = RADIUS * Math.cbrt(Math.random());
    const theta = Math.random() * Math.PI * 2;
    const phi = Math.acos(1 - 2 * Math.random());

    positions[i * 3] = r * Math.sin(phi) * Math.cos(theta);
    positions[i * 3 + 1] = r * Math.cos(phi);
    positions[i * 3 + 2] = r * Math.sin(phi) * Math.sin(theta);

    seeds[i * 2] = Math.random();
    seeds[i * 2 + 1] = Math.random();
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
  geometry.setAttribute('aSeed', new THREE.BufferAttribute(seeds, 2));

  const uniforms = {
    uTime: { value: 0 },
    uPixelRatio: { value: renderer.getPixelRatio() },
  };

  //  The journey's star shader, unchanged. Copied rather than imported
  //  because importing it would pull in the whole journey module - 72 KB of
  //  formations and scroll machinery for one material.
  const material = new THREE.ShaderMaterial({
    uniforms,
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

        float b = aSeed.x * aSeed.x * aSeed.x * aSeed.x;
        b *= 0.65 + 0.35 * sin(uTime * 0.6 + aSeed.y * 90.0);

        vBright = b;
        vWarm = aSeed.y;

        gl_PointSize = (0.6 + b * 3.0) * uPixelRatio * (90.0 / max(-mv.z, 1.0));
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

  const points = new THREE.Points(geometry, material);
  points.frustumCulled = false;
  scene.add(points);

  const resize = () => {
    renderer.setSize(window.innerWidth, window.innerHeight, false);
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    uniforms.uPixelRatio.value = renderer.getPixelRatio();
  };

  window.addEventListener('resize', resize);

  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  let raf = 0;
  let elapsed = 0;
  let previous = performance.now();

  /*  ARRIVING, NOT STARTING. The camera carries the truck's remaining speed
      and eases to a crawl, so this opens mid-move. Starting from rest would
      make the landing page's exit a wind-up to nothing. */
  let velocity = reduced ? 0 : 26;
  camera.position.x = -16;

  const frame = (now: number) => {
    const delta = Math.min((now - previous) / 1000, 0.1);
    previous = now;
    elapsed += delta;

    uniforms.uTime.value = elapsed;

    /*  Exponential decay sampled by time, never a per-frame multiply - the
        mistake CLAUDE.md lists five times. This composes exactly, so the
        deceleration is the same shape at 30fps and at 120. */
    velocity *= Math.exp(-1.6 * delta);
    camera.position.x += velocity * delta;

    //  A permanent slow drift, so the page is alive but not distracting.
    if (!reduced) camera.position.x += 0.35 * delta;

    renderer.render(scene, camera);
    raf = requestAnimationFrame(frame);
  };

  //  Paused when the tab is hidden: a backdrop nobody is looking at should
  //  not cost battery.
  const visibility = () => {
    if (document.hidden) {
      cancelAnimationFrame(raf);
    } else {
      previous = performance.now();
      raf = requestAnimationFrame(frame);
    }
  };

  document.addEventListener('visibilitychange', visibility);
  raf = requestAnimationFrame(frame);

  return {
    dispose() {
      cancelAnimationFrame(raf);
      window.removeEventListener('resize', resize);
      document.removeEventListener('visibilitychange', visibility);
      geometry.dispose();
      material.dispose();
      renderer.dispose();
    },
  };
}
