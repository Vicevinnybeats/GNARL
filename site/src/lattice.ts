import * as THREE from 'three';

/**
 * The floor: a lattice that lights up in squares under your cursor.
 *
 * ONE MESH, NOT A GRID OF THEM. The hall's floor is 400 units across at one
 * cell per unit — 160,000 cells. As geometry that is absurd; it is a single
 * plane whose fragment shader knows where the pointer is standing and
 * brightens the cell under it and its neighbours, falling off with distance.
 *
 * THE GRID IS IN WORLD UNITS, NOT UV. An earlier version of this shader ran
 * on a screen-filling plane and divided its UVs into cells, which is correct
 * for a backdrop and wrong for a floor: UV cells on a plane you are flying
 * over stretch to the horizon, so the squares nearest the camera would be
 * enormous and the far ones sub-pixel. Cells are a fixed size in the world,
 * and perspective does the rest — which is also what makes the floor read as
 * a surface with a scale rather than as wallpaper.
 *
 * THE LIT CELL IS FOUND BY RAYCAST, not by screen position. On a flat
 * backdrop the pointer's 0..1 coordinates ARE the surface coordinates; on a
 * floor seen in perspective they are not, and using them would light a cell
 * that drifts away from the cursor as the camera moves. The caller
 * intersects the pointer ray with the floor plane and hands us the world
 * point.
 */

/** Cell size in world units. One monolith is ~7.6 wide, so ~7 cells. */
const CELL = 1.1;

export function createFloorGrid(): THREE.Mesh {
  const uniforms = {
    /** Where the pointer ray hits the floor, in world XZ. */
    uPointer: { value: new THREE.Vector2(-9999, -9999) },
    uTime: { value: 0 },
  };

  const material = new THREE.ShaderMaterial({
    uniforms,
    transparent: true,
    depthWrite: false,
    // Additive: the floor is light ON a dark reflective surface, never paint
    // over it. Anything else would flatten the reflection underneath.
    blending: THREE.AdditiveBlending,
    vertexShader: /* glsl */ `
      varying vec2  vWorld;
      varying float vDepth;

      void main() {
        vec4 world = modelMatrix * vec4(position, 1.0);
        vWorld = world.xz;

        vec4 mv = viewMatrix * world;
        vDepth = -mv.z;

        gl_Position = projectionMatrix * mv;
      }
    `,
    fragmentShader: /* glsl */ `
      precision highp float;

      uniform vec2  uPointer;
      uniform float uTime;

      varying vec2  vWorld;
      varying float vDepth;

      const float CELL = ${CELL.toFixed(4)};

      void main() {
        vec2 cellUv = vWorld / CELL;
        vec2 cell   = floor(cellUv);
        vec2 within = fract(cellUv);

        // The hairlines. fwidth() keeps them about a pixel wide at any
        // distance rather than aliasing into crawling dashes near the
        // horizon - which on a floor you fly over is most of the frame.
        vec2 grid  = abs(within - 0.5);
        vec2 width = fwidth(cellUv);
        vec2 line  = smoothstep(vec2(0.5), vec2(0.5) - width * 1.5, grid);
        float lattice = max(line.x, line.y);

        // Past a few pixels per cell the grid is finer than the screen and
        // every line aliases. Fade the pattern out where that happens rather
        // than letting it shimmer: the horizon should be fog, not noise.
        float density = max(width.x, width.y);
        lattice *= 1.0 - smoothstep(0.35, 0.9, density);

        // Which cell the pointer is standing in, and how far this one is.
        vec2  pointerCell = floor(uPointer / CELL);
        float distance    = length(cell - pointerCell);

        // Squares light, not a soft radial blob: the falloff is quantised to
        // whole cells, so what brightens is always a block of squares.
        float lit = 1.0 - smoothstep(0.0, 5.0, distance);
        lit = pow(lit, 1.6);

        // A slow drift, so a page nobody is touching is not a still image.
        float breath = 0.5 + 0.5 * sin(uTime * 0.4 + cell.x * 0.15 + cell.y * 0.21);

        vec3 base = vec3(0.30, 0.24, 0.56);
        vec3 hot  = vec3(0.45, 0.92, 1.00);

        float alpha = lattice * (0.20 + 0.09 * breath + 1.10 * lit);

        // The fill inside a lit cell, so it reads as a square lighting UP
        // rather than only its border being traced.
        alpha += lit * 0.10;

        // Into the fog. Matched to the scene's FogExp2 by eye rather than by
        // formula - this is additive and the fog is not, so the two do not
        // compose the same way and the grid has to die slightly sooner or it
        // floats on top of the murk.
        alpha *= exp(-vDepth * 0.016);

        // And a near fade, or the cells under the camera are a bright smear.
        alpha *= smoothstep(0.0, 6.0, vDepth);

        gl_FragColor = vec4(mix(base, hot, lit), alpha);
      }
    `,
  });

  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(420, 420), material);
  mesh.frustumCulled = false;

  return mesh;
}
