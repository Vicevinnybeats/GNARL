import * as THREE from 'three';
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js';
import { Reflector } from 'three/examples/jsm/objects/Reflector.js';

import { createFloorGrid } from './lattice';

/**
 * The world: a corridor you fly down.
 *
 * NOT A PAGE WITH A PICTURE ON IT. The earlier version was a flat document
 * with one textured plane floating over it, and it read as exactly that. This
 * is a place: a near-black hall with a wet lattice floor running to the
 * horizon, three lit monoliths standing in a row down its length, and fog
 * eating everything past them. Scrolling flies the CAMERA through it — you
 * approach the first monolith, pass alongside it, and leave it glowing behind
 * you as the next one resolves out of the fog.
 *
 * The three monoliths carry the plugin's three tabs, so travelling the page
 * walks the instrument. They are the only light sources in the scene, which
 * is what makes the fog and the floor reflection do any work at all: a dark
 * room with a bright object in it has depth for free, and a uniformly lit one
 * has none.
 *
 * WHY THE TEXT IS NOT IN THE WORLD. The obvious move is to project each
 * card's position from a world anchor. Every layout failure in this project
 * has come from something being COMPUTED where it could have been laid out —
 * four panel overflows in the plugin, the panel that outgrew its viewport
 * here — and projected text breaks worst at exactly the window sizes nobody
 * screenshots. So the cards stay in CSS flow and the camera path puts each
 * monolith to the right of centre, where the card lands beside it anyway.
 */

export interface World {
  setScroll(progress: number): void;
  setPointer(x: number, y: number): void;
  resize(): void;
  render(elapsed: number): void;
  dispose(): void;
}

/** The plugin's own aspect ratio, so a screen is never stretched. */
const PANEL_ASPECT = 1180 / 720;

/** How wide a monolith's screen is, in world units. */
const SCREEN_WIDTH = 7.6;
const SCREEN_HEIGHT = SCREEN_WIDTH / PANEL_ASPECT;

/*  Where the three monoliths stand, and which face each shows. Spaced far
    enough apart that one has faded into the fog before the next arrives —
    seeing all three at once would read as a showroom rather than as a hall. */
const MONOLITHS: Array<{ z: number; x: number; texture: string }> = [
  { z: -12, x: 5.4, texture: './ui-osc.webp' },
  { z: -33, x: 6.2, texture: './ui-mod.webp' },
  { z: -54, x: 5.4, texture: './ui-fx.webp' },
];

/*  THOSE Z VALUES ARE NOT SPACING, THEY ARE A SCHEDULE. The page has five
    full-height sections, so the scroll reaches each one at a known fraction
    and the camera is therefore at a known point in the hall: engine at
    about z -12, presets at -33, fx at -54. Standing each monolith at its
    section's number is what makes the FX heading arrive beside the FX tab.

    Spaced by eye instead, they drifted a section out of step - the FX card
    was read against a wall of MOD knobs, which is not wrong so much as
    nonsense, and it looks completely deliberate in a screenshot until you
    read the tab. */

/*  ALL THREE STAND ON THE SAME SIDE, and that is not a rhythm choice. The
    first version alternated them left and right so the hall would not read
    as a rail - and the cards are always on the left, so the middle monolith
    stood exactly where the text is and the FX section rendered its heading
    THROUGH a wall of knobs. Unreadable, and invisible in the diff.

    This is the third time this project has learned that which side a thing
    sits on is decided by where the words are (CLAUDE.md section 6). The
    hall gets its variety from the weave in the camera path and from the
    monoliths standing at different distances instead. */

/*  HOW FAR OFF THE PATH THEY STAND, relative to the numbers above.
    A phone needs them much further out. The lateral offset that frames a
    monolith beautifully at a 52-degree lens puts the camera practically
    against it at 74, and at closest approach it stopped being an instrument
    and became an unreadable slab of pixels filling the screen - measured in
    a screenshot, the whole top of the frame was empty black and the right
    half was one corner of a knob.

    At 11 units the arithmetic works out: half the panel subtends
    atan(3.8/11) = 19 degrees, and half the HORIZONTAL field on a 9:19 phone
    at fov 74 is also about 19. So the panel exactly fills the width at the
    moment you pass it, which is the shot. */
