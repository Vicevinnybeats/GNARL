import { boot } from '../boot';
import { createUniverse } from '../universe';

// The surface, opening into modulation.
boot('engine', () => createUniverse(document.querySelector('#stage')!, { from: 1, to: 2 }));
