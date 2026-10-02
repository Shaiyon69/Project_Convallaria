# Generates the pixel-art sheets for enemies, items, weapons and characters.
# Run from the repo root: python tools/sprites.py   (needs Pillow)
#
# Sprites are ASCII grids: '.' is empty, every other char is a palette key.
# Each shape gets a 1px outline in the palette's 'o' colour and a soft shadow,
# matching the Godot-era art. Walk sheets are 4 frames x 4 rows (down, left,
# right, up) of 48 px cells; right is the mirrored left view.
#   '1' / '2'  feet: lifted a pixel on frames 1 / 3 (drawn in colour 'f')
#   '<' / '>'  wings: shown on even / odd frames (drawn in colour 'w')
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


def walk_sheet(name, views, p):
    """views: dict with 'down', 'left', 'up' grids."""
    sheet = Image.new("RGBA", (48 * 4, 48 * 4), (0, 0, 0, 0))
    rows = [views["down"], views["left"], mirror(views["left"]), views["up"]]
    for r, g in enumerate(rows):
        for f in range(4):
            sheet.alpha_composite(cell(g, p, f), (f * 48, r * 48))
    sheet.save(OUT + name)


def icon(name, g, p, size=16):
    spr = render(g, p)
    spr = spr.crop(spr.getbbox())
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    img.alpha_composite(spr, ((size - spr.width) // 2, (size - spr.height) // 2))
    outline(img, p["o"])
    img.save(OUT + name)


# ---------------------------------------------------------------- enemies

BEETLE = pal(o="1f3a1c", G="5bc23f", g="3a8f35", d="276b2c", h="b9f07a", k="16241a", e="f4fff0", f="2a3d22")
walk_sheet("enemies/beetle.png", {
    "down": grid("""
..k.......k..
...k.....k...
....ggggg....
..gGGhGGGGg..
.gGhhGGGGGGg.
.gGGGGgGGGGg.
gdgGGGgGGGgdg
gdkekGgGkekdg
.dddggdggddd.
..dddddddd...
.1.2.....1.2.
"""),
    "left": grid("""
.k...........
..k..........
...kggggg....
..gGGhhGGgg..
.gGGhhGGGGGg.
gkeGGGGGGGGgg
gkkdgggGGGGdg
.ddddddgggdd.
..ddddddddd..
..1.2.1.2.1..
"""),
    "up": grid("""
....ggggg....
..gGGhGGGGg..
.gGhhGgGGGGg.
.gGGGGgGGGGg.
gdGGGGgGGGGdg
gdGGGGgGGGGdg
gdgGGGgGGGgdg
.dddgggggddd.
..ddddddddd..
.1.2.....1.2.
"""),
}, BEETLE)

BEE = pal(o="3a2414", Y="ffd23a", y="e79a1c", k="2b1d14", h="fff4a8", e="ffffff", w="d8f2ff", f="2b1d14")
walk_sheet("enemies/bee.png", {
    "down": grid("""
.<<.......<<.
<<<<.k.k.<<<<
.>>>.....>>>.
....YYYYY....
...YhYYYYY...
..YYYYYYYYY..
..YekYYYekY..
..kkkkkkkkk..
..yYYYYYYYy..
..kkkkkkkkk..
...yyyyyyy...
.....yyy.....
"""),
    "left": grid("""
...<<<<......
..<<<<<......
...>>>>>.....
..k.>>>......
.k.YYYYYkk...
..YhYYYkYYk..
.YekYYkYYkYy.
.YkYYYkYYkYyk
..YYYYkYYkyy.
...yyykyyk...
"""),
    "up": grid("""
.<<.......<<.
<<<<.k.k.<<<<
.>>>.....>>>.
....YYYYY....
...YhYYYYY...
..YYYYYYYYY..
..kkkkkkkkk..
..yYYYYYYYy..
..kkkkkkkkk..
...yyyyyyy...
.....yky.....
"""),
}, BEE)

SHROOM = pal(o="4a2016", C="ffcf3a", c="e08a1e", s="fff6d0", h="fff19a", S="f2e2c0", t="cdb48e", k="2a1a14", m="8a2a2a", f="7a5a3a")
walk_sheet("enemies/shroom.png", {
    "down": grid("""
....cCCCCc....
..cCChCCsCCc..
.cCssCCCCCCCc.
cCCsCCCCCsCCCc
cCCCCCsCCCCCcc
ccCCCCCCCCCccc
.ccccccccccc..
...StSSSStS...
...SkSSSkSS...
...SSSmmSSS...
...tSSSSSSt...
....tSSSSt....
....1....2....
"""),
    "left": grid("""
....cCCCCc....
..cCChCCCsCc..
.cCssCCCCCCCc.
cCCsCCCCsCCCCc
cCCCCCCCCCCCcc
ccCCCCCCCCCccc
.ccccccccccc..
...SSSStSS....
..mkSSSSSSt...
..mmSSSSSSS...
...SSSSSSSt...
....tSSSSt....
....1....2....
"""),
    "up": grid("""
....cCCCCc....
..cCChCCCCCc..
.cCCCCCCsCCCc.
cCCsCCCCCCCCCc
cCCCCCCCCsCCcc
ccCCCCCCCCCccc
.ccccccccccc..
...SSSSSSSS...
...SSSSSSSS...
...tSSSSSSt...
...tSSSSSSt...
....tSSSSt....
....1....2....
"""),
}, SHROOM)

BOAR = pal(o="3b1a14", B="b4533c", b="8a3a2c", d="62281f", h="d98a68", s="f0d9c8", k="1d0f0c", n="e8a0a0", f="3b1a14")
walk_sheet("enemies/boar.png", {
    "down": grid("""
..bb......bb..
.bBBb.dd.bBBb.
..bBBBBBBBBb..
.bBhBBBBBBBBb.
.bBBkBBBBkBBb.
bBBBBBnnnBBBBb
bBBBBnkknBBBBb
bdBsBnnnnBsBdb
bdBBsBBBBsBBdb
.bdBBBBBBBBdb.
..bddddddddb..
..11.2..1.22..
"""),
    "left": grid("""
.......bb.dd..
......bBBbddd.
...bbBBBBBBBdd
..bBBBhBBBBBBd
.bBkBBBBBBBBBd
nnBBBBBBBBBBBd
nkBBBBBBBBBBBd
nnsBBBBBBBBBdb
.sBBBBBBBBBddb
..bdBBBBBBddb.
...bbdddddddb.
...11.2..1.22.
"""),
    "up": grid("""
..bb......bb..
.bBBb.dd.bBBb.
..bBBBddBBBb..
.bBBBBddBBBBb.
.bBBBBddBBBBb.
bBBBBBddBBBBBb
bBBBBBBBBBBBBb
bdBBBBBBBBBBdb
bdBBBBBBBBBBdb
.bdBBBBBBBBdb.
..bddddddddb..
..11.2..1.22..
"""),
}, BOAR)

GOLEM = pal(o="22262a", R="9aa3a8", r="737c82", d="565e64", h="c8d0d4", M="6ab04c", m="4a8a3a", e="7cf7ff", k="22262a", f="565e64")
walk_sheet("enemies/golem.png", {
    "down": grid("""
....mMMMMm....
..mMMMmMMMMm..
.mRMMRRRRMMRm.
.RRRRRRRRRRRr.
.RhRRRRRRRRRr.
.RRekRRRRekRr.
dRRRRRRRRRRRrd
dRrRRRrrRRRrRd
dRrRRRRRRRRrRd
drRrrrrrrrrRrd
.rrrrrrrrrrrr.
..rr.rddr.rr..
..11.....22...
"""),
    "left": grid("""
.....mMMMMm...
...mMMMmMMMMm.
..MMRRRRRMMRR.
..RRRRRRRRRRRr
..RhRRRRRRRRRr
..keRRRRRRRRRr
..RRRRRRRRRRRd
..RRrRRRRrRRRd
..dRRRRRRRRRrd
...rrrrrrrrrrd
...rrrrrrrrrr.
....rr.rd.rr..
....11...22...
"""),
    "up": grid("""
....mMMMMm....
..mMMMmMMMMm..
.mMMMMMMMMMMm.
.RMMRMMRRMMRr.
.RRRRRRRRRRRr.
.RhRRRRRRRRRr.
dRRRRRRrRRRRrd
dRrRRRRrRRRrRd
dRrRRRRRRRRrRd
drRrrrrrrrrRrd
.rrrrrrrrrrrr.
..rr.rddr.rr..
..11.....22...
"""),
}, GOLEM)

HARE = pal(o="133c44", T="5fd6d0", t="38a6a6", d="257a80", h="b8fff6", p="ff9cc0", k="10262a", e="ffffff", n="ff7aa8", f="257a80")
walk_sheet("enemies/hare.png", {
    "down": grid("""
..tT....Tt..
..tpT..Tpt..
..tpT..Tpt..
..tTT..TTt..
..tTTTTTTt..
.tTThTTTTTt.
.tTekTTekTt.
.tTTTnnTTTt.
..tTTTTTTt..
..tThhhhTt..
..tdThhTdt..
..dd.dd.dd..
..1.......2.
"""),
    "left": grid("""
....tTTt....
...tTppT....
..tTpTt.....
..tTTt......
..tTTTTt....
.tTTTTTTt...
nTekTTTTTt..
tTTTTTTTTTt.
.tTTTThhTTTt
..tdTThhTTdt
...tTTTTTdt.
...ddd.dddhh
...1.....2..
"""),
    "up": grid("""
..tT....Tt..
..tTT..TTt..
..tTT..TTt..
..tTT..TTt..
..tTTTTTTt..
.tTTTTTTTTt.
.tTTTTTTTTt.
.tTTTTTTTTt.
..tTTTTTTt..
..tTThhTTt..
..tdThhTdt..
..dd.dd.dd..
..1.......2.
"""),
}, HARE)

# ---------------------------------------------------------------- weapons

THORN = pal(o="2a3a1a", G="7ccf4a", g="4a9a34", t="f0f0c0", T="c0d090", r="a03a3a")
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
icon("weapons/thorn/orb.png", grid("""
....t....
...tg....
.t.gGg.t.
..gGGGgT.
tgGGrGGgt
.TgGGGg..
.t.gGg.t.
....gt...
....t....
"""), THORN)

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


# Lily: hair e1b975 ba895b fbe568, bow 9d466e c94d82 e275a4, dress 044669 045886 036ca8 04354e.
# Shy: hair 6785d1 aebee8 c7ccf1 4e71c8, scarf ca2635 c93c49 c94e51.
recolor("player/shy.png", "player/shy_walk.png", {}, cols=(2, 3, 4, 5))
recolor("player/woman.png", "player/ivy.png", {
    "e1b975": "6ab04c", "ba895b": "3a8f35", "fbe568": "9ad86a",  # hair
    "9d466e": "c8743a", "c94d82": "ffb03a", "e275a4": "ffe07a",  # bow becomes a marigold
    "044669": "6a3418", "045886": "8a4a26", "036ca8": "a86a3a", "04354e": "4a2410",  # dress
})
recolor("player/woman.png", "player/rowan.png", {
    "e1b975": "c8482a", "ba895b": "8a2a1a", "fbe568": "f07a4a",
    "9d466e": "2a6a8a", "c94d82": "3aa0c8", "e275a4": "8ae0ff",
    "044669": "2a5a2a", "045886": "3a7a3a", "036ca8": "5aa04a", "04354e": "1a3a1a",
})
recolor("player/shy.png", "player/nyx.png", {
    "6785d1": "4a2e6a", "aebee8": "7a4ea0", "c7ccf1": "a07ac8", "4e71c8": "2e1a48",
    "ca2635": "3ac890", "c93c49": "5ae0a8", "c94e51": "2a9a70",
}, cols=(2, 3, 4, 5))
print("sprites ok")