const LATERAL_MOBILE = 6.4 / 5.4;

/*  AND HOW HIGH THEY STAND ON A PHONE. This is the part two attempts got
    backwards. The card owns the bottom two thirds of a 9:19 screen, so the
    instrument has to be in the top third - and a monolith standing on the
    floor is BELOW a camera at eye height, so it renders below the horizon no
    matter where the lens points. Tilting down to "see more" only fills the
    top with void; tilting up slides the world down and hides it behind the
    card. Neither is a lens problem. The monoliths are simply raised on a
    phone, until they are above the eye line and the card has the floor. */
const RAISE_MOBILE = 6.2;

/*  And the first one stands CLOSER on a phone. At the top of the page the
    camera has not moved yet, so whatever the hero shows is fixed geometry -
    and with the desktop spacing the instrument sat 24 units out, small,
    low, and directly behind the card. Pulling the row towards the camera
    puts it above the card at the one scroll position that cannot be
    composed any other way. */
const APPROACH_MOBILE = 0.62;

/*  The camera's journey, in world z. It starts well back so the first
    monolith is small and far, and ends just past the last one - sixteen
    units, not forty. Ending further out spent the whole download section in
    an empty hall with nothing but floor in frame, which is not "everything
    behind you", it is nothing at all. */
const CAMERA_Z_START = 10;
const CAMERA_Z_END = -70;

/** Eye height. Low enough that the floor reflection is a large part of frame. */
const EYE_Y = 1.75;

/*  The floor sits here. The monoliths are raised clear of it so they appear
    to STAND rather than to be sunk into it, and so the reflection has a gap
    under it — a reflection that touches its object reads as a mirror, one
    with air under it reads as a wet floor. */
const FLOOR_Y = -2.4;

