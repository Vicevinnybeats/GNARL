import { boot } from '../boot';
import { createUniverse } from '../universe';

// Modulation winding into the rack.
boot('presets', () => createUniverse(document.querySelector('#stage')!, { from: 2, to: 3 }));
