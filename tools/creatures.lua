-- Enemy walk sheets (assets/enemies/<name>.png), rendered by headless Aseprite. Run from the repo root:
--   aseprite -b --script tools/creatures.lua
--   aseprite -b --script-param only=beetle,bee --script tools/creatures.lua
-- A sheet is 4 frames x 4 rows (down, left, right, up) of 48 px cells, the same layout as the
-- Godot characters; right is the mirrored left view. Each view is a body grid ('.' empty, other
-- chars are palette keys; '<' / '>' are wings shown on alternate frames in colour 'w') whose
-- bottom row is the ground (cell row 31), plus legs. sym() builds a facing view from its left
-- half (the last column is the centre), swapping highlight keys on the shadowed right side.
-- A leg runs hip -> knee -> foot. In side views feet swing back and forth over the 4-frame
-- cycle (legs in phase 1 opposite to phase 0) and lift as they pass forward; facing views lift
-- alternate feet. The body bobs a pixel on the passing frames. A creature's `ramp` (dark to
-- light keys) gets rim light: one step lighter on its top-left edge, one darker bottom right.
-- Every sprite gets a 1px outline in palette colour 'o'; the game draws ground shadows.

local STRIDE = { 0, 1, 0, -1 }
local pc = app.pixelColor
local function hex(s) return pc.rgba(tonumber(s:sub(1, 2), 16), tonumber(s:sub(3, 4), 16), tonumber(s:sub(5, 6), 16), 255) end

