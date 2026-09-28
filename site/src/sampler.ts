import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';

/**
 * Turning a model into particles.
 *
 * SAMPLE THE SURFACE, DO NOT USE THE VERTICES. The obvious approach — take
 * the mesh's own vertex positions as the point cloud — gives a cloud whose
 * density is a map of how the modeller happened to subdivide: dense along
 * the detailed parts, empty across a large flat face that is two triangles.
 * A head comes out as a cluster of eyelashes floating beside nothing.
 *
 * So points are scattered across the TRIANGLES, each triangle picked with
 * probability proportional to its AREA, and placed uniformly inside it. The
 * result has even density over the surface whatever the topology, which is
 * the only thing that reads as an object made of light.
 *
 * Picking by area needs a cumulative table and a binary search rather than a
 * loop per point: a hundred thousand points across a hundred thousand
 * triangles is ten billion comparisons done the naive way, which freezes the
 * tab. With the table it is a hundred thousand lookups of twenty steps.
 *
 * THE UNIFORM POINT INSIDE A TRIANGLE is not `a*r1 + b*r2 + c*r3`
 * normalised — that bunches towards the centroid. The square root below is
 * what makes it uniform, and it is the one line here that is easy to get
 * subtly, invisibly wrong.
 */

export interface SampledSurface {
  /** xyz per point, already centred and scaled to `size`. */
  readonly positions: Float32Array;
  /** Unit surface normal per point, for effects that push along it. */
  readonly normals: Float32Array;
  readonly count: number;
}

export interface SampleOptions {
  /** How many points to scatter. */
  readonly count: number;
  /** Longest dimension of the result, in world units. */
  readonly size?: number;
}

/** Scatters points over every triangle of a geometry, weighted by area. */
export function sampleGeometry(
  source: THREE.BufferGeometry,
  options: SampleOptions,
): SampledSurface {
  const { count, size = 10 } = options;

  // Indexed or not, work from a non-indexed copy so a triangle is always
  // three consecutive vertices and there is one code path rather than two.
  const geometry = source.index ? source.toNonIndexed() : source.clone();
  geometry.computeVertexNormals();

  const position = geometry.getAttribute('position');
  const normal = geometry.getAttribute('normal');
  const triangles = Math.floor(position.count / 3);

  // Cumulative area, so a triangle's share of the samples is its share of
  // the surface.
  const cumulative = new Float64Array(triangles);

  const a = new THREE.Vector3();
  const b = new THREE.Vector3();
  const c = new THREE.Vector3();
  const ab = new THREE.Vector3();
  const ac = new THREE.Vector3();
  const cross = new THREE.Vector3();

  let total = 0;

  for (let t = 0; t < triangles; t += 1) {
    a.fromBufferAttribute(position, t * 3);
    b.fromBufferAttribute(position, t * 3 + 1);
    c.fromBufferAttribute(position, t * 3 + 2);

    ab.subVectors(b, a);
    ac.subVectors(c, a);
    total += cross.crossVectors(ab, ac).length() * 0.5;

    cumulative[t] = total;
  }

  const positions = new Float32Array(count * 3);
  const normals = new Float32Array(count * 3);

  const na = new THREE.Vector3();
  const nb = new THREE.Vector3();
  const nc = new THREE.Vector3();
  const point = new THREE.Vector3();
  const pointNormal = new THREE.Vector3();

  const box = new THREE.Box3();

  for (let i = 0; i < count; i += 1) {
    const target = Math.random() * total;

    // Binary search for the first cumulative entry at or past the target.
    let lo = 0;
    let hi = triangles - 1;

    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if ((cumulative[mid] ?? 0) < target) lo = mid + 1;
      else hi = mid;
    }

    a.fromBufferAttribute(position, lo * 3);
    b.fromBufferAttribute(position, lo * 3 + 1);
    c.fromBufferAttribute(position, lo * 3 + 2);

    na.fromBufferAttribute(normal, lo * 3);
    nb.fromBufferAttribute(normal, lo * 3 + 1);
    nc.fromBufferAttribute(normal, lo * 3 + 2);

    /*  Uniform barycentric coordinates. The sqrt is the whole trick: without
        it the points crowd towards the centroid of every triangle, which on
        a low-poly model shows up as a visible dot at the middle of each
        face. */
    const r1 = Math.sqrt(Math.random());
    const r2 = Math.random();

    const wa = 1 - r1;
    const wb = r1 * (1 - r2);
    const wc = r1 * r2;

    point.set(0, 0, 0)
      .addScaledVector(a, wa)
      .addScaledVector(b, wb)
      .addScaledVector(c, wc);

    pointNormal.set(0, 0, 0)
      .addScaledVector(na, wa)
      .addScaledVector(nb, wb)
      .addScaledVector(nc, wc)
      .normalize();

    positions[i * 3] = point.x;
    positions[i * 3 + 1] = point.y;
    positions[i * 3 + 2] = point.z;

    normals[i * 3] = pointNormal.x;
    normals[i * 3 + 1] = pointNormal.y;
    normals[i * 3 + 2] = pointNormal.z;

    box.expandByPoint(point);
  }

  geometry.dispose();

  /*  Centred and scaled from the SAMPLED points rather than from the
      geometry's own bounding box. They are nearly the same thing, but a
      model with a stray far-off vertex that no triangle uses would scale the
      whole cloud down to nothing, and that vertex never gets sampled. */
  const centre = box.getCenter(new THREE.Vector3());
  const span = box.getSize(new THREE.Vector3());
  const scale = size / Math.max(span.x, span.y, span.z, 1e-6);

  for (let i = 0; i < count; i += 1) {
    positions[i * 3] = (positions[i * 3]! - centre.x) * scale;
    positions[i * 3 + 1] = (positions[i * 3 + 1]! - centre.y) * scale;
    positions[i * 3 + 2] = (positions[i * 3 + 2]! - centre.z) * scale;
  }

  return { positions, normals, count };
}

