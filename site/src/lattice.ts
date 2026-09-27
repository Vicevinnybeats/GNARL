import * as THREE from 'three';

/**
 * The lattice that lights up in squares under the cursor.
 *
 * ONE MESH, NOT A GRID OF THEM. A 40x24 lattice is 960 cells; as individual
 * meshes that is 960 draw calls a frame for a background. It is a single
 * plane whose fragment shader knows where the pointer is and brightens the
 * cell containing it, plus its neighbours, falling off with distance.
 *
 * The lit cell is computed in CELL SPACE, not in pixels: `floor(uv * cells)`
 * gives the cell index, and the distance between cell indices is what drives
 * the glow. Doing it in pixels would make the lit area a different shape on
 * every aspect ratio.
 */
export function createLattice(cells: THREE.Vector2): THREE.Mesh {
  const uniforms = {
    uPointer: { value: new THREE.Vector2(-10, -10) },
    uCells: { value: cells },
    uTime: { value: 0 },
    uReveal: { value: 0 },
  };

  const material = new THREE.ShaderMaterial({
    uniforms,
    transparent: true,
    depthWrite: false,
    vertexShader: /* glsl */ `
      varying vec2 vUv;
      void main() {
        vUv = uv;
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }
    `,
    fragmentShader: /* glsl */ `
      precision highp float;

      uniform vec2  uPointer;   // 0..1, -10 when the pointer has left
      uniform vec2  uCells;
      uniform float uTime;
      uniform float uReveal;

      varying vec2 vUv;

      void main() {
        vec2 cell = floor(vUv * uCells);
        vec2 within = fract(vUv * uCells);

        // The hairlines. fwidth() keeps them one pixel wide at any zoom
        // rather than aliasing into dashes at the horizon.
        vec2 grid = abs(within - 0.5);
        vec2 width = fwidth(vUv * uCells);
        vec2 line = smoothstep(vec2(0.5), vec2(0.5) - width * 1.5, grid);
        float lattice = max(line.x, line.y);

        // Which cell the pointer is in, and how far this one is from it.
        vec2 pointerCell = floor(uPointer * uCells);
        float distance = length(cell - pointerCell);

        // Squares light, not a soft radial blob: the falloff is quantised to
        // whole cells, so what brightens is always a block of squares.
        float lit = 1.0 - smoothstep(0.0, 4.5, distance);
        lit = pow(lit, 1.6);

        // A slow drift so a still page is not a still image.
        float breath = 0.5 + 0.5 * sin(uTime * 0.4 + cell.x * 0.15 + cell.y * 0.21);

        vec3 base = vec3(0.32, 0.26, 0.55);
        vec3 hot  = vec3(0.45, 0.92, 1.0);

        float alpha = lattice * (0.05 + 0.03 * breath + 0.55 * lit);
        vec3 colour = mix(base, hot, lit);

        // The fill inside a lit cell, so it reads as a square lighting up
        // rather than only its border.
        alpha += lit * 0.07;

        gl_FragColor = vec4(colour, alpha * uReveal);
      }
    `,
  });

  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(1, 1), material);
  mesh.renderOrder = -1;
  mesh.frustumCulled = false;

  return mesh;
}