local function grid(s)
  local rows, w = {}, 0
  for line in (s .. "\n"):gmatch("(.-)\n") do
    line = line:match("^%s*(.-)%s*$")
    if #line > 0 then rows[#rows + 1] = line; w = math.max(w, #line) end
  end
  for i, r in ipairs(rows) do rows[i] = r .. string.rep(".", w - #r) end
  return rows
end

-- A facing view from its left half; `swap` recolours the mirrored (shadowed) side.
local function sym(s, swap)
  local rows = grid(s)
  for i, r in ipairs(rows) do
    local right = r:sub(1, -2):reverse()
    if swap then right = right:gsub(".", function(c) return swap[c] or c end) end
    rows[i] = r .. right
  end
  return rows
end

local function Leg(x, y, dx, ph, o)
  o = o or {}
  return { x = x, y = y, dx = dx or 0, ph = ph or 0, c = o.c or "l", w = o.w or 1, k = o.k or 0, s = o.s or 1,
    back = o.back or false, foot = o.foot, toe = o.toe or 0, hang = o.hang or 0, ky = o.ky or 0 }
end

local function line(x0, y0, x1, y1)
  local pts, dx, dy = {}, math.abs(x1 - x0), -math.abs(y1 - y0)
  local sx, sy, err = x0 < x1 and 1 or -1, y0 < y1 and 1 or -1, math.abs(x1 - x0) - math.abs(y1 - y0)
  while true do
    pts[#pts + 1] = { x0, y0 }
    if x0 == x1 and y0 == y1 then return pts end
    local e2 = 2 * err
    if e2 >= dy then err = err + dy; x0 = x0 + sx end
    if e2 <= dx then err = err + dx; y0 = y0 + sy end
  end
end

local function frame(img, cx, cy, view, cr, f, side, fly)
  local body = view.bodies and view.bodies[f + 1] or view.body
  local legs = view.legs or {}
  local H, W = #body, #body[1]
  local alt = view.alt or 0
  local ox, oy = 24 - W // 2, 32 - H - alt
  local bob = fly and ({ 0, 1, 2, 1 })[f + 1] or ({ 0, 1, 0, 1 })[f + 1]
  local cell = {}
  local function put(x, y, c)
    local X, Y = ox + x, oy + y
    if X >= 0 and Y >= 0 and X < 48 and Y < 48 then cell[Y * 48 + X] = c end
  end
  local function leg(L)
    local s = STRIDE[f + 1] * (L.ph == 0 and 1 or -1) * L.s
    local hx, hy = L.x, L.y - bob
    local fx, fy
    if L.hang > 0 then
      fx, fy = hx + L.dx + (side and s or 0), hy + L.hang
    else
      fx, fy = hx + L.dx + (side and s or 0), H - 1
      if (side and s < 0) or (not side and s > 0) then fy = fy - 1 end  -- the swinging foot lifts
    end
    local kx, ky = (hx + fx) // 2 + L.k, (hy + fy + 1) // 2 + L.ky
    local pts = line(hx, hy, kx, ky)
    for _, p in ipairs(line(kx, ky, fx, fy)) do pts[#pts + 1] = p end
    for _, p in ipairs(pts) do
      for i = 0, L.w - 1 do put(p[1] + i, p[2], L.c) end
    end
    if L.foot then
      for i = 0, L.w + math.abs(L.toe) - 1 do put(fx + (L.toe >= 0 and i or -i), fy, L.foot) end
    end
  end
  for _, L in ipairs(legs) do if L.back then leg(L) end end
  for y, row in ipairs(body) do
    for x = 1, #row do
      local c = row:sub(x, x)
      if c ~= "." then
        local show = true
        if c == "<" or c == ">" then
          show = (c == "<") == (f % 2 == 0)
          c = "w"
        end
        if show then put(x - 1, y - 1 - bob, c) end
      end
    end
  end
  -- rim light on the body: top-left edges a step lighter, bottom-right a step darker
  if cr.ramp then
    local idx, shifted = {}, {}
    for i, k in ipairs(cr.ramp) do idx[k] = i end
    for Y = 0, 47 do
      for X = 0, 47 do
        local k = cell[Y * 48 + X]
        local i = k and idx[k]
        if i then
          local function open(x, y) return x < 0 or y < 0 or x >= 48 or y >= 48 or not cell[y * 48 + x] end
          local lit = open(X, Y - 1) or open(X - 1, Y)
          local dark = open(X, Y + 1) or open(X + 1, Y)
          if lit and not dark then i = math.min(#cr.ramp, i + 1) elseif dark and not lit then i = math.max(1, i - 1) end
          shifted[Y * 48 + X] = cr.ramp[i]
        end
      end
    end
    for k, v in pairs(shifted) do cell[k] = v end
  end
  for _, L in ipairs(legs) do if not L.back then leg(L) end end
  if view.wings then  -- (up, down) overlays, flapping every frame
    for y, row in ipairs(view.wings[f % 2 + 1]) do
      for x = 1, #row do
        local c = row:sub(x, x)
        if c ~= "." then put(x - 1, y - 1 - bob, c) end
      end
    end
  end
  local o = cr.p.o
  for Y = 0, 47 do
    for X = 0, 47 do
      local k = cell[Y * 48 + X]
      if k then
        img:drawPixel(cx + X, cy + Y, cr.p[k] or error(cr.name .. ": no colour for key " .. k))
      else
        for _, d in ipairs{ { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
          local x, y = X + d[1], Y + d[2]
          if x >= 0 and y >= 0 and x < 48 and y < 48 and cell[y * 48 + x] then
            img:drawPixel(cx + X, cy + Y, o)
            break
          end
        end
      end
    end
  end
end

local function flip(g)
  local out = {}
  for i, r in ipairs(g) do out[i] = r:reverse() end
  return out
end

local function mirrorView(v)
  local W = #v.body[1]
  local out = {}
  for k, val in pairs(v) do out[k] = val end
  out.body = flip(v.body)
  out.legs = {}
  for i, L in ipairs(v.legs or {}) do
    local M = {}
    for k, val in pairs(L) do M[k] = val end
    M.x, M.dx, M.k, M.toe = W - 1 - L.x, -L.dx, -L.k, -L.toe
    if M.w > 1 then M.x = M.x - (M.w - 1) end
    out.legs[i] = M
  end
  if v.bodies then
    out.bodies = {}
    for i, g in ipairs(v.bodies) do out.bodies[i] = flip(g) end
  end
  if v.wings then out.wings = { flip(v.wings[1]), flip(v.wings[2]) } end
  return out
end

local function pal(t)
  local p = {}
  for k, v in pairs(t) do p[k] = hex(v) end
  return p
end

local CREATURES, ORDER = {}, {}
local function creature(name, fn) CREATURES[name] = fn; ORDER[#ORDER + 1] = name end

local function sheet(cr, views, fly)
  local spr = Sprite(48 * 4, 48 * 4, ColorMode.RGB)
  local img = spr.cels[1].image
  local rows = { { views.down, false }, { views.left, true }, { mirrorView(views.left), true }, { views.up, false } }
  for r, rv in ipairs(rows) do
    for f = 0, 3 do frame(img, f * 48, (r - 1) * 48, rv[1], cr, f, rv[2], fly) end
  end
  spr:saveAs(app.fs.joinPath(app.fs.currentPath, "assets", "enemies/" .. cr.name .. ".png"))
  spr:close()
end

-- ---------------------------------------------------------------- beetle (runner): an emerald jewel beetle

creature("beetle", function()
  local cr = { name = "beetle", p = pal{ o = "0c1a14", d = "1a4a32", g = "2a7a48", G = "40aa5a", h = "8ae07a", H = "e0ffe8",
    i = "2e8a9a", j = "5ac8c8", p = "1a2e24", P = "2e4a3a", q = "4e7060", e = "f0f8b0", m = "c8a050", k = "1a2e24", l = "14241c" } }
  local swap = { H = "h", h = "G" }
  local left = { body = grid[[
    .k......................
    ..k.....................
    ..k......dddddddd.......
    ...k...ddgGGhhGGgdd.....
    ...k..dgGGhHHhGGGGgd....
    ....pdgGGGhhhGGGGGGGd...
    ..pPPPdGGGGGGGGGGGGGgd..
    .pPqqPPdGGGGGGGGGGGGgd..
    mpPePqPPdgGGGGGGGGGGjd..
    .mpPPPPPdggGGGGGGGGjid..
    ..mppPPPddiigggggggiid..
    .....pppdddiiiiiiiidd...
    ..........dddddddddd....
    ........................
    ........................
  ]], legs = { Leg(8, 11, -2, 0, { back = true }), Leg(12, 11, -1, 1, { back = true }), Leg(16, 11, 1, 0, { back = true }),
    Leg(8, 12, -2, 1), Leg(12, 12, 0, 0), Leg(16, 12, 2, 1) } }
  local down = { body = sym([[
    .....dddd
    ...ddgGGd
    ..dgGhhGd
    .dgGhHhGd
    .dGGhhGGd
    .dGGGGGGd
    .dgGGGGGd
    .djgGGGGd
    ..dijggGd
    ...ddiiid
    ..k.pPPPP
    ...kPqqPP
    ...pPePPP
    ....pPPPP
    ....mpppp
    ....m....
    .........
    .........
  ]], swap), legs = { Leg(2, 6, -3, 0, { back = true }), Leg(14, 6, 3, 1, { back = true }), Leg(2, 8, -3, 1), Leg(14, 8, 3, 0),
    Leg(4, 12, -2, 0), Leg(12, 12, 2, 1) } }
  local up = { body = sym([[
    ...k.....
    ....k....
    ....pPPPP
    ...dpPqPP
    ..ddgGGGd
    .dgGhhGGd
    .dGhHhGGd
    .dGGhGGGd
    .dGGGGGGd
    .dgGGGGGd
    .djgGGGGd
    ..dijgGGd
    ...ddiiid
    .....dddd
    .........
    .........
  ]], swap), legs = { Leg(2, 7, -3, 0, { back = true }), Leg(14, 7, 3, 1, { back = true }), Leg(2, 9, -3, 1), Leg(14, 9, 3, 0),
    Leg(3, 11, -2, 0), Leg(13, 11, 2, 1) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- bee (swarm): round and fuzzy, glassy wings

creature("bee", function()
  local cr = { name = "bee", p = pal{ o = "2a1a0e", Y = "ffd23a", y = "d8901c", h = "fff4a8", k = "2b1d14", K = "4a3424",
    f = "fff0d0", F = "e8c890", E = "3a4a8a", e = "ffffff", s = "3a2a1a", w = "d8f2ff", W = "ffffff", l = "2b1d14" } }
  local left = { body = grid[[
    .................
    ..k..............
    ...k.............
    ...kk.fFf........
    ..kEEkfffYYkkYk..
    .kEeEkFfYhYkkYYk.
    .kEEEkFYYYYkkYYkk
    ..kkk.yYYYykkYYks
    .......yyyykkyk..
    .................
  ]], wings = { grid[[
    .......wW........
    ......wWWw.......
    .....wWWWw.......
    ......wWw........
    .......w.........
  ]], grid[[
    .................
    .................
    .................
    .................
    .........wwww....
    ........wWWWWw...
    .........www.....
  ]] }, legs = { Leg(7, 8, 0, 0, { hang = 2 }), Leg(9, 8, 1, 1, { hang = 2 }), Leg(8, 8, -1, 1, { hang = 2, back = true }) }, alt = 5 }
  local frontWings = { grid[[
    .................
    .................
    .ww...........ww.
    wWWw.........wWWw
    wWWWw.......wWWWw
    .wWWw.......wWWw.
    ..ww.........ww..
  ]], grid[[
    .................
    .................
    .................
    .................
    .................
    .................
    .................
    www...........www
    wWWWw.......wWWWw
    .wWWw.......wWWw.
    ..ww.........ww..
  ]] }
  local down = { body = sym([[
    ........
    ......yy
    .....yYY
    ....kkkk
    ...yYYhY
    ...kkkkk
    ...fFfff
    ..k.fFff
    ...kkkkk
    ..kEEkkk
    ..kEeEkk
    ...kkkkK
  ]], { h = "Y" }), wings = frontWings, legs = { Leg(6, 11, -1, 0, { hang = 2 }), Leg(9, 11, 1, 1, { hang = 2 }) }, alt = 4 }
  local up = { body = sym([[
    ........
    ...k....
    ....k...
    ....kkkk
    ...kkKKk
    ...fFfff
    ..yYYhYY
    ..kkkkkk
    ..yYYYYY
    ..kkkkkk
    ...yYYYY
    .....yyk
    .......s
  ]], { h = "Y" }), wings = frontWings, legs = { Leg(5, 9, -1, 0, { hang = 3, back = true }), Leg(10, 9, 1, 1, { hang = 3, back = true }) }, alt = 4 }
  sheet(cr, { down = down, left = left, up = up }, true)
end)

-- ---------------------------------------------------------------- shroom (shooter): a spotted toadstool with a face

creature("shroom", function()
  local cr = { name = "shroom", p = pal{ o = "4a1a16", C = "ff8a3a", c = "d0581e", D = "9a3a18", h = "ffc070", s = "fff6e8",
    S = "f2e2c4", t = "d0b48e", r = "8a4a2a", R = "b0704a", k = "2a1a14", e = "ffffff", b = "f0a090", m = "8a2a2a", f = "7a5a3a" } }
  local swap = { h = "C", C = "c" }
  local down = { body = sym([[
    .....cccc
    ...ccChhh
    ..cCshhCC
    .cCsssCCC
    .cCCsCCCs
    cCCCCCCss
    cDCCCCCCC
    .DDcccccc
    ..rRRRRRR
    ...tSSSSS
    ...tSeSSS
    ..btSkSSS
    ...tSSSSm
    ...ttSSSS
    ....tttSS
    .........
    .........
  ]], swap), legs = { Leg(5, 13, 0, 0, { c = "t", w = 2, foot = "f" }), Leg(10, 13, 0, 1, { c = "t", w = 2, foot = "f" }) } }
  local left = { body = grid[[
    .....cccccc......
    ...cCChhhCCcc....
    ..cCsshhCCsCCc...
    .cCsssCCCCssCCc..
    .cCCsCCCCCCsCCCc.
    cCCCCCCCCCCCCCccD
    cDCCCCCCCCCCCccDD
    .DDcccccccccccDD.
    ..rRRRRRRRRRRrr..
    ....tSSSSSSSt....
    ...eSSSSSSSSt....
    ..bkSSSSSSSSt....
    ...mSSSSSSSSt....
    ...SSSSSSSSt.....
    ....tttSSStt.....
    .................
    .................
  ]], legs = { Leg(6, 13, -1, 0, { c = "t", w = 2, foot = "f", back = true }), Leg(8, 13, 1, 1, { c = "t", w = 2, foot = "f" }) } }
  local up = { body = sym([[
    .....cccc
    ...ccChhh
    ..cCCCsCC
    .cCsCCCCC
    .cCCCCCsC
    cCCsCCCCC
    cDCCCCCCC
    .DDcccccc
    ...tSSSSS
    ...tSSSSS
    ...tSSSSS
    ...ttSSSS
    ....tttSS
    .........
    .........
  ]], swap), legs = { Leg(5, 11, 0, 0, { c = "t", w = 2, foot = "f" }), Leg(10, 11, 0, 1, { c = "t", w = 2, foot = "f" }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- boar (brute): bristled mane, tusks, glaring eye

creature("boar", function()
  local cr = { name = "boar", p = pal{ o = "2a120e", B = "b4533c", b = "8a3a2c", d = "62281f", D = "4a1e18", h = "d98a68",
    H = "f0b090", m = "3a1a14", M = "5a2a20", n = "e8a0a0", N = "f0c0b8", k = "1d0f0c", s = "f6ecd8", e = "ffd040", t = "62281f" } }
  local left = { body = grid[[
    ..........................
    .........bb...M.M.M.......
    ........bBhb.mMmMmMm......
    .......bBHhbmMMMMMMMmm....
    ......bBhBBBbBBBBBBBBBMm..
    ....bbBBBBBBBBBBBBBBBBBBb.
    ...bBBBekBBBBBBBBBBBBBBBBb
    .nnBBBBBBBBBBhBBBBBBBBBBBt
    nNNnBBBBBhBBBBBBBBBBBBBBdt
    nkknbBBBBBBBBBBBBBBBBBBddt
    .nnsbbBBBBBBBBBBBBBBBBddd.
    ..ss..bbdBBBBBBBBBBBbddd..
    .s.......ddddddddddddd....
    ..........................
    ..........................
    ..........................
  ]], legs = { Leg(10, 11, 0, 1, { c = "D", w = 2, foot = "k", back = true }), Leg(19, 11, 0, 0, { c = "D", w = 2, foot = "k", back = true }),
    Leg(8, 11, 0, 0, { c = "d", w = 2, foot = "k" }), Leg(17, 11, 0, 1, { c = "d", w = 2, foot = "k" }) } }
  local down = { body = sym([[
    ..b.....M
    .bBb..mMm
    .bBhbmMMM
    .bBBBBBBB
    ..bBhBBBB
    ..BBekBBB
    ..BBBBBnn
    .sBBBnNNN
    .sbBBnkNN
    ..sbbnnnn
    ....ddddd
    .........
    .........
    .........
  ]], { h = "B", H = "h" }), legs = { Leg(4, 10, 0, 0, { c = "d", w = 2, foot = "k" }), Leg(11, 10, 0, 1, { c = "d", w = 2, foot = "k" }) } }
  local up = { body = sym([[
    ...bb....
    ..bBBb..M
    ..bBBBmMm
    .bBBBBmMM
    .bBhBBBmM
    bBBBBBBBB
    bBBBBBBBB
    bdBBBBBBB
    bdBBBBBBt
    .bdBBBBBt
    ..bdddddd
    .........
    .........
    .........
  ]], { h = "B" }), legs = { Leg(4, 10, 0, 0, { c = "d", w = 2, foot = "k" }), Leg(11, 10, 0, 1, { c = "d", w = 2, foot = "k" }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- golem (tank): mossy stone, crystal eyes, a glowing rune

creature("golem", function()
  local cr = { name = "golem", p = pal{ o = "1a1e22", R = "9aa3a8", r = "737c82", d = "565e64", D = "3e464c", h = "c8d0d4",
    H = "e8eef0", M = "6ab04c", m = "4a8a3a", N = "9ad86a", e = "7cf7ff", E = "d8ffff", k = "22262a", c = "5ad8e8" },
    ramp = { "d", "r", "R", "h" } }
  local down = { body = sym([[
    .......mMN
    ......mMNN
    ......hRRR
    ......RhRR
    ......REeR
    ...mMNdRRR
    ..hRRRRddd
    .hRRRRRRRR
    hRRrRRRRcR
    RRRrRRRRcc
    RRR.rRRRRR
    rRR.rRRRRR
    rrr..rrrrr
    .rr.......
    ..........
    ..........
  ]], { h = "R", N = "M" }), legs = { Leg(6, 12, 0, 0, { c = "r", w = 3, foot = "d" }), Leg(10, 12, 0, 1, { c = "r", w = 3, foot = "d" }) } }
  local left = { body = grid[[
    ......mMNMm.......
    .....mMNNMMm......
    ....RRRRRRRr......
    ....RhRRRRRr......
    ....ERRRRRRr......
    ....rRRRRRRrMm....
    ...dRRRRRRRRRRr...
    ..RhRRRRRRRRRRRr..
    ..RRRRRRRRRRRRRr..
    ..RRRRRRRRcRRRRr..
    ...rRRRRRRRRRRr...
    ....rrrrrrrrrr....
    ..................
    ..................
    ..................
  ]], legs = { Leg(9, 11, 0, 1, { c = "d", w = 3, foot = "k", back = true }), Leg(5, 11, 0, 0, { c = "r", w = 3, foot = "d" }) } }
  -- the near arm swings opposite the near leg
  left.wings = { grid[[
    ..................
    ..................
    ..................
    ..................
    ..................
    ..................
    ......hR..........
    .....hRRr.........
    .....RRRr.........
    .....RRRr.........
    .....rRRr.........
    .....rrrr.........
    ......rr..........
  ]], grid[[
    ..................
    ..................
    ..................
    ..................
    ..................
    ..................
    ......hR..........
    ......RRRr........
    ......RRRr........
    .......RRRr.......
    .......rRRr.......
    .......rrrr.......
    ........rr........
  ]] }
  local up = { body = sym([[
    .......mMN
    ......mMMN
    ......mMMM
    ......RRMM
    ...mMNRRRR
    ..hRRRRRRR
    .hRRRRRRRR
    hRRrRRRRRR
    RRRrRRRRRR
    RRR.rRRRRR
    rRR.rRRRRR
    rrr..rrrrr
    .rr.......
    ..........
    ..........
  ]], { h = "R", N = "M" }), legs = { Leg(6, 11, 0, 0, { c = "r", w = 3, foot = "d" }), Leg(10, 11, 0, 1, { c = "r", w = 3, foot = "d" }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- hare (dasher)

creature("hare", function()
  local cr = { name = "hare", p = pal{ o = "133c44", T = "5fd6d0", t = "38a6a6", d = "257a80", h = "d8fff8", p = "ff9cc0",
    k = "10262a", e = "ffffff", n = "ff7aa8", w = "ffffff", l = "257a80" }, ramp = { "d", "t", "T", "h" } }
  local left = { body = grid[[
    .......tt.........
    ......tpTt........
    .....tpTt.........
    ....tpTt..........
    ...tTTTt..........
    ..tTTTTTt.........
    .tTekTTTTtTTTt....
    nTTTTTTTTTTTTTTt..
    .tTTThTTTTTTTTTTt.
    ..tThhhTTTTTTTTTtw
    ...thhTTTTTTTTTTww
    ....hhttTTTTTTdd..
    ..................
    ..................
  ]], legs = { Leg(6, 11, -1, 0, { c = "t", foot = "h", back = true }), Leg(5, 11, -1, 1, { c = "t", foot = "h" }),
    Leg(14, 11, 1, 0, { c = "d", w = 2, foot = "h", toe = -2, back = true }), Leg(13, 11, 1, 1, { c = "t", w = 2, foot = "h", toe = -2 }) } }
  local down = { body = grid[[
    ...tT....Tt...
    ...tpT..Tpt...
    ...tpT..Tpt...
    ...tpT..Tpt...
    ...tTTttTTt...
    ..tTTTTTTTTt..
    .tTTTTTTTTTTt.
    .tTekTTTTekTt.
    .tTTTTnnTTTTt.
    ..tTTThhTTTt..
    ..tTThhhhTTt..
    ...tThhhhTt...
    ..............
    ..............
  ]], legs = { Leg(5, 11, 0, 0, { c = "t", foot = "h" }), Leg(8, 11, 0, 1, { c = "t", foot = "h" }),
    Leg(3, 10, -1, 1, { c = "d", w = 2, foot = "h", back = true }), Leg(9, 10, 1, 0, { c = "d", w = 2, foot = "h", back = true }) } }
  local up = { body = grid[[
    ...tT....Tt...
    ...tTT..TTt...
    ...tTT..TTt...
    ...tTT..TTt...
    ...tTTTTTTt...
    ..tTTTTTTTTt..
    .tTTTTTTTTTTt.
    .tTTTTTTTTTTt.
    .tTTTTTTTTTTt.
    ..tTTTwwTTTt..
    ..tdTwwwwTdt..
    ...ddTwwTdd...
    ..............
    ..............
  ]], legs = { Leg(4, 11, 0, 0, { c = "d", w = 2, foot = "h" }), Leg(8, 11, 0, 1, { c = "d", w = 2, foot = "h" }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- toad

creature("toad", function()
  local cr = { name = "toad", p = pal{ o = "1e2a12", T = "7ab04a", t = "4a7a2e", h = "b8e078", E = "f0e070", e = "f8f8c0",
    k = "1a1a10", m = "3a4a1e", b = "e8e0a0", w = "5a8a34", l = "4a7a2e" }, ramp = { "t", "T", "h" } }
  local left = { body = grid[[
    ....................
    ......EeE...........
    ....tTEkETt.........
    ...tTTTTTTtt........
    ..tTThTTTTTTt.......
    .tTTTTTTTwTTTtt.....
    mmmmmTTTTTTTTTwTt...
    .tbbbbTTTTwTTTTTTt..
    ..tbbbbTTTTTTTTwTTt.
    ...ttbbbbbTTTTTTTt..
    .....ttttbbbbTTtt...
    ....................
    ....................
  ]], legs = { Leg(5, 10, -2, 0, { c = "t", foot = "T", toe = -1, back = true }), Leg(6, 10, -2, 1, { c = "T", foot = "h", toe = -1 }),
    Leg(15, 10, 1, 0, { c = "t", w = 2, foot = "T", toe = -2, back = true }), Leg(14, 10, 1, 1, { c = "T", w = 2, foot = "h", toe = -2 }) } }
  local down = { body = grid[[
    ..EeE......EeE..
    .tEkEt....tEkEt.
    .tTTTTTTTTTTTTt.
    tTTTThTTTTTTTTTt
    tTTwTTTTTTTwTTTt
    tTTTTTTTTTTTTTTt
    tmmmmmmmmmmmmmmt
    .tbbbbbbbbbbbbt.
    ..tbbbbbbbbbbt..
    ...tttttttttt...
    ................
    ................
  ]], legs = { Leg(3, 8, -2, 0, { c = "T", foot = "h", toe = -1 }), Leg(12, 8, 2, 1, { c = "T", foot = "h", toe = 1 }),
    Leg(2, 6, -2, 1, { c = "t", w = 2, back = true }), Leg(12, 6, 2, 0, { c = "t", w = 2, back = true }) } }
  local up = { body = grid[[
    ..EeE......EeE..
    .tEEEt....tEEEt.
    .tTTTTTTTTTTTTt.
    tTTThTTTTwTTTTTt
    tTTTTTTwTTTTTTTt
    tTTwTTTTTTTwTTTt
    tTTTTTTTTTTTTTTt
    .tTTTwTTTTTTTTt.
    .ttTTTTTTTTTTtt.
    tTtttttttttttTtt
    ................
    ................
  ]], legs = { Leg(3, 8, -2, 0, { c = "T", foot = "h", toe = -1 }), Leg(12, 8, 2, 1, { c = "T", foot = "h", toe = 1 }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- spider (and spiderlings)

creature("spider", function()
  local cr = { name = "spider", p = pal{ o = "140e1a", A = "5a4a6a", a = "3a2e48", h = "8a7aa0", r = "d04a5a", C = "4a3c5a",
    c = "2e2438", e = "ff5a6a", E = "ffd0d8", f = "d8c8a0", l = "2e2438", L = "1e1828" }, ramp = { "a", "A", "h" } }
  local sideLegs = { Leg(4, 6, -4, 0, { ky = -4, c = "L", back = true }), Leg(5, 6, -2, 1, { ky = -4, c = "L", back = true }),
    Leg(6, 6, 2, 0, { ky = -4, c = "L", back = true }), Leg(7, 6, 4, 1, { ky = -4, c = "L", back = true }),
    Leg(3, 7, -4, 1, { ky = -4 }), Leg(4, 7, -1, 0, { ky = -4 }), Leg(6, 7, 1, 1, { ky = -4 }), Leg(7, 7, 4, 0, { ky = -4 }) }
  local left = { body = grid[[
    ......................
    ...........aAAAAa.....
    ..........aAAhAAAAa...
    .........aAAhAAAAAAa..
    ....cCCc.aAAAArrAAAa..
    ..cCCCCCcaAAAArrAAAa..
    .EeCeCCCCaAAAAAAAAa...
    .cCCCCCCc.aaAAAAaa....
    ..ff.cc....aaaaa......
    ......................
    ......................
    ......................
  ]], legs = sideLegs }
  local downLegs = {}
  for i, yd in ipairs{ { 5, -5 }, { 6, -6 }, { 7, -6 }, { 8, -4 } } do
    local k = i - 1
    downLegs[#downLegs + 1] = Leg(6, yd[1], yd[2], k % 2, { ky = -3, back = k < 2, c = k < 2 and "L" or "l" })
    downLegs[#downLegs + 1] = Leg(11, yd[1], -yd[2], 1 - k % 2, { ky = -3, back = k < 2, c = k < 2 and "L" or "l" })
  end
  local down = { body = grid[[
    ......aAAAAa......
    .....aAAhAAAa.....
    .....aAArrAAa.....
    .....aAArrAAa.....
    ......aAAAAa......
    ......cCCCCc......
    .....cCeCCeCc.....
    .....cCEeeECc.....
    ......cCCCCc......
    .......f..f.......
    ..................
    ..................
    ..................
  ]], legs = downLegs }
  local upLegs = {}
  for i, L in ipairs(downLegs) do
    local M = {}
    for k, v in pairs(L) do M[k] = v end
    M.y = L.y - 4
    upLegs[i] = M
  end
  local up = { body = grid[[
    ......cCCCCc......
    .....cCCCCCCc.....
    ......cCCCCc......
    .....aAAAAAAa.....
    ....aAAhAAAAAa....
    ....aAAArrAAAa....
    ....aAAArrAAAa....
    ....aAAAAAAAAa....
    .....aAAAAAAa.....
    ......aaaaaa......
    ..................
    ..................
    ..................
  ]], legs = upLegs }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- snail

creature("snail", function()
  local cr = { name = "snail", p = pal{ o = "2a1a10", S = "d89a50", s = "9a5a2a", h = "f8d090", d = "6a3a1a", B = "b8c8a8",
    b = "8a9a7a", k = "1a1a10", e = "1a1a10", m = "6a7a5a" } }
  local function body(stretch)
    local rows = grid[[
      ..e...e.............
      ...b.b....sSSSSs....
      ...b.b...sShhhSSs...
      ...bbb..sShssshSSs..
      ..bBBbb.sSsSSSsSSs..
      .bBBBBb.sSsSdsSsSs..
      .bBkBBbbsSssSsSSSs..
      bBBBBBBbbsSSSSSSs...
      bBBBBBBBBbssssss.b..
      bBBBBBBBBBBBBBBBBBb.
      .bbbbbbbbbbbbbbbbbbb
    ]]
    if stretch then return rows end
    local out = {}
    for i, r in ipairs(rows) do out[i] = i <= 9 and r:sub(2) .. "." or r:sub(2, -2) .. ".." end
    return out
  end
  local left = { body = body(true), bodies = { body(true), body(true), body(false), body(false) } }
  local downBody = grid[[
    ....sSSSSSSs....
    ...sShhhhSSSs...
    ..sShsssshSSSs..
    ..sSsSSSSsSSSs..
    ..sSsSdssSsSSs..
    ..sSssSSSsSSSs..
    ...sSSSSSSSSs...
    .....e....e.....
    ......b..b......
    ......b..b......
    ....bbBBBBbb....
    ...bBkBBBBkBb...
    ...bBBBmmBBBb...
    ..bBBBBBBBBBBb..
    ..bbbbbbbbbbbb..
  ]]
  local squashed = {}
  for i = 1, 7 do squashed[#squashed + 1] = downBody[i] end
  for i = 9, #downBody do squashed[#squashed + 1] = downBody[i] end
  squashed[#squashed + 1] = string.rep(".", 16)
  local down = { body = downBody, bodies = { downBody, downBody, squashed, squashed } }
  local up = { body = grid[[
    .....e....e.....
    ......b..b......
    ......bbbb......
    ....sSSSSSSs....
    ...sShhhhSSSs...
    ..sShsssshSSSs..
    ..sSsSSSSsSSSs..
    ..sSsSdssSsSSs..
    ..sSssSSSsSSSs..
    ...sSSSSSSSSs...
    ..bbBBBBBBBBbb..
    ..bBBBBBBBBBBb..
    ...bbbbbbbbbb...
  ]] }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- crow

creature("crow", function()
  local cr = { name = "crow", p = pal{ o = "0e0e18", K = "3a3a52", k = "22222e", h = "6a6a8a", e = "ffffff", y = "d8b040",
    Y = "f0d070", w = "2a2a3e", W = "4e4e6e", l = "d8b040" }, ramp = { "k", "K", "h" } }
  local left = { body = grid[[
    ................
    ...kKK..........
    .yyKeK..........
    ..yKKKKkk.......
    ...kKhKKKKKk....
    ....kKKKKKKKKkk.
    .....kkKKKKkkkkk
    .......kk.......
  ]], wings = { grid[[
    ......wW........
    .....wWWw.......
    .....wWWWw......
    ......wWWw......
    .......ww.......
  ]], grid[[
    ................
    ................
    ................
    ................
    .......wWWWw....
    ........wWWWw...
    .........www....
  ]] }, legs = { Leg(7, 7, 0, 0, { hang = 2 }), Leg(8, 7, 0, 1, { hang = 2, back = true }) }, alt = 8 }
  local frontWings = { grid[[
    .wW..........Ww.
    .wWW........WWw.
    ..wWW......WWw..
    ...ww......ww...
  ]], grid[[
    ................
    ................
    ................
    ................
    wwWW........WWww
    .wwWW......WWww.
    ....w......w....
  ]] }
  local down = { body = grid[[
    ................
    ......kKKk......
    .....kKKKKk.....
    .....KeKKeK.....
    .....kKyyKk.....
    ......kYYk......
    .....kKyKKk.....
    ....kKKKKKKk....
    ....kKKhKKKk....
    .....kKKKKk.....
    ......kkkk......
  ]], wings = frontWings, legs = { Leg(6, 10, 0, 0, { hang = 2 }), Leg(9, 10, 0, 1, { hang = 2 }) }, alt = 6 }
  local up = { body = grid[[
    ................
    ......kKKk......
    .....kKKKKk.....
    .....kKKKKk.....
    ......kKKk......
    .....kKKKKk.....
    ....kKKhKKKk....
    ....kKKKKKKk....
    .....kKKKKk.....
    ......kkkk......
    .....kk..kk.....
  ]], wings = frontWings, alt = 6 }
  sheet(cr, { down = down, left = left, up = up }, true)
end)

-- ---------------------------------------------------------------- mole

creature("mole", function()
  local cr = { name = "mole", p = pal{ o = "140e10", M = "5a4a50", m = "3e3238", h = "8a7a80", n = "ff9ab0", N = "ffc8d4",
    k = "0a0608", P = "ffb8c8", q = "e07a98", t = "3e3238", l = "3e3238" }, ramp = { "m", "M", "h" } }
  local left = { body = grid[[
    ..................
    ......mMMMMMm.....
    ....mMMhMMMMMMm...
    ...mMMhMMMMMMMMm..
    .nmMkMMMMMMMMMMMm.
    nNmMMMMMMMMMMMMMMt
    .nmMMMMMMMMMMMMMmt
    ..qPmMMMMMMMMMMm..
    .qPPqmmmmmmmmmm...
    ..................
    ..................
  ]], legs = { Leg(5, 8, -1, 0, { c = "q", w = 2, foot = "P", toe = -1 }), Leg(13, 8, 0, 1, { c = "m", w = 2, foot = "q" }),
    Leg(6, 8, -1, 1, { c = "m", w = 2, back = true }), Leg(14, 8, 0, 0, { c = "m", w = 2, back = true }) } }
  local down = { body = grid[[
    .....mmmmmm.....
    ...mMMMMMMMMm...
    ..mMMhMMMMMMMm..
    ..mMMMMMMMMMMm..
    .mMMkMMMMMMkMMm.
    .mMMMMMnnMMMMMm.
    qPmMMMnNNnMMMmPq
    qPPmMMMnnMMMmPPq
    .qqmmMMMMMMmmqq.
    ....mmmmmmmm....
    ................
    ................
  ]], legs = { Leg(5, 9, 0, 0, { c = "m", w = 2, foot = "q" }), Leg(9, 9, 0, 1, { c = "m", w = 2, foot = "q" }) } }
  local up = { body = grid[[
    .....mmmmmm.....
    ...mMMMMMMMMm...
    ..mMMhMMMMMMMm..
    ..mMMMMMMMMMMm..
    .mMMMMMMMMMMMMm.
    .mMMMMMMMMMMMMm.
    qmMMMMMMMMMMMMmq
    qqmMMMMMMMMMMmqq
    ..mmMMMMMMMMmm..
    ....mmmtmmmm....
    ................
    ................
  ]], legs = { Leg(5, 9, 0, 0, { c = "m", w = 2, foot = "q" }), Leg(9, 9, 0, 1, { c = "m", w = 2, foot = "q" }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- ---------------------------------------------------------------- bosses
-- Bodies fill more of the cell than the regular kinds (the game draws them at BOSS_ART scale), each
-- with its own silhouette so a guardian never reads as a big regular or an elite.

-- Hive Queen: crowned, a long violet-banded abdomen, broad glassy wings
creature("hive_queen", function()
  local cr = { name = "hive_queen", p = pal{ o = "2a1430", Y = "ffc23a", y = "d88a1c", h = "fff0a0", k = "3a1e4a", K = "5a3270",
    f = "fff0d0", F = "e8c890", E = "8a3aa8", e = "ffffff", G = "ffd84a", g = "c89020", r = "ff3a5a", s = "2a1a1a",
    w = "d8f2ff", W = "ffffff", l = "3a1e4a" } }
  local left = { body = grid[[
    ....G..G..G........................
    ....GG.GG.GG.......................
    ....GGGGrGGG.......................
    ....gggggggg.......................
    ...kkkkkkkkkk..fFf.................
    ..kKKKKKKKKKKk.fFFff...............
    .kKEEEKKKKKKKkfFFfff..yYYYy........
    .kEEeEEKKKKKKkfffffFyYYhhYYkkYYk...
    .kEEEEEKKKKKKkFffffYYhYYYYkkYYkkk..
    .kEEEEKKKKKKkkfFfffYYYYYYYkkYYkkYk.
    ..kEEKKKKKKkk.ffFffyYYYYYYkkYYkkkss
    ...kKKKKKkk....fffyyYYYYYkkYYkkk...
    ....kkkk.k......f..yyyyyykkyykk....
    .........k.........................
  ]], wings = { grid[[
    ...............wWWw.......
    ..............wWWWWWw.....
    .............wWWWWWWWw....
    ..............wWWWWWWWw...
    ...............wwWWWww....
    .................www......
  ]], grid[[
    ..........................
    ..........................
    ..........................
    ..........................
    ......................wwwww.
    .....................wWWWWWWw
    ......................wWWWWWw
    .......................wwww..
  ]] }, legs = { Leg(15, 11, -1, 0, { hang = 3 }), Leg(18, 11, 1, 1, { hang = 3 }), Leg(16, 11, 0, 1, { hang = 3, back = true }) }, alt = 6 }
  local frontWings = { grid[[
    .......................
    .......................
    ..www.............www..
    .wWWWw...........wWWWw.
    wWWWWWw.........wWWWWWw
    wWWWWWWw.......wWWWWWWw
    .wWWWWWw.......wWWWWWw.
    ..wwwww.........wwwww..
  ]], grid[[
    .......................
    .......................
    .......................
    .......................
    .......................
    .......................
    .......................
    .......................
    wwww...............wwww
    wWWWWw...........wWWWWw
    .wWWWWw.........wWWWWw.
    ..wwww...........wwww..
  ]] }
  local down = { body = sym([[
    ...........y
    .........yYY
    ........kkkk
    .......yYYhY
    .......kkkkk
    ......yYYYYY
    .......kkkkk
    ......fFffff
    ......G..G.G
    ......GGrGGG
    ......gggggg
    .....kKKKKKK
    ....kEEEKKKK
    ....kEeEKKKK
    ....kEEEKKKK
    .....kEKKKKK
    ......kkkkkk
    ......k.....
  ]], { h = "Y" }), wings = frontWings, legs = { Leg(8, 16, -1, 0, { hang = 3 }), Leg(14, 16, 1, 1, { hang = 3 }) }, alt = 5 }
  local up = { body = sym([[
    ......G..G.G
    ......GGGGGG
    ......gggggg
    .....kKKKKKK
    .....kKKKKKK
    ......kkkkkk
    ......fFffff
    .....fFfffff
    ......ffffff
    ....yYYYhYYY
    ....kkkkkkkk
    ....yYYYYYYY
    ....kkkkkkkk
    .....yYYYYYY
    ......kkkkkk
    .......yyyyk
    ...........s
  ]], { h = "Y" }), wings = frontWings, legs = { Leg(7, 8, -1, 0, { hang = 4, back = true }), Leg(15, 8, 1, 1, { hang = 4, back = true }) }, alt = 5 }
  sheet(cr, { down = down, left = left, up = up }, true)
end)

-- Stone Colossus: basalt slabs split by glowing magma, amber crystals on its shoulders, huge fists
creature("colossus", function()
  local cr = { name = "colossus", p = pal{ o = "120c0a", R = "5a5250", r = "463e3c", d = "322a28", D = "221c1a", h = "7a706c",
    m = "e8501c", M = "ffb03a", c = "ffd070", C = "e89a2a", e = "ffe080", E = "ff8020" }, ramp = { "d", "r", "R", "h" } }
  local down = { body = sym([[
    ...c...........
    ..cCc......dRRR
    ..cCCc....dRRRR
    .dRRRRd...dRhRR
    dRRhRRRd..dReER
    dRhRRRRRdddRRRR
    dRRRRRRRRRRRRRR
    dRRRRRRRRRRmRRR
    .dRRRdRRRRRmmRR
    .dRRd.dRRRRRmRR
    .dRRd.dRRRRRRmm
    dRRRRd.dRRRRmMM
    dRRRRd.dRRRRRmM
    dRmRRd..dRRRRRR
    .dRRd...dRRRRRR
    ..dd.....dddddd
    ...............
    ...............
  ]], { h = "R", R = "r", e = "E" }), legs = { Leg(10, 15, 0, 0, { c = "D", w = 4, foot = "d", s = 2 }), Leg(15, 15, 0, 1, { c = "D", w = 4, foot = "d", s = 2 }) } }
  local left = { body = grid[[
    ...........c.........
    ..........cCc........
    ....dRRRRdcCCc.......
    ...dRhRRRRRRRRd......
    ..dReERRRRRhRRRd.....
    ..dRRRRRRRRRRRRRd....
    ...dRRRRRRmRRRRRd....
    ....dRRRRmmRRRRRd....
    ....dRRRRRmMRRRd.....
    ....dRRRRRRmRRRd.....
    ...dRRRRd.dRRRRd.....
    ..dRhRRRd..dRRRd.....
    ..dRRRmRd..dRRd......
    ..dRRRRRd...dd.......
    ...dddddd............
    .....................
    .....................
    .....................
  ]], legs = { Leg(7, 14, 0, 1, { c = "D", w = 4, foot = "d", s = 2, back = true }), Leg(11, 14, 0, 0, { c = "r", w = 4, foot = "d", s = 2 }) } }
  local up = { body = sym([[
    ...c...........
    ..cCc..........
    ..cCCc.....dRRR
    .dRRRRd...dRRRR
    dRRhRRRd..dRhRR
    dRhRRRRRdddRRRR
    dRRRRRRRRRRRRRR
    dRRRRRmRRRRRRRR
    .dRRRdRmRRRRRRR
    .dRRd.dRmmRRRRR
    .dRRd.dRRRmRRRR
    dRRRRd.dRRRmmRR
    dRRRRd.dRRRRRmM
    dRRRRd..dRRRRRR
    .dRRd...dRRRRRR
    ..dd.....dddddd
    ...............
    ...............
  ]], { h = "R", R = "r" }), legs = { Leg(10, 15, 0, 0, { c = "D", w = 4, foot = "d", s = 2 }), Leg(15, 15, 0, 1, { c = "D", w = 4, foot = "d", s = 2 }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- Brood Mother: a bloated spider with a clutch of eggs on her back, a red hourglass and many eyes
creature("brood_mother", function()
  local cr = { name = "brood_mother", p = pal{ o = "140a14", A = "4a2a3a", a = "2e1824", h = "7a4a6a", r = "e0304a", C = "3a2232",
    c = "22121c", e = "ff5a6a", E = "ffd0d8", f = "e8d8b0", g = "c8bca0", G = "f4f0e0", W = "ffffff", l = "2e1824", L = "1a0c14" },
    ramp = { "a", "A", "h" } }
  local sideLegs = {}
  for i, d in ipairs{ -9, -3, 3, 9 } do
    sideLegs[#sideLegs + 1] = Leg(5 + i * 2, 9, d, i % 2, { ky = -8, c = "L", w = 2, back = true })
    sideLegs[#sideLegs + 1] = Leg(4 + i * 2, 10, d + (d < 0 and 1 or -1), 1 - i % 2, { ky = -8, w = 2 })
  end
  local left = { body = grid[[
    .....................gGGg..gGg.......
    ...................gGGWGGggGWGg......
    ..................gGWGGGgGGGGGGg.....
    .................aGGGGgaagGGGgGa.....
    ................aAAgggaAAAgggaAAAa...
    ...............aAAhAAAAAAAAAAAAAAAa..
    .......cCCc...aAAhAAAAArrAAAAAAAAAa..
    .....cCCCCCCc.aAAAAAAArrrrAAAAAAAAa..
    ....cCCCCCCCCcaAAAAAAAArrAAAAAAAAAAa.
    ...EeCEeCCCCCcaAAAAAAAAAAAAAAAAAAAa..
    ...cEECEECCCCcaaAAAAAAAAAAAAAAAAAa...
    ...cCCCCCCCCc..aaAAAAAAAAAAAAAAaa....
    ....ff.cccc.....aaaAAAAAAAAAAaa......
    ....f..f..........aaaaaaaaaa.........
  ]], legs = sideLegs }
  local downLegs = {}
  for i, yd in ipairs{ { 11, -10 }, { 12, -11 }, { 13, -11 }, { 14, -8 } } do
    local k = i - 1
    downLegs[#downLegs + 1] = Leg(8, yd[1], yd[2], k % 2, { ky = -6, back = k < 2, c = k < 2 and "L" or "l", w = 2 })
    downLegs[#downLegs + 1] = Leg(17, yd[1], -yd[2], 1 - k % 2, { ky = -6, back = k < 2, c = k < 2 and "L" or "l", w = 2 })
  end
  local down = { body = sym([[
    .......gGg..gG
    .....gGWGGggGW
    ....gGGGGgGGGG
    ....aGgGGGGgGG
    ...aAAgggaaggg
    ..aAAhAAAAAAAA
    ..aAhAAAAAAArr
    ..aAAAAAAAAArr
    ..aAAAAAAAAAAr
    ...aAAAAAAAAAA
    ....aaAAAAAAAA
    ......aacCCCCC
    ......cCeECCeE
    .....cCCEeCCEe
    ......cCCCCCCC
    .......cCCCCCC
    ........f...f.
  ]], { h = "A" }), legs = downLegs }
  local upLegs = {}
  for i, L in ipairs(downLegs) do
    local M = {}
    for k, v in pairs(L) do M[k] = v end
    M.y = L.y - 9
    upLegs[i] = M
  end
  local up = { body = sym([[
    ........cCCCCC
    .......cCCCCCC
    ........cCCCCC
    .....aaAAAAAAA
    ....aAAhAAAgGG
    ...aAAAAAgGWGG
    ...aAAAAgGGGGg
    ..aAAAAAgGGgGG
    ..aAAAAgGWGGGW
    ..aAAAAAgGGGgG
    ..aAAAAAAgggAg
    ...aAAAAAAAAAA
    ....aAAAAAAAAA
    .....aaaaaaaaa
    ..............
  ]], { h = "A" }), legs = upLegs }
  sheet(cr, { down = down, left = left, up = up })
end)

-- Thornback Boar: dark-furred, a ridge of bramble along its spine, long tusks and a red glare
creature("thornback", function()
  local cr = { name = "thornback", p = pal{ o = "1a0c0a", B = "6a3a2a", b = "4a2418", d = "34180f", D = "26100a", h = "8e5a40",
    H = "b07a5a", k = "120806", n = "c87878", N = "e0a0a0", s = "f6ecd8", e = "ff3a2a", r = "8a1a10", V = "5a9a3a",
    v = "2e5e2a", t = "e8e0b0" } }
  local left = { body = grid[[
    ..................t....t.....t....t.......
    .................tV...tV....tV...tV.......
    ...........bb...vVVv.vVVv..vVVv.vVVv......
    ..........bBhb.vVvvVvVvvVvvVvvVvVvvVv.....
    .........bBHhbbvvBBvvBBvvBBvvBBvvBBvvt....
    ........bBhBBBBBBBBBBBBBBBBBBBBBBBBBBVv...
    ......bbBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBvt..
    .....bBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBb..
    ....bBBBreBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBb.
    ...bBBBBrrBBBBBBBhBBBBBBBBBBBBBBBBBBBBBBdt
    .nnnBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBdt
    nNNNnBBBBBBBBBhBBBBBBBBBBBBBBBBBBBBBBBBddt
    nNkNnbBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBddd.
    nNNNnbbBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBddd..
    .nnnsbbbBBBBBBBBBBBBBBBBBBBBBBBBBBBbddd...
    ..sssbbbbdBBBBBBBBBBBBBBBBBBBBBBBbdddd....
    ..s.ss....bbdBBBBBBBBBBBBBBBBBBbddd.......
    .s.s.........ddddddddddddddddddd..........
    ..........................................
    ..........................................
    ..........................................
    ..........................................
  ]], legs = { Leg(12, 16, 0, 1, { c = "D", w = 4, foot = "k", s = 2, back = true }), Leg(29, 16, 0, 0, { c = "D", w = 4, foot = "k", s = 2, back = true }),
    Leg(9, 16, 0, 0, { c = "d", w = 4, foot = "k", s = 2 }), Leg(26, 16, 0, 1, { c = "d", w = 4, foot = "k", s = 2 }) } }
  local down = { body = sym([[
    .....t.......t
    .....Vt.....tV
    ..bb.vV.t..vVV
    .bBBbvVvVvvVvv
    .bBhBbvvVvvvvv
    .bBBBBBBBBBBBB
    ..bBBBBBBBBBBB
    ..bBhBBBBBBBBB
    ..BBreBBBBBBBB
    ..BBrrBBBBBBBB
    ..BBBBBBBBBnnn
    .sBBBBBBBnNNNN
    .sBBBBBBnNNNNN
    .ssBBBBBnNkNNN
    ..sbBBBBnNNNNN
    ...sbbbbbnnnnn
    .......ddddddd
    ..............
    ..............
    ..............
  ]], { h = "B", H = "h" }), legs = { Leg(6, 16, 0, 0, { c = "d", w = 4, foot = "k", s = 2 }), Leg(17, 16, 0, 1, { c = "d", w = 4, foot = "k", s = 2 }) } }
  local up = { body = sym([[
    .....bb......t
    ....bBBb....tV
    ....bBBBb.t.vV
    ...bBBBBBtV.vv
    ..bBBhBBBvVvVv
    ..bBBBBBvvVVvv
    .bBBBBBBBvvVVv
    .bBhBBBBBvVvvv
    .bBBBBBBBBvVVv
    .bBBBBBBBBvvVv
    .bdBBBBBBBBvvv
    .bdBBBBBBBBBBB
    ..bdBBBBBBBBBB
    ...bddBBBBBBBB
    .....dddddddtt
    ..............
    ..............
    ..............
  ]], { h = "B" }), legs = { Leg(6, 13, 0, 0, { c = "d", w = 4, foot = "k", s = 2 }), Leg(17, 13, 0, 1, { c = "d", w = 4, foot = "k", s = 2 }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

-- Rat King (final boss): a portly crowned rat in a red ermine-trimmed robe
creature("rat_king", function()
  local cr = { name = "rat_king", p = pal{ o = "2a1e26", G = "9aa3ab", g = "6e7880", D = "4c545c", h = "c4ccd2", p = "f08aa0",
    P = "c05a78", k = "1a1014", e = "ffffff", Y = "ffd84a", y = "c89020", r = "ff3050", C = "c0283a", c = "8a1a2a",
    W = "fff8f0", w = "d8d0c8", t = "9a3a5a" } }
  local down = { body = sym([[
    .......Y...Y
    .......YY.YY
    .......YYYrY
    ..ggg..yyyyy
    .gPPPgDGGGGG
    .gPpPgGGGGGG
    .gPPgGGhGGGG
    ..ggGGhGGGGG
    ....GGGkeGGG
    ....GGGkkGGG
    ....GGGGGGGh
    .....GGGGGpp
    .....gGGGGpp
    ......gGGGGG
    ....CCWWWWWW
    ...CCWkWWkWW
    ..CCCCWWWWWW
    ..CcCCCGGGGG
    .CCcCCGGhhGG
    .CCcCGGGGGGG
    .CCcCGGGGGGG
    .CcCCgGGGGGG
    .CcCCCgGGGGG
    ..CcCCCggggg
    ...cccc.....
    ............
  ]], { h = "G", p = "P" }), legs = { Leg(8, 23, 0, 0, { c = "g", w = 3, foot = "P", s = 2 }), Leg(14, 23, 0, 1, { c = "g", w = 3, foot = "P", s = 2 }) } }
  local left = { body = grid[[
    ..........Y.Y.Y..........
    ..........YYYYY..........
    ..........YrYYY..ggg.....
    ..........yyyyy.gPPPg....
    ........GGGGGGGGgPpPg....
    ......GGhGGGGGGGgPPg.....
    .....GhGGGGGGGGGGgg......
    ....GGGGkeGGGGGGGGG......
    ..GGGGGGkkGGGGGGGGG......
    ppGGGGGGGGGGGGGGGGg......
    ppGGGGGGGGGGGGGGGg.......
    .gGGGGGGGGGGGGGGg........
    ...gggGGGGGGGgg..........
    .......WWWWWWWCC.........
    ......WWkWWWWWCCC........
    ......GGWWWWWCCCCC.......
    ......GGGGGGCCCcCCC......
    .....gGGhGGGCCCcCCC......
    .....gGGGGGGCCCCcCCC.....
    .....gGGGGGGCCCCcCCC.....
    .....gGGGGGGCCCCCcCCC....
    ......gGGGGGCCCCCcCCCttt.
    .......gggg.cccccccc...t.
    .......................t.
  ]], legs = { Leg(7, 21, 0, 0, { c = "g", w = 3, foot = "P", s = 2 }), Leg(10, 21, 0, 1, { c = "D", w = 3, foot = "P", s = 2, back = true }) } }
  local up = { body = sym([[
    .......Y...Y
    .......YY.YY
    .......YYYYY
    ..ggg..yyyyy
    .gPPPgGGGGGG
    .gPPPgGGGGGG
    .gPPgGGGGGGG
    ..ggGGGGGGGG
    ....GGGGhGGG
    ....GGGGGGGG
    .....gGGGGGG
    ......gggggg
    ....CCCCCCCC
    ...CCCCCCCCC
    ..CCCcCCCCCC
    ..CCcCCCCCcC
    .CCCcCCCCCcC
    .CCcCCCCCCcC
    .CCcCCCCCcCC
    .CCcCCCCCcCC
    .CcCCCCCCcCC
    .WWWWWWWWWWW
    .WkWWWkWWWWk
    ..wwwwwwwwww
    ...........t
    ...........t
  ]], { h = "G" }), legs = { Leg(8, 23, 0, 0, { c = "g", w = 3, foot = "P", s = 2, back = true }), Leg(14, 23, 0, 1, { c = "g", w = 3, foot = "P", s = 2, back = true }) } }
  sheet(cr, { down = down, left = left, up = up })
end)

local only = app.params and app.params.only
local want = {}
if only and #only > 0 then for n in only:gmatch("[^,]+") do want[#want + 1] = n end else want = ORDER end
for _, n in ipairs(want) do (CREATURES[n] or error("no creature " .. n))() end
print("creatures ok")
