#!/usr/bin/env python3
"""Generates the plugin's logo SVGs from the SITE'S OWN wordmark.

ONE BRAND, ONE SOURCE OF GEOMETRY. The marketing site's boot screen draws
GNARL from a stroke font in site/src/loader.ts - polylines in a 0..0.9 by
0..1 box, advance 1.28, tracking 0.1. The plugin's logo is built from
those same polylines, so the mark somebody sees on the website is the mark
they see in their DAW, rather than a second drawing of the same word.

If the glyphs in loader.ts change, re-run this and the BinaryData swap in
the same commit. Colour is the site's --accent, #64e6ff.

Strokes are converted to FILLED outlines here (a quad per segment and a
disc at every vertex, which gives round caps and round joins) because the
plugin loads these through Drawable::createFromImageData and
getOutlineAsPath(), which returns fill geometry - an SVG stroke would come
through as nothing.
"""
import math, re, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
BLUE = '#64e6ff'
ADVANCE, TRACKING = 1.28, 0.1          # loader.ts: GLYPH_ADVANCE, textGeometry(..., 0.1)
STROKE = 0.13                          # stroke width as a fraction of cap height

def glyphs():
    src = (ROOT / 'site/src/loader.ts').read_text()
    out = {}
    for ch in 'GNARL':
        m = re.search(r'^\s*%s:\s*(\[.*\]),\s*$' % ch, src, re.M)
        assert m, f'glyph {ch} not found in loader.ts'
        out[ch] = eval(m.group(1))     # plain nested number lists
    return out

def quad(a, b, w):
    (x1, y1), (x2, y2) = a, b
    dx, dy = x2 - x1, y2 - y1
    L = math.hypot(dx, dy) or 1.0
    nx, ny = -dy / L * w / 2, dx / L * w / 2
    pts = [(x1 + nx, y1 + ny), (x2 + nx, y2 + ny), (x2 - nx, y2 - ny), (x1 - nx, y1 - ny)]
    return 'M' + ' L'.join(f'{x:.2f},{y:.2f}' for x, y in pts) + ' Z'

def disc(p, r):
    x, y = p
    return f'M{x - r:.2f},{y:.2f} a{r:.2f},{r:.2f} 0 1,0 {2 * r:.2f},0 a{r:.2f},{r:.2f} 0 1,0 {-2 * r:.2f},0 Z'

def word_paths(text, h, ox, oy, G):
    """Cap height h; (ox, oy) is the top-left of the cap box. y is flipped:
    loader.ts is y-up (three.js), SVG is y-down."""
    d, pen, w = [], 0.0, STROKE * h
    for ch in text:
        for stroke in G[ch]:
            pts = [(ox + (pen + x) * h, oy + (1 - y) * h) for x, y in stroke]
            for a, b in zip(pts, pts[1:]):
                d.append(quad(a, b, w))
            d += [disc(p, w / 2) for p in pts]
        pen += ADVANCE + TRACKING
    width = (pen - ADVANCE - TRACKING + 0.9) * h
    return d, width

def svg(w, h, body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.0f} {h:.0f}" '
            f'width="{w:.0f}" height="{h:.0f}">{body}</svg>\n')

def paths(d):
    return ''.join(f'<path d="{p}" fill="{BLUE}"/>' for p in d)

def ring(cx, cy, R, t):
    r = R - t
    return (f'<path fill="{BLUE}" fill-rule="evenodd" d="'
            f'M{cx - R},{cy} a{R},{R} 0 1,0 {2 * R},0 a{R},{R} 0 1,0 {-2 * R},0 Z '
            f'M{cx - r},{cy} a{r},{r} 0 1,1 {2 * r},0 a{r},{r} 0 1,1 {-2 * r},0 Z"/>')

def main():
    G = glyphs()
    icons = ROOT / 'icons'
    H, PAD = 100.0, 10.0

    d, w = word_paths('GNARL', H, PAD, PAD, G)
    (icons / 'gnarl_word.svg').write_text(svg(w + 2 * PAD, H + 2 * PAD, paths(d)))

    #  The mark: the G alone, square box, same weight as the wordmark.
    d, w = word_paths('G', H, PAD, PAD, G)
    (icons / 'gnarl_mark.svg').write_text(svg(w + 2 * PAD, H + 2 * PAD, paths(d)))

    #  The ring is the boot screen's dial. Same thickness as a glyph stroke.
    (icons / 'gnarl_ring.svg').write_text(svg(100, 100, ring(50, 50, 48, STROKE * 32)))

    #  Lockup: the wordmark inside the dial, as the boot screen composes it.
    S = 400.0
    h = S * 0.105
    d, w = word_paths('GNARL', h, 0, 0, G)
    d, w = word_paths('GNARL', h, (S - w) / 2, (S - h) / 2, G)
    (icons / 'gnarl_word_ring.svg').write_text(
        svg(S, S, ring(S / 2, S / 2, S / 2 - 4, STROKE * h) + paths(d)))

    for f in ['gnarl_word', 'gnarl_mark', 'gnarl_ring', 'gnarl_word_ring']:
        print(f'{f}.svg', (icons / f'{f}.svg').stat().st_size, 'bytes')

if __name__ == '__main__':
    main()
