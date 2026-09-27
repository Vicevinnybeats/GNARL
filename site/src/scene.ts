import * as THREE from 'three';

import { createLattice } from './lattice';

/**
 * The 3D layer: a lattice, and the plugin's own interface floating on it.
 *
 * THE CENTREPIECE IS THE PRODUCT, not a stand-in for it. The panel is
 * textured with real screenshots of the plugin at its design size, which
 * means the site cannot drift from what the thing actually looks like — the
 * same reasoning that has the wavetable display draw what the oscillator
 * plays rather than a picture of it.
 *
 * IT TURNS TOWARDS THE CURSOR WITH A BEAT OF DELAY. The rotation eases
 * towards the pointer rather than tracking it, so the panel feels like it
 * has mass. Instant tracking reads as a cursor-locked gimmick; a damped
 * follow reads as an object.
 */

export interface Scene {
  readonly panel: THREE.Group;
  setScroll(progress: number): void;
  setPointer(x: number, y: number): void;
  setTexture(index: number): void;
  resize(): void;
  render(elapsed: number): void;
  dispose(): void;
}

const TEXTURES = ['./ui-osc.webp', './ui-mod.webp', './ui-fx.webp'];

/** The plugin's own aspect ratio, so the panel is never stretched. */
const PANEL_ASPECT = 1180 / 720;

/*  How much of the VIEWPORT'S WIDTH the panel spans, at three points in the
    scroll. Fractions rather than world units, because the same numbers then
    hold on a phone and on a 32-inch monitor - see the note in render(). */
const kSpanHero = 0.60;
const kSpanPeak = 0.94;
const kSpanAside = 0.56;

/*  A phone is the other way round: the panel is 16:10 in a 9:19 window, so
    width is scarce and height is not, and it has no side to park in. It
    therefore starts SMALLER as a fraction (0.60 of 390px is 234px, which is
    a stamp, not an instrument), zooms past the frame at the peak - a crop
    on a phone reads as being inside the thing - and settles wide. */
const kSpanHeroMobile = 0.86;
const kSpanPeakMobile = 1.15;
const kSpanAsideMobile = 0.96;

/*  And a phone's version of "to the side" is UPWARDS. Measured in a
    screenshot, a centred panel behind a full-width card showed as two
    slivers down the edges - the product invisible on the device most people
    will open the page on. It sits in a band across the top instead, which
    the sections' own top padding keeps clear. */
const kMobileBandCentre = 0.26;

/*  The panel never takes more than this fraction of the height. A short,
    wide window runs out of height long before it runs out of width. */
const kMaxHeightSpan = 0.74;

/*  Nothing like binding on a phone, where the panel is a third the height
    of the window; it is here so a landscape phone cannot overflow. */
const kMaxHeightSpanMobile = 0.3;

/** Gap between the parked panel and the right edge, as a fraction. */
const kEdgeMargin = 0.02;

/*  Where the panel's centre sits on the HOMEPAGE, as a fraction of the
    width. Not 0.5: the hero card is on the left, and a panel centred behind
    it puts the product's own controls under the headline. Right of centre
    from the first frame, so the card lands in the space beside it and the
    slide on scroll continues a move that has already started. */
const kHeroCentre = 0.6;

