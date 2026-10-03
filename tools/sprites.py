# Generates the pixel-art for items, the thorn orbit, characters, chests and the world.
# Enemy walk sheets live in tools/creatures.py.
# Run from the repo root: python tools/sprites.py   (needs Pillow)
#
# Sprites are ASCII grids: '.' is empty, every other char is a palette key.
# Each shape gets a 1px outline in the palette's 'o' colour and a soft shadow,
# matching the Godot-era art.
import math

from PIL import Image

OUT = "assets/"
SHADOW = (46, 75, 77, 112)


def hexrgb(s):
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4)) + (255,)


def pal(**kw):
    return {k: hexrgb(v) for k, v in kw.items()}


def grid(s):
    rows = [r for r in s.strip("\n").split("\n")]
    w = max(len(r) for r in rows)
    return [r.ljust(w, ".") for r in rows]


def mirror(g):
    return [r[::-1] for r in g]


def outline(img, color):
    w, h = img.size
    px = img.load()
    edge = [(x, y) for y in range(h) for x in range(w) if px[x, y][3] == 0 and any(
        0 <= x + dx < w and 0 <= y + dy < h and px[x + dx, y + dy][3] == 255 and px[x + dx, y + dy] != SHADOW
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
    for p in edge:
        px[p] = color


def render(g, p, frame=0, size=None):
    """One sprite as an RGBA image (no outline), frame picks feet/wing pose."""
    h, w = len(g), len(g[0])
    img = Image.new("RGBA", size or (w, h + 1), (0, 0, 0, 0))
    px = img.load()
    bob = 1 if frame % 2 else 0
    for y, row in enumerate(g):
        for x, c in enumerate(row):
            if c == ".":
                continue
            yy = y
            if c in "12":
                if (c == "1" and frame == 1) or (c == "2" and frame == 3):
                    yy -= 1
                c = "f"
            elif c in "<>":
                if (c == "<") != (frame % 2 == 0):
                    continue
                c = "w"
                yy -= bob
            elif not any(ch in "12" for ch in g[y]):
                yy -= bob  # body bobs, feet stay planted
            if 0 <= yy:
                px[x, yy + 1] = p[c]
    return img


def cell(g, p, frame=0):
    """Sprite centred in a 48 px cell, feet on row 31, with outline and shadow."""
    spr = render(g, p, frame)
    c = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    w, h = spr.size
    x0, y0 = 24 - w // 2, 32 - h
    sw = max(6, w - 2)
    for y in range(31, 34):
        half = sw // 2 - (1 if y != 32 else 0)
        for x in range(24 - half, 24 + half + (sw % 2)):
            c.putpixel((x, y), SHADOW)
    c.alpha_composite(spr, (x0, y0))
    outline(c, p["o"])
    return c


def icon_img(g, p, size=16):
    spr = render(g, p)
    spr = spr.crop(spr.getbbox())
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    img.alpha_composite(spr, ((size - spr.width) // 2, (size - spr.height) // 2))
    outline(img, p["o"])
    return img


def icon(name, g, p, size=16):
    icon_img(g, p, size).save(OUT + name)


def strip(name, grids, p, size=16):
    """Animation frames side by side, size x size each."""
    img = Image.new("RGBA", (size * len(grids), size), (0, 0, 0, 0))
    for i, g in enumerate(grids):
        img.alpha_composite(icon_img(g, p, size), (i * size, 0))
    img.save(OUT + name)


# ---------------------------------------------------------------- weapons

THORN = pal(o="2a3a1a", G="7ccf4a", g="4a9a34", t="f0f0c0", T="c0d090", r="a03a3a", R="e0584a", w="ffffff")
icon("weapons/thorn/thorn.png", grid("""
.......tt..
......tT...
.....gg....
..t.gGg....
..TgGGg.t..
...gGgggT..
..gGGg.....
.gGg.......
gGg........
gg.........
"""), THORN)
ORB = grid("""
....t....
...tg....
.t.gGg.t.
..gGGGgT.
tgGGrGGgt
.TgGGGg..
.t.gGg.t.
....gt...
....t....
""")


def orb_frame(f):
    """A glint runs round the thorn tips, one quadrant per frame; the core throbs."""
    rows = [list(r) for r in ORB]
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c in "tT" and int((math.atan2(y - 4, x - 4) + math.pi) / (math.pi / 2)) % 4 == f:
                row[x] = "w"
            elif c == "r" and f in (1, 2):
                row[x] = "R"
    return ["".join(r) for r in rows]


strip("weapons/thorn/orb.png", [orb_frame(f) for f in range(4)], THORN)

# ---------------------------------------------------------------- items

ITEM_ART = {
    "quill": (pal(o="3a2a4a", w="ffffff", W="d8d0f0", s="9a8ac8", n="5a4a8a"), """
.........ww
.......wwWw
......wWWs.
.....wWWs..
....wWWs...
...wWWs....
..wWWs.....
..wWs......
..Ws.......
.ns........
n..........
"""),
    "bark": (pal(o="3a2412", B="a86a3a", b="7a4a26", d="5a3418", g="6ab04c", G="8ad86a"), """
..gG.gG.....
.gGGgGGg....
..bBBBBBBb..
.bBdBBBdBBb.
.bBBdBBBdBb.
.bdBBdBBBdb.
.bBdBBdBBBb.
.bBBdBBBdBb.
.bdBBBdBBdb.
..bbbbbbbb..
"""),
    "boots": (pal(o="2a1a12", L="c87a3a", l="9a5426", s="ffffff", W="e0f4ff", d="5a3418"), """
..WW.......
.WsW.......
..WWLLL....
....LlL....
....LlL....
....LLL....
....LLLL...
....LLLLLL.
...dLLLLLLL
...ddddddd.
"""),
    "clover": (pal(o="1a3a1a", G="5bc23f", g="3a8f35", h="b9f07a", s="4a6a2a"), """
..gGG.GGg..
.gGhGgGhGg.
.gGGGgGGGg.
..gGGgGGg..
gGGGgggGGGg
gGhGGgGGhGg
gGGGg.gGGGg
.ggg.s.ggg.
.....s.....
......s....
"""),
    "sprout": (pal(o="2a2412", G="6ad04c", g="3a9a35", h="c0f08a", P="c8743a", p="9a5226", d="6a3418"), """
.gGG...GGg.
gGhGG.GGhGg
.gGGGgGGGg.
...ggg.g...
.....g.....
..PPPPPPP..
..pPPPPPp..
...PPPPP...
...pPPPp...
....ddd....
"""),
    "leech": (pal(o="3a1220", R="e04a5a", r="a8283a", h="ffb0b8", G="5bc23f", g="3a8f35"), """
.....gG....
....gGg....
...RRRRR...
..RhRRRRr..
.RhRRRRRRr.
.RRRRRRRRr.
.RRRRRRRrr.
..rRRRRrr..
...rrrrr...
"""),
    "ember": (pal(o="3a120a", R="ff7a2a", r="d83a1a", y="ffe07a", Y="fff6c8", d="8a1a10"), """
....y.....
...yy..y..
..yYy.yy..
..yYYyYy..
.rRyYYYyR.
.RRRyYyRRr
rRRRRRRRRr
rRdRRRRdRr
.rRdddRRr.
..rrrrrr..
"""),
    "bell": (pal(o="2a2a12", Y="ffd23a", y="d89a1c", h="fff4a8", b="3ac8ff", k="6a4a1a"), """
....kk....
...kYYk...
...YhYY...
..YhYYYy..
..YhYYYy..
..YYYYYy..
.YYYYYYyy.
yyyyyyyyyy
....bb....
...b..b...
"""),
    "spores": (pal(o="2a1a3a", P="b45ad0", p="8a3aa8", h="e8b0ff", g="a0e05a", s="f0e0ff"), """
.g......g.
...g.g....
g.PPPPP..g
.PhPPsPPp.
PPsPPPPsPp
PPPPhPPPPp
.pPsPPPPp.
..ppPPpp..
...g..g...
.g......g.
"""),
    "charm": (pal(o="12223a", B="5aa0ff", b="2a60c8", h="d0e8ff", y="ffd23a", Y="fff4a8", k="6a4a1a"), """
....kk....
...k..k...
....yy....
..bBBBBb..
.bBhBBBBb.
.BhBYyBBB.
.BBBYyBBb.
.bBBBBBBb.
..bBBBBb..
...bBBb...
....bb....
"""),
    "crown": (pal(o="3a2a0a", Y="ffd23a", y="d8961c", h="fff4a8", w="ffffff", W="e0e8ff", G="5bc23f", r="e04a7a"), """
.w....w....w.
wWw..wWw..wWw
.Y....Y....Y.
.YY..YYY..YY.
.YhY.YrY.YhY.
.YYYYYYYYYYY.
.YrYYGYYYrYy.
.YYYYYYYYYYy.
.yyyyyyyyyyy.
"""),
    "sickle": (pal(o="1a1a2a", S="d8e0f0", s="9aa4b8", h="ffffff", B="8a5a3a", b="5a3418", r="c82a3a"), """
....SSSS...
..SShhSSS..
.Sh....sSS.
.S......sS.
.........S.
........sS.
.......bs..
......Bb...
.....Bb....
....rB.....
...Bb......
..Bb.......
"""),
}
for name, (p, art) in ITEM_ART.items():
    icon(f"player/items/{name}.png", grid(art), p)


# ---------------------------------------------------------------- characters

def recolor(src, dst, mapping, cols=(0, 1, 2, 3)):
    """Copy a character sheet's walk frames, swapping palette colours."""
    im = Image.open(OUT + src).convert("RGBA")
    out = Image.new("RGBA", (48 * 4, 48 * 4), (0, 0, 0, 0))
    for r in range(4):
        for i, c in enumerate(cols):
            out.alpha_composite(im.crop((c * 48, r * 48, c * 48 + 48, r * 48 + 48)), (i * 48, r * 48))
    m = {hexrgb(a): hexrgb(b) for a, b in mapping.items()}
    out.putdata([m.get(px, px) for px in out.get_flattened_data()])
    out.save(OUT + dst)


# Shy keeps her Godot sheet; every other character is drawn in tools/characters.lua.
recolor("player/shy.png", "player/shy_walk.png", {}, cols=(2, 3, 4, 5))
# ---------------------------------------------------------------- chests

# Frames: closed wood, closed gold (the guardian's free reward), opened.
CHEST_CLOSED = grid("""
...wwwwwwwwwwwwww...
..wWWhWWWWWWWWWWWw..
.wWWMMWWWWWWWWMMWWw.
.wWWMMWWWWWWWWMMWWw.
.wwwMMwwwwwwwwMMwww.
.mmmmmmmmyYYymmmmmm.
.dwwMMwwwyYkywMMwwd.
.dWWMMWWWyyyyWMMWWd.
.dWWMMWWWWWWWWMMWWd.
.dwwMMwwwwwwwwMMwwd.
.dWWMMWWWWWWWWMMWWd.
.dddmmddddddddmmddd.
""")
CHEST_OPEN = grid("""
...wwwwwwwwwwwwww...
..wWWMMWWWWWWWMMWw..
.wWWWMMWWWWWWWMMWWw.
.wwwwMMwwwwwwwMMwww.
.mkkkkkkkkkkkkkkkkm.
.mkkkkkkkkkkkkkkkkm.
.mmmmmmmmyYYymmmmmm.
.dwwMMwwwyYkywMMwwd.
.dWWMMWWWyyyyWMMWWd.
.dWWMMWWWWWWWWMMWWd.
.dwwMMwwwwwwwwMMwwd.
.dWWMMWWWWWWWWMMWWd.
.dddmmddddddddmmddd.
""")
WOOD = pal(o="2a1610", W="b4683a", w="8f4f2c", d="66391f", h="d88a52", M="9aa4b8", m="5c6478", Y="ffd23a", y="c88a1c", k="2a1a14", f="2a1610")
GOLD = pal(o="3a2a0a", W="ffd23a", w="e0a02a", d="a8661a", h="fff4a8", M="ffffff", m="c8d0e0", Y="7cf7ff", y="2a9ad0", k="1a2a4a", f="3a2a0a")
chest = Image.new("RGBA", (48 * 3, 48), (0, 0, 0, 0))
for i, (g, p) in enumerate([(CHEST_CLOSED, WOOD), (CHEST_CLOSED, GOLD), (CHEST_OPEN, WOOD)]):
    chest.alpha_composite(cell(g, p), (i * 48, 0))
chest.save(OUT + "drops/chest/chest.png")


# ---------------------------------------------------------------- world: portal


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


# Water is drawn in tools/terrain.lua.


# Trees live in tools/trees.lua (Aseprite).
import random
BAYER = [[0, 2], [3, 1]]
def ramp(*cs):
    return [hexrgb(c) for c in cs]


def shade(rmp, light, x, y):
    light += (BAYER[y % 2][x % 2] - 1.5) * 0.05
    return rmp[max(0, min(len(rmp) - 1, int(light * len(rmp))))]



# Portal: a giant lily opened flat on the ground, seen at 3/4, a swirling pool in its heart.
# Frames 0-2 corrupted (wilted violet petals with thorns, violet vortex, sparks),
# 3-5 purified (white petals blushing pink, golden stamens, teal pool, pollen motes).
# The petals sway a pixel over the three frames.
def portal(frame, pure):
    img = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    px = img.load()
    swirl = ramp(*(("1e3a4a", "2a9aa8", "6ae0d0", "e8fff4") if pure else ("1a0a24", "4a1a6a", "8a2ab0", "e070ff")))
    petal = ramp(*(("c06a98", "e8a8c8", "f6dcea", "fff8fc", "ffffff") if pure else ("1e0e2a", "3a1a4e", "5a2a70", "7a3a8e", "a050b0")))
    cx, cy = 24, 31
    sway = (0, 1, 0)[frame] if pure else (0, 1, -1)[frame]

    def blob(x0, y0, r, color):
        for y in range(int(y0 - r) - 1, int(y0 + r) + 2):
            for x in range(int(x0 - r) - 1, int(x0 + r) + 2):
                if 0 <= x < 48 and 0 <= y < 48 and (x + 0.5 - x0) ** 2 + (y + 0.5 - y0) ** 2 <= r * r:
                    px[x, y] = color

    def draw_petal(ang):
        back = math.sin(ang) < 0
        reach, ground = (19, 7) if back else (16, 10)  # front petals start at the pool's rim so it shows
        bx, by = cx + math.cos(ang) * ground, cy + math.sin(ang) * ground * 0.6
        lift = 9 if back else 1  # back petals stand up, front ones lie open
        tx = cx + math.cos(ang) * reach + sway * (1 if math.cos(ang) > 0 else -1)
        ty = cy + math.sin(ang) * reach * 0.55 - lift + (0 if pure else 3)  # wilted tips droop
        for s in range(18):
            t = s / 17
            x = bx + (tx - bx) * t
            y = by + (ty - by) * t - math.sin(t * math.pi) * (3 if back else 1)
            w = (3.6 if back else 3.0) * math.sin(math.pi * min(1.0, t * 1.1) ** 0.75) + 0.4
            light = 0.25 + t * 0.6 - (0.2 if not back else 0) + (0.1 if math.cos(ang) < 0 else 0)
            blob(x, y, w, shade(petal, light, int(x), int(y)))
        for s in range(4, 15):  # midrib
            t = s / 17
            x, y = bx + (tx - bx) * t, by + (ty - by) * t - math.sin(t * math.pi) * (3 if back else 1)
            px[int(x), int(y)] = petal[1] if pure else hexrgb("b040c0")
        if pure:
            px[int(tx), int(ty)] = hexrgb("f080b0")  # blushing tip
        else:  # thorn on the tip
            px[int(tx), int(ty) - 1] = hexrgb("120818")
            px[int(tx), int(ty) - 2] = hexrgb("120818")

    angles = [k * 2 * math.pi / 8 + math.pi / 8 for k in range(8)]
    for ang in sorted(angles, key=math.sin):  # back petals first
        if math.sin(ang) < 0:
            draw_petal(ang)
    rx, ry = 10, 5.5
    for y in range(48):
        for x in range(48):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            d = (dx * dx + dy * dy) ** 0.5
            if d <= 1:
                a = math.atan2(dy, dx)
                band = (a * 2 / math.pi + d * 3 - frame * 0.67) % 2  # spiral arms turning each frame
                level = 0 if d < 0.25 else 1 + int(band > 1.2) + int(band > 1.75)
                px[x, y] = swirl[1 if d > 0.85 else level]
    for k in range(7):  # stamens on stalks round the back rim, or thorns
        a = math.pi + (k + 0.5) * math.pi / 7
        sx, sy = int(cx + math.cos(a) * 8), int(cy + math.sin(a) * 4)
        h = 4 + (k % 2) * 2
        for y in range(sy - h, sy):
            px[sx, y] = hexrgb("b8d878") if pure else hexrgb("2a1236")
        px[sx, sy - h - 1] = hexrgb("ffd23a") if pure else hexrgb("e070ff")
        if pure:
            px[sx + 1, sy - h - 1] = hexrgb("f0a020")
    for ang in sorted(angles, key=math.sin):
        if math.sin(ang) >= 0:
            draw_petal(ang)
    r = random.Random(frame)
    for i in range(7):  # pollen / sparks drifting up out of the pool
        mx = cx + r.uniform(-9, 9)
        my = cy - 3 - ((i * 5 + frame * 4) % 24)
        px[int(mx), int(my)] = hexrgb("fff2a8") if pure else swirl[2 + (i % 2)]
        if not pure and i % 3 == 0:
            px[int(mx) + 1, int(my) + 1] = swirl[1]
    outline(img, hexrgb("2a1a2a") if pure else hexrgb("0c0612"))
    return img
portals = Image.new("RGBA", (48 * 6, 48), (0, 0, 0, 0))
for i in range(6):
    portals.alpha_composite(portal(i % 3, i >= 3), (i * 48, 0))
portals.save(OUT + "world/portal.png")


# ---------------------------------------------------------------- pickups
# drops/pickups.png: 16 px cells, a row per kind (main.cpp PICKUP_ROW), four animation
# frames: exp seeds small/medium/large, magnet, feather (speed), seed bomb, gold, silver, heart berry.
def seed_frames(rmp, rx, ry):
    frames = []
    for f in range(4):
        img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        px = img.load()
        cx, cy = 8, 9.5
        for y in range(16):
            for x in range(16):
                dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
                if dy < 0:
                    dx *= 1 + (-dy) * 0.5  # teardrop: narrower at the top
                if dx * dx + dy * dy <= 1:
                    px[x, y] = shade(rmp, 0.62 - dx * 0.35 - dy * 0.3 - (dx * dx + dy * dy) * 0.2, x, y)
        top = int(cy - ry)
        px[8, top - 1], px[9, top - 2], px[10, top - 2] = hexrgb("4a8a3a"), hexrgb("6ac04a"), hexrgb("a8e06a")  # sprout
        hx, hy = int(cx - rx * 0.4), int(cy - ry * 0.35)
        if f < 2:
            px[hx, hy] = hexrgb("ffffff")
        if f == 1 and rx > 3:  # a glint on the second frame
            for ox, oy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                px[hx + ox, hy + oy] = rmp[-1]
        outline(img, tuple(max(0, v - 30) for v in rmp[0][:3]) + (255,))
        frames.append(img)
    return frames


def grid_frames(art, p, n=4):
    return [icon_img(grid(art[f % len(art)]), p) for f in range(n)]


MAGNET = ["""
..rrrrrr..
.rRRRRRRr.
rRRrrrrRRr
rRr....rRr
rRr....rRr
rRr....rRr
wWw....wWw
wWw....wWw
""", """
..rrrrrr..
.rRRRRRRr.
rRRrrrrRRr
rRr....rRr
rRr....rRr
rRr....rRr
WWw....WWw
wWw....wWw
"""]
FEATHER = ["""
.......wWW
.....wwWWb
....wWWbb.
...wWWbb..
..wWWbb...
.wWbbb....
.Wbb......
s.........
""", """
.......wWW
.....wWWWb
....wWWbb.
...wWbbb..
..wWWbb...
.wWbbb....
.Wbb......
s.........
"""]
BOMB = ["""
.....y.
....f..
...f...
.kkkkk.
kKKkkkk
kKkkkkk
kkkkkkd
kkkkkdd
.kkddd.
""", """
....y.Y
....f..
...f...
.kkkkk.
kKKkkkk
kKkkkkk
kkkkkkd
kkkkkdd
.kkddd.
""", """
.....Y.
....f..
...f...
.kkkkk.
kKKkkkk
kKkkkkk
kkkkkkd
kkkkkdd
.kkddd.
"""]
HEART = ["""
...ll..
..lL...
.RR.RR.
RWRRRRR
RRRRRRR
RRRRRRd
.RRRRd.
..RRd..
...d...
""", """
...ll..
..lL...
.RR.RR.
RRRRRRR
RWRRRRR
RRRRRRd
.RRRRd.
..RRd..
...d...
"""]


def coin_frames(rmp):
    frames = []
    for w in (7, 5, 1.5, 5):  # spinning: face, turning, edge, turning
        img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        px = img.load()
        for y in range(16):
            for x in range(16):
                dx, dy = (x + 0.5 - 8) / (w / 2), (y + 0.5 - 8) / 3.6
                if dx * dx + dy * dy <= 1:
                    px[x, y] = shade(rmp, 0.6 - dx * 0.2 - dy * 0.3, x, y)
                    if w > 4 and 0.45 < dx * dx + dy * dy < 0.75:
                        px[x, y] = rmp[1]  # rim
        if w > 4:
            px[7, 6], px[7, 7] = rmp[-1], rmp[-1]
        outline(img, tuple(max(0, v - 30) for v in rmp[0][:3]) + (255,))
        frames.append(img)
    return frames


PICKUPS = [
    seed_frames(ramp("2a7a3a", "3e9a3e", "6ac04a", "a8e06a", "e0ffc0"), 2.6, 3.2),  # exp, small
    seed_frames(ramp("1e4a8a", "2a6ac8", "4a9ae8", "8ac8ff", "e0f4ff"), 3.4, 4.0),  # exp, medium
    seed_frames(ramp("7a2a8a", "b04ac8", "d880f0", "f0b8ff", "fff0ff"), 4.2, 4.8),  # exp, large
    grid_frames(MAGNET, pal(o="2a0e12", r="a02a2a", R="e0484a", w="a8b0c0", W="ffffff")),
    grid_frames(FEATHER, pal(o="12283a", w="8ad0ff", W="e0f6ff", b="4a8ad0", s="2a4a6a")),
    grid_frames(BOMB, pal(o="0e0a10", k="3a3048", K="6a6080", d="241c2c", f="c8a070", y="ffd23a", Y="ff6a2a")),
    coin_frames(ramp("8a5a10", "c88a1c", "ffd23a", "fff0a0", "ffffff")),  # gold
    coin_frames(ramp("4a5060", "7a8498", "b8c0d0", "e8ecf4", "ffffff")),  # silver
    grid_frames(HEART, pal(o="3a0a14", R="e83a4a", W="ffc0c8", d="a01e2e", l="6ac04a", L="3e9a3e")),
]
pickups = Image.new("RGBA", (16 * 4, 16 * len(PICKUPS)), (0, 0, 0, 0))
for row, frames in enumerate(PICKUPS):
    for col, f in enumerate(frames):
        pickups.alpha_composite(f, (col * 16, row * 16))
pickups.save(OUT + "drops/pickups.png")
print("sprites ok")
