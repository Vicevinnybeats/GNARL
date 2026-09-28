import { boot } from '../boot';
import { createUniverse } from '../universe';

// Orbits into the oscillator's own surface.
boot('home', () => createUniverse(document.querySelector('#stage')!, { from: 0, to: 1 }));
