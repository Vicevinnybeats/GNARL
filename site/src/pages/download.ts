import { boot } from '../boot';
import { createUniverse } from '../universe';

// The vortex resolving into the interface itself.
boot('download', () => createUniverse(document.querySelector('#stage')!, { from: 3, to: 4 }));