export function createWorld(canvas: HTMLCanvasElement): World {
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  /*  THE CINEMATIC PATH IS NOT AFFORDABLE ON A PHONE. A planar reflection
      re-renders the whole scene from a mirrored camera every frame, and bloom
      is three more full-screen passes on top. On a desktop GPU that is
      nothing; on a mid-range phone it is the difference between 60 fps and a
      slideshow, and a stuttering fly-through is worse than a still one. So
      the phone gets the same world with the reflection dropped and the bloom
      at half resolution — same place, cheaper light. */
  let mobile = window.innerWidth < 860;

  const renderer = new THREE.WebGLRenderer({
    canvas,
    antialias: !mobile,
    powerPreference: 'high-performance',
  });

  renderer.setPixelRatio(Math.min(window.devicePixelRatio, mobile ? 1.5 : 2));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.15;

  const scene = new THREE.Scene();

  /*  Exponential fog, not linear. Linear fog has a visible start plane that
      the monoliths cross all at once; exponential just gets thicker, which is
      what a dark hall does. The colour matches the clear colour exactly or
      the horizon shows as a seam. */
  const VOID = new THREE.Color(0x05040f);
  scene.background = VOID;
  scene.fog = new THREE.FogExp2(VOID, 0.019);

  const camera = new THREE.PerspectiveCamera(52, 1, 0.1, 400);

  // --- the floor ---------------------------------------------------------
  /*  Two surfaces, one under the other. The Reflector does the mirror; the
      grid is drawn just above it, additively, so the lattice reads as light
      ON the wet floor rather than as a texture painted into it. One object
      cannot do both: a Reflector owns its material. */
  let reflector: Reflector | null = null;

  if (!mobile) {
    reflector = new Reflector(new THREE.PlaneGeometry(400, 400), {
      // Half resolution. The reflection is seen through fog, at a glancing
      // angle, under a grid - it does not survive inspection and does not
      // need to.
      textureWidth: 1024,
      textureHeight: 1024,
      color: 0x0d0b20,
    });
    reflector.rotation.x = -Math.PI / 2;
    reflector.position.y = FLOOR_Y;
    scene.add(reflector);
  }

  const grid = createFloorGrid();
  grid.rotation.x = -Math.PI / 2;
  grid.position.y = FLOOR_Y + 0.01;
  scene.add(grid);

  const gridUniforms = (grid.material as THREE.ShaderMaterial).uniforms;

  // --- the monoliths -----------------------------------------------------
  const loader = new THREE.TextureLoader();
  const disposables: Array<{ dispose(): void }> = [];

  const screenGeometry = new THREE.PlaneGeometry(SCREEN_WIDTH, SCREEN_HEIGHT);
  const chassisGeometry = new THREE.BoxGeometry(
    SCREEN_WIDTH * 1.06,
    SCREEN_HEIGHT * 1.09,
    0.55,
  );

  /*  The chassis is deliberately almost black and NOT lit by anything. There
      are no lights in this scene at all - every surface is either an emissive
      screen or a dark body catching the bloom from one. Adding a light would
      flatten the very contrast the fog is built on. */
  const chassisMaterial = new THREE.MeshBasicMaterial({ color: 0x090815 });
  disposables.push(chassisGeometry, screenGeometry, chassisMaterial);

  /*  One radial gradient, shared by all three pools. Drawn rather than
      shipped: it is 128 bytes of canvas work against another file to fetch,
      and a gradient is the one image a generator makes better than a file. */
  const spillTexture = (() => {
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
      gradient.addColorStop(0.45, 'rgba(255,255,255,0.28)');
      gradient.addColorStop(1, 'rgba(255,255,255,0)');
      context.fillStyle = gradient;
      context.fillRect(0, 0, size, size);
    }

    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    return texture;
  })();
  disposables.push(spillTexture);

  const spills: THREE.Mesh[] = [];

  const monoliths = MONOLITHS.map((spec) => {
    const texture = loader.load(spec.texture);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.anisotropy = renderer.capabilities.getMaxAnisotropy();
    disposables.push(texture);

    const material = new THREE.MeshBasicMaterial({ map: texture, toneMapped: false });
    disposables.push(material);

    const group = new THREE.Group();

    const chassis = new THREE.Mesh(chassisGeometry, chassisMaterial);
    group.add(chassis);

    const screen = new THREE.Mesh(screenGeometry, material);
    screen.position.z = 0.29;
    group.add(screen);

    /*  The pool of light the screen throws on the floor. The reflection
        alone is too dim to sell the light as REACHING the floor, and a real
        light would need a real lighting model.

        IT IS A SOFT GRADIENT, NOT THE SCREEN'S OWN TEXTURE. The first
        version mapped the UI onto this plane, reasoning that the spill
        should be the colour of what is casting it - and since the plane is
        26 units deep and lying flat, the result was the plugin's interface
        printed LEGIBLY across the floor: you could read "CUTOFF" and
        "Browser preview" in the carpet. Light that falls on a floor is a
        blur, not a slide projection. */
    const spill = new THREE.Mesh(
      new THREE.PlaneGeometry(SCREEN_WIDTH * 2.4, 26),
      new THREE.MeshBasicMaterial({
        map: spillTexture,
        color: 0x5cd8ff,
        transparent: true,
        opacity: 0.34,
        blending: THREE.AdditiveBlending,
        depthWrite: false,
        toneMapped: false,
      }),
    );
    spill.rotation.x = -Math.PI / 2;
    spill.position.set(0, FLOOR_Y + 0.02 - 0, 0);
    disposables.push(spill.geometry, spill.material as THREE.Material);

    group.position.set(spec.x, 0, spec.z);
    // Turned to face the corridor's centre line, so you read it side-on as
    // you pass rather than seeing it edge-on.
    group.rotation.y = -0.42;

    scene.add(group);

    // The spill lies flat on the floor in WORLD space, so it cannot be a
    // child of a rotated, raised group.
    spill.position.set(spec.x, FLOOR_Y + 0.02, spec.z);
    scene.add(spill);
    spills.push(spill);

    return group;
  });

  // --- post ---------------------------------------------------------------
  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));

  /*  Bloom is what makes the screens read as LIGHT rather than as bright
      rectangles. The threshold is high on purpose: only the neon in the UI
      should glow, not the whole panel, or the instrument turns into a
      luminous blob and you cannot read a single control on it. */
  const bloom = new UnrealBloomPass(
    new THREE.Vector2(1, 1),
    mobile ? 0.62 : 0.9,
    0.7,
    0.6,
  );
  composer.addPass(bloom);
  composer.addPass(new OutputPass());

  // --- state --------------------------------------------------------------
  const pointer = new THREE.Vector2(0.5, 0.5);
  const pointerEase = new THREE.Vector2(0.5, 0.5);
  let scroll = 0;
  let scrollEase = 0;

  const lookTarget = new THREE.Vector3();

  /*  For finding the cell the pointer is standing on. The floor is seen in
      perspective, so the pointer's screen position is NOT its position on the
      floor - using it directly lights a cell that slides away from the cursor
      as the camera moves. The ray is intersected with the floor plane
      instead, which is the only answer that stays under the cursor. */
  const raycaster = new THREE.Raycaster();
  const floorPlane = new THREE.Plane(new THREE.Vector3(0, 1, 0), -FLOOR_Y);
  const ndc = new THREE.Vector2();
  const floorHit = new THREE.Vector3();

  function resize(): void {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;

    mobile = window.innerWidth < 860;

    renderer.setSize(width, height, false);
    composer.setSize(width, height);
    bloom.setSize(width, height);

    camera.aspect = width / height;

    /*  A WIDER FIELD OF VIEW ON A PHONE. A 52-degree lens on a 9:19 window
        sees almost nothing to either side, so a monolith standing 5 units
        off the centre line is simply off-screen for the whole approach - the
        world would be a corridor you never see the walls of. */
    camera.fov = mobile ? 74 : 52;
    camera.updateProjectionMatrix();

    /*  Restand them for the new lens. This runs on every resize rather than
        once, because crossing the 860px boundary by dragging a window has to
        rebuild the composition - a desktop layout with phone spacing is the
        same bug seen from the other side. */
    const lateral = mobile ? LATERAL_MOBILE : 1;

    for (let i = 0; i < monoliths.length; i += 1) {
      const spec = MONOLITHS[i]!;
      const z = mobile ? spec.z * APPROACH_MOBILE : spec.z;

      monoliths[i]!.position.x = spec.x * lateral;
      monoliths[i]!.position.y = mobile ? RAISE_MOBILE : 0;
      monoliths[i]!.position.z = z;

      // The pool of light stays on the floor; only the thing casting it
      // moves. It widens a little, because a source further up throws a
      // broader, softer patch.
      spills[i]!.position.x = spec.x * lateral;
      spills[i]!.position.z = z;
      spills[i]!.scale.setScalar(mobile ? 1.35 : 1);
    }
  }

  return {
    setScroll(progress: number) {
      scroll = progress;
    },

    setPointer(x: number, y: number) {
      pointer.set(x, y);
    },

    resize,

    render(elapsed: number) {
      gridUniforms.uTime!.value = elapsed;

      /*  Both eased rather than used raw. The scroll easing is what stops a
          trackpad's stepped deltas showing up as judder in a moving CAMERA,
          where it is far more obvious than it was on a moving panel - a
          stuttering fly-through reads as a broken page. */
      pointerEase.lerp(pointer, reduced ? 1 : 0.06);
      scrollEase += (scroll - scrollEase) * (reduced ? 1 : 0.075);

      /*  THE FLIGHT. Position is a straight run down the corridor; the
          interest is all in where the camera LOOKS. It aims at a point well
          ahead of itself, pulled sideways by the pointer, so the hall swings
          as you move the mouse without the camera ever leaving its track.
          Steering the position instead would let a fast mouse move put the
          camera inside a monolith. */
      const z = THREE.MathUtils.lerp(
        CAMERA_Z_START,
        mobile ? CAMERA_Z_END * APPROACH_MOBILE : CAMERA_Z_END,
        scrollEase,
      );

      /*  A slow lateral weave, so the monoliths pass at different distances
          and the corridor never looks like a straight rail. Small: the
          horizon must not slide. */
      const weave = Math.sin(scrollEase * Math.PI * 2.1) * (mobile ? 0.8 : 1.9);

      camera.position.set(
        weave + (pointerEase.x - 0.5) * 0.9,
        EYE_Y + (pointerEase.y - 0.5) * -0.55,
        z,
      );

      lookTarget.set(
        weave * 0.3 + (pointerEase.x - 0.5) * 5.2,
        /*  Aiming BELOW the monoliths on a phone, which is what puts them
            high in the frame where the card is not. Aiming above them, which
            is the instinctive reading of "look up at the thing", does the
            exact opposite: the camera tilts up, the world slides down, and
            the instrument ends up behind the card with the void above it.
            Same band the flat version reserved in CSS, made with the lens. */
        (mobile ? 3.4 : 0.35) + (pointerEase.y - 0.5) * -2.4,
        z - 22,
      );
      /*  ON A PHONE THE CAMERA TURNS ITS HEAD. Half the horizontal field of
          a 9:19 screen is about 19 degrees, and a monolith standing beside
          the path subtends more than that as you draw level with it - so a
          camera aimed straight down the corridor watches each instrument
          slide out of frame exactly when it is closest, which is the one
          moment it is worth seeing. Two attempts tried to fix that by moving
          the monoliths; the geometry does not allow it, because anything far
          enough to the side to stay in a narrow frame is too far away to
          read.

          So the aim is blended towards whichever monolith is nearest, and
          you pass them the way you would in a corridor: looking at them.
          Desktop keeps the fixed forward aim - at 52 degrees on a 16:10
          frame they are comfortably in shot already, and a camera that turns
          when it does not need to reads as seasickness. */
      if (mobile) {
        let nearest = -1;
        let nearestWeight = 0;

        for (let i = 0; i < MONOLITHS.length; i += 1) {
          const weight = THREE.MathUtils.clamp(
            1 - Math.abs(camera.position.z - monoliths[i]!.position.z) / 30,
            0,
            1,
          );

          if (weight > nearestWeight) {
            nearestWeight = weight;
            nearest = i;
          }
        }

        if (nearest >= 0) {
          const target = monoliths[nearest]!.position;
          // Eased, not snapped: a hard swap as one monolith overtakes
          // another would whip the whole world sideways in a frame.
          const w = nearestWeight * nearestWeight;
          lookTarget.x = THREE.MathUtils.lerp(lookTarget.x, target.x, w);
          lookTarget.y = THREE.MathUtils.lerp(lookTarget.y, target.y, w);
          lookTarget.z = THREE.MathUtils.lerp(lookTarget.z, target.z, w * 0.85);
        }
      }

      camera.lookAt(lookTarget);

      /*  A touch of bank into the weave. A camera that translates sideways
          without rolling reads as a dolly on rails; a few degrees of roll
          reads as flight. */
      camera.rotation.z += Math.cos(scrollEase * Math.PI * 2.1) * 0.035;

      /*  Now that the camera is final for this frame, find where the pointer
          is standing. Order matters: raycasting against last frame's camera
          leaves the lit cell one frame behind a moving view, which on a
          flying camera is a visible lag rather than a subtlety. */
      ndc.set(pointerEase.x * 2 - 1, -(pointerEase.y * 2 - 1));
      raycaster.setFromCamera(ndc, camera);

      if (raycaster.ray.intersectPlane(floorPlane, floorHit)) {
        gridUniforms.uPointer!.value.set(floorHit.x, floorHit.z);
      }

      /*  The monoliths turn slightly towards the camera as it passes, the
          "beat of delay" from the brief - each one notices you. Clamped, so
          the far ones do not spin to face you from across the hall. */
      for (let i = 0; i < monoliths.length; i += 1) {
        const group = monoliths[i]!;
        const spec = MONOLITHS[i]!;
        const near = THREE.MathUtils.clamp(
          1 - Math.abs(camera.position.z - spec.z) / 26,
          0,
          1,
        );

        group.rotation.y = -0.42 + near * 0.3;
      }

      composer.render();
    },

    dispose() {
      for (const item of disposables) item.dispose();
      reflector?.dispose();
      composer.dispose();
      renderer.dispose();
    },
  };
}