export function createScene(canvas: HTMLCanvasElement): Scene {
  const reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  const renderer = new THREE.WebGLRenderer({
    canvas,
    antialias: true,
    alpha: true,
    powerPreference: 'high-performance',
  });

  /*  Capped at 2. A phone can report 3 or 4, which quadruples the fragment
      work for a difference nobody can see at arm's length - and this scene
      is a background, not the product. */
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));

  const scene = new THREE.Scene();

  const camera = new THREE.PerspectiveCamera(38, 1, 0.1, 100);
  camera.position.set(0, 0, 6);

  // --- the lattice ------------------------------------------------------
  const lattice = createLattice(new THREE.Vector2(40, 24));
  lattice.position.z = -4;
  scene.add(lattice);

  // --- the panel --------------------------------------------------------
  const loader = new THREE.TextureLoader();
  const textures = TEXTURES.map((url) => {
    const texture = loader.load(url);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.anisotropy = renderer.capabilities.getMaxAnisotropy();
    return texture;
  });

  const panelMaterial = new THREE.MeshBasicMaterial({
    map: textures[0],
    transparent: true,
  });

  const panel = new THREE.Group();

  const face = new THREE.Mesh(
    new THREE.PlaneGeometry(PANEL_ASPECT, 1, 1, 1),
    panelMaterial,
  );
  panel.add(face);

  /*  A soft plate behind the panel so it reads as an object with a back
      rather than as a floating decal. Slightly larger and much darker. */
  const plate = new THREE.Mesh(
    new THREE.PlaneGeometry(PANEL_ASPECT * 1.035, 1.06),
    new THREE.MeshBasicMaterial({ color: 0x07060f, transparent: true, opacity: 0.9 }),
  );
  plate.position.z = -0.02;
  panel.add(plate);

  scene.add(panel);

  // --- state ------------------------------------------------------------
  const pointer = new THREE.Vector2(0.5, 0.5);
  const pointerEase = new THREE.Vector2(0.5, 0.5);
  let scroll = 0;
  let scrollEase = 0;
  let mobile = window.innerWidth < 860;

  const latticeUniforms = (lattice.material as THREE.ShaderMaterial).uniforms;

  function resize(): void {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;

    mobile = window.innerWidth < 860;

    renderer.setSize(width, height, false);

    camera.aspect = width / height;
    camera.updateProjectionMatrix();

    // The lattice fills the frustum at its own depth, whatever the aspect.
    const distance = camera.position.z - lattice.position.z;
    const visibleHeight = 2 * Math.tan((camera.fov * Math.PI) / 360) * distance;
    lattice.scale.set(visibleHeight * camera.aspect * 1.05, visibleHeight * 1.05, 1);

    // Fewer, larger cells on a phone: 40 columns across 380 logical pixels
    // is a grey haze rather than a lattice.
    latticeUniforms.uCells!.value.set(mobile ? 14 : 40, mobile ? 26 : 24);
  }

  return {
    panel,

    setScroll(progress: number) {
      scroll = progress;
    },

    setPointer(x: number, y: number) {
      pointer.set(x, y);
      latticeUniforms.uPointer!.value.set(x, 1 - y);
    },

    setTexture(index: number) {
      const texture = textures[Math.max(0, Math.min(textures.length - 1, index))];
      if (texture && panelMaterial.map !== texture) {
        panelMaterial.map = texture;
        panelMaterial.needsUpdate = true;
      }
    },

    resize,

    render(elapsed: number) {
      latticeUniforms.uTime!.value = elapsed;
      latticeUniforms.uReveal!.value = THREE.MathUtils.lerp(
        latticeUniforms.uReveal!.value,
        1,
        0.02,
      );

      /*  THE BEAT OF DELAY. Both the pointer and the scroll are eased
          towards their target rather than used raw. The scroll easing is
          what stops a trackpad's stepped deltas showing up as judder in the
          zoom; the pointer easing is what gives the panel mass. */
      pointerEase.lerp(pointer, reduced ? 1 : 0.06);
      scrollEase += (scroll - scrollEase) * (reduced ? 1 : 0.09);

      /*  THE SCROLL IS A ZOOM, NOT A FALL, and the panel is sized from the
          CAMERA'S FRUSTUM rather than by a scalar.

          The first version multiplied a fixed scale, which meant the same
          number had to work for a 1440x900 desktop and a 390x844 phone. It
          did not: measured in a screenshot, the phone showed the panel
          bleeding off all four edges at scroll zero, and the desktop's FX
          section zoomed so far past the frame that the instrument stopped
          reading as an object and became wallpaper behind the text. Solving
          it with two magic numbers would leave the next viewport wrong
          again.

          So the scroll drives a fraction of the VIEWPORT the panel spans,
          and the world-space scale is derived from that at the panel's
          actual depth. A fraction cannot overflow a screen it is measured
          against. The height is clamped the same way, because a short wide
          window fails on height first and a phone fails on width.

          The curve peaks and comes back: you move INTO the instrument
          leaving the hero, and it settles to a size that FITS BESIDE the
          text rather than under it. A zoom that only increases has nowhere
          to put the words. */
      /*  The move FINISHES before the page does. Progress runs to 1 only at
          the very bottom of the document, so tying the parked state to it
          left the panel still half-zoomed over the last two sections - in a
          screenshot of the download section it was bleeding off the right
          edge with the FAQ card on top of it. It reaches its parked size and
          position at `settled` and holds, which also gives the part of the
          page that is actually being READ a composition that stops moving. */
      const peak = 0.34;
      const settled = 0.62;
      const park = Math.min(1, scrollEase / settled);
      const hero = mobile ? kSpanHeroMobile : kSpanHero;
      const wide = mobile ? kSpanPeakMobile : kSpanPeak;
      const parkedSpan = mobile ? kSpanAsideMobile : kSpanAside;

      const span = scrollEase < peak
        ? THREE.MathUtils.lerp(hero, wide, scrollEase / peak)
        : THREE.MathUtils.lerp(
            wide,
            parkedSpan,
            Math.min(1, (scrollEase - peak) / (settled - peak)),
          );

      panel.position.z = THREE.MathUtils.lerp(0, 1.6, park);

      const distance = camera.position.z - panel.position.z;
      const visibleHeight = 2 * Math.tan((camera.fov * Math.PI) / 360) * distance;
      const visibleWidth = visibleHeight * camera.aspect;

      /*  Whichever limit binds. The tilt and the drop shadow both need a
          little room, hence the margin below one. */
      const scale = Math.min(
        (span * visibleWidth) / PANEL_ASPECT,
        (mobile ? kMaxHeightSpanMobile : kMaxHeightSpan) * visibleHeight,
      );

      panel.scale.setScalar(scale);

      /*  AND IT GOES TO THE SIDE. The centre is a fraction of the viewport
          too, so "hug the right edge with a small margin" is one expression
          rather than a number that only holds at one width. On a phone
          there is no side to go to - a panel half off a 390px screen is not
          a reveal, it is a bug - so it stays centred and lifts instead,
          clearing the card below it. */
      const spanNow = (scale * PANEL_ASPECT) / visibleWidth;
      const parked = 1 - spanNow / 2 - kEdgeMargin;
      const centre = mobile
        ? 0.5
        : THREE.MathUtils.lerp(kHeroCentre, Math.max(kHeroCentre, parked), park);

      panel.position.x = (centre - 0.5) * visibleWidth;
      panel.position.y = mobile
        ? (0.5 - kMobileBandCentre) * visibleHeight
        : 0;

      // Turning to the hand. Small angles: a panel that swings is a toy.
      const tiltX = (pointerEase.y - 0.5) * 0.28;
      const tiltY = (pointerEase.x - 0.5) * -0.42;

      panel.rotation.x = tiltX;
      panel.rotation.y = tiltY + THREE.MathUtils.lerp(0, -0.22, park);

      // The lattice counter-parallaxes, so the two layers separate.
      lattice.position.x = (pointerEase.x - 0.5) * -0.35;
      lattice.position.y = (pointerEase.y - 0.5) * 0.35;

      renderer.render(scene, camera);
    },

    dispose() {
      for (const texture of textures) texture.dispose();
      panelMaterial.dispose();
      renderer.dispose();
    },
  };
}
