import * as THREE from 'three';

/**
 * The sky, shared by every page.
 *
 * It is the one thing that is the same on all five, which is what makes them
 * feel like five views of one place rather than five pages that happen to
 * share a stylesheet. Extracted the moment a second page wanted it: two
 * copies of a starfield drift apart in density and colour and the continuity
 * quietly goes.
 */

export interface Starfield {
  update(elapsed: number): void;
  resize(renderer: THREE.WebGLRenderer): void;
  dispose(): void;
}

export function createStarfield(
  scene: THREE.Scene,
  renderer: THREE.WebGLRenderer,
  mobile: boolean,
): Starfield {
  const COUNT = mobile ? 3500 : 9000;

  const geometry = new THREE.BufferGeometry();
  const positions = new Float32Array(COUNT * 3);
  const seeds = new Float32Array(COUNT * 2);

  for (let i = 0; i < COUNT; i += 1) {
    // A shell well outside anything the camera does, so the parallax is
    // gentle and nothing can ever fly into it.
    const r = 120 + Math.random() * 90;
    const theta = Math.random() * Math.PI * 2;
    const phi = Math.acos(1 - 2 * Math.random());

    positions[i * 3] = r * Math.sin(phi) * Math.cos(theta);
    positions[i * 3 + 1] = r * Math.cos(phi);
    positions[i * 3 + 2] = r * Math.sin(phi) * Math.sin(theta);

    seeds[i * 2] = Math.random();
    seeds[i * 2 + 1] = Math.random();
  }

  geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
  geometry.setAttribute('aSeed', new THREE.BufferAttribute(seeds, 2));

  const uniforms = {
    uTime: { value: 0 },
    uPixelRatio: { value: renderer.getPixelRatio() },
  };

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

        /*  Unequal, and steeply so. A field of equally bright dots reads as
            noise; a few bright ones among many faint ones reads as a sky.
            The fourth power is what keeps most of them nearly invisible. */
        float b = aSeed.x * aSeed.x * aSeed.x * aSeed.x;

        // A slow twinkle, out of phase per star so they never pulse together.
        b *= 0.65 + 0.35 * sin(uTime * 0.6 + aSeed.y * 90.0);

        vBright = b;
        vWarm = aSeed.y;

        gl_PointSize = (0.9 + b * 3.4) * uPixelRatio;
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

  return {
    update(elapsed: number) {
      uniforms.uTime.value = elapsed;
      points.rotation.y = elapsed * 0.006;
    },

    resize(next: THREE.WebGLRenderer) {
      uniforms.uPixelRatio.value = next.getPixelRatio();
    },

    dispose() {
      scene.remove(points);
      geometry.dispose();
      material.dispose();
    },
  };
}
