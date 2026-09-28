"""Draws the PWA icon set.

GENERATED, NOT DRAWN BY HAND, so the set cannot drift: every size comes from
one vector description, and regenerating after a palette change updates all of
them. The mark is the same waveform the favicon carries, so the installed app
and the browser tab read as one thing.

TWO SHAPES, and the difference matters on Android: a normal icon is shown as
drawn, and a MASKABLE one is cropped to whatever shape the launcher uses -
circle, squircle, rounded square. A maskable icon therefore needs its content
inside the middle 80% ("safe zone") or the launcher cuts it. Shipping only a
normal icon gets it letterboxed in a white box on Android; shipping only a
maskable one wastes 20% everywhere else. Both, declared with `purpose`.
"""
from PIL import Image, ImageDraw
import os

BG_DEEP = (5, 4, 15)
VIOLET = (166, 120, 255)
CYAN = (100, 230, 255)

OUT = os.path.join(os.path.dirname(__file__), '..', 'ui', 'public', 'icons')


def draw(size, maskable):
    # 4x supersample, then downscale: the waveform is thin diagonal strokes and
    # PIL has no antialiased line drawing.
    s = size * 4
    img = Image.new('RGB', (s, s), BG_DEEP)
    d = ImageDraw.Draw(img)

    # A soft radial lift, approximated with concentric ellipses - the flat
    # near-black reads as a hole on a dark launcher.
    for i in range(28, 0, -1):
        f = i / 28
        r = int(s * 0.75 * f)
        c = (int(5 + 30 * (1 - f)), int(4 + 18 * (1 - f)), int(15 + 60 * (1 - f)))
        d.ellipse([s * 0.16 - r // 2, -r // 2, s * 0.16 + r, r], fill=c)

    # The safe zone: a maskable icon keeps its content inside the middle 80%.
    inset = 0.26 if maskable else 0.16
    x0, x1 = s * inset, s * (1 - inset)
    mid = s * 0.5
    amp = s * (0.16 if maskable else 0.20)

    pts = [0.0, 1.0, -1.0, 1.0, -1.0, 0.55, -0.35]
    step = (x1 - x0) / (len(pts) - 1)
    line = [(x0 + i * step, mid - v * amp) for i, v in enumerate(pts)]

    w = max(2, int(s * 0.055))
    d.line(line, fill=VIOLET, width=int(w * 1.9), joint='curve')
    d.line(line, fill=CYAN, width=w, joint='curve')

    return img.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    made = []
    for size in (96, 192, 512):
        p = os.path.join(OUT, f'icon-{size}.png')
        draw(size, False).save(p, optimize=True)
        made.append(p)
    for size in (192, 512):
        p = os.path.join(OUT, f'maskable-{size}.png')
        draw(size, True).save(p, optimize=True)
        made.append(p)
    # iOS ignores the manifest's icons and reads this one.
    p = os.path.join(OUT, 'apple-touch-icon.png')
    draw(180, False).save(p, optimize=True)
    made.append(p)

    for p in made:
        print(f'  {os.path.basename(p):24} {os.path.getsize(p):>7,} bytes')


if __name__ == '__main__':
    main()