/**
 * Loads a .glb/.gltf and samples every mesh in it as one cloud.
 *
 * Returns null rather than throwing when the file is missing, because the
 * model is OPTIONAL: the pages that use this have a procedural figure they
 * fall back to, and a marketing page that renders nothing because an asset
 * 404'd is worse than one that renders the shape it can build itself. Same
 * reasoning as the plugin's backdrop artwork slot.
 */
export async function sampleModel(
  url: string,
  options: SampleOptions,
): Promise<SampledSurface | null> {
  try {
    const loader = new GLTFLoader();
    const gltf = await loader.loadAsync(url);

    const geometries: THREE.BufferGeometry[] = [];

    gltf.scene.updateMatrixWorld(true);

    gltf.scene.traverse((object) => {
      if (!(object instanceof THREE.Mesh)) return;

      // Baked into world space, or a model whose parts are positioned by
      // their node transforms collapses into a heap at the origin.
      const geometry = (object.geometry as THREE.BufferGeometry).clone();
      geometry.applyMatrix4(object.matrixWorld);

      // Only position and normal survive the merge; anything else differs
      // between meshes and would make the merge fail.
      for (const name of Object.keys(geometry.attributes)) {
        if (name !== 'position' && name !== 'normal') geometry.deleteAttribute(name);
      }

      geometries.push(geometry.index ? geometry.toNonIndexed() : geometry);
    });

    if (geometries.length === 0) return null;

    const merged = mergeGeometries(geometries);
    const sampled = sampleGeometry(merged, options);

    merged.dispose();
    for (const geometry of geometries) geometry.dispose();

    return sampled;
  } catch {
    return null;
  }
}

/** Concatenates position/normal buffers. Enough for what the sampler needs. */
function mergeGeometries(list: THREE.BufferGeometry[]): THREE.BufferGeometry {
  let vertices = 0;

  for (const geometry of list) {
    vertices += geometry.getAttribute('position').count;
  }

  const position = new Float32Array(vertices * 3);
  const normal = new Float32Array(vertices * 3);

  let offset = 0;

  for (const geometry of list) {
    const p = geometry.getAttribute('position');
    const n = geometry.getAttribute('normal');

    for (let i = 0; i < p.count; i += 1) {
      position[(offset + i) * 3] = p.getX(i);
      position[(offset + i) * 3 + 1] = p.getY(i);
      position[(offset + i) * 3 + 2] = p.getZ(i);

      if (n) {
        normal[(offset + i) * 3] = n.getX(i);
        normal[(offset + i) * 3 + 1] = n.getY(i);
        normal[(offset + i) * 3 + 2] = n.getZ(i);
      }
    }

    offset += p.count;
  }

  const merged = new THREE.BufferGeometry();
  merged.setAttribute('position', new THREE.BufferAttribute(position, 3));
  merged.setAttribute('normal', new THREE.BufferAttribute(normal, 3));

  return merged;
}

/**
 * The fallback figure: a loudspeaker driver, generated rather than fetched.
 *
 * ON THEME AND WITHOUT A LICENCE QUESTION, which is not a small thing for a
 * page that advertises something we sell — CLAUDE.md section 9 is explicit
 * that we ship only what we made or hold a licence for, and that applies to
 * a downloaded mesh exactly as much as to a wavetable. A driver is also a
 * shape this genre already owns.
 *
 * A lathe, because a speaker IS a revolved profile: dust cap, cone, roll
 * surround, rim. Modelling it any other way would be modelling a drawing of
 * it.
 */
export function createDriverGeometry(): THREE.BufferGeometry {
  const profile: THREE.Vector2[] = [
    new THREE.Vector2(0.00, 0.62),   // centre of the dust cap
    new THREE.Vector2(0.18, 0.58),
    new THREE.Vector2(0.30, 0.44),   // cap meets cone
    new THREE.Vector2(0.34, 0.40),
    new THREE.Vector2(1.45, -0.34),  // the cone itself
    new THREE.Vector2(1.60, -0.40),
    new THREE.Vector2(1.74, -0.22),  // the roll of the surround
    new THREE.Vector2(1.88, -0.38),
    new THREE.Vector2(2.00, -0.42),  // rim
    new THREE.Vector2(2.06, -0.42),
  ];

  const geometry = new THREE.LatheGeometry(profile, 96);
  geometry.rotateX(-Math.PI / 2);
  geometry.computeVertexNormals();

  return geometry;
}
