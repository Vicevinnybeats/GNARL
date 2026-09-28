import { boot } from '../boot';
import { createDissolve } from '../dissolve';

/*  The one page that is not the morphing figure. The rack takes a signal
    apart, so this page takes an object apart - see dissolve.ts for why the
    picture and the subject are the same idea here rather than an animation
    applied to one. */
boot('fx', () => createDissolve(document.querySelector('#stage')!));
