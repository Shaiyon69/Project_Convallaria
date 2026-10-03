-- Trees for world/tree.png, rendered by headless Aseprite. Run from the repo root:
--   aseprite -b --script tools/trees.lua
-- 48x72 cells drawn 1:1 in the world, one row per biome (BIOMES order), four variants
-- each. Shapes are laid out on a 32x48 design grid and sampled at 1.5x, lit from the top
-- left with a 2x2 dither. Each material is outlined in its own darkest shade, and there is
-- no ground shadow: drawWorld draws it so it stays put when the tree turns see-through.
-- Trunk base sits on row 68.

local W, H, S = 48, 72, 1.5

local function hex(s) return { tonumber(s:sub(1, 2), 16), tonumber(s:sub(3, 4), 16), tonumber(s:sub(5, 6), 16) } end
local function ramp(...)
  local r = {}
  for i, s in ipairs{ ... } do r[i] = hex(s) end
  return r
end

local LEAF = ramp("1e5a34", "2a7a3a", "3e9a3e", "6ac04a", "a8e06a")
local BLOSSOM = ramp("7a2a5a", "b04a7a", "e070a0", "f4a0c0", "ffe0ee")
local WILLOW = ramp("1a3a3a", "24584a", "3a7a5a", "5aa070", "98d098")
local DUSK = ramp("2a2448", "3e3a6a", "5a5a90", "8a84c0", "c0c0ea")
local CAP = ramp("4a1a40", "7a2a62", "b04a8c", "e080b8", "ffc8e4")
local STALK = ramp("8a7660", "b8a48c", "d8c8b0", "f0e6d2")
local FROND = ramp("164a32", "1e7044", "34a050", "6ccc5c", "b0f080")
local AUTUMN = ramp("5a1810", "a0341a", "d4601e", "f09a30", "ffd468")
local GOLDEN = ramp("5a3e10", "9a701a", "d0a42a", "f0d050", "fff2a8")
local PINE = ramp("0e2a26", "18443a", "286248", "46845a", "86b488")
local STONE = ramp("2e303a", "4a4c58", "6e7282", "9a9eb0", "c8ccd8")
local BARK = ramp("3e2414", "5a3820", "7a4e2c", "9a6a3c", "bc8c56")
local PALMBARK = ramp("4a3220", "6a4c2c", "8e6c40", "b08e58", "d0b078")
local MOSS = ramp("2e5a2e", "3e7a3e", "6ac04a")
local SNOW, WHITE, PINK, GOLD = hex("e8f0f8"), hex("ffffff"), hex("f4a0c0"), hex("ffd23a")
local BAYER = { { 0, 2 }, { 3, 1 } }

-- A cell: colour, material ramp and draw order per pixel (order picks which neighbour an
-- outline pixel takes its shade from: the one drawn last, i.e. in front).
local cell, layer
local function new()
  cell, layer = {}, 0
  for i = 0, W * H - 1 do cell[i] = false end
end
local function put(x, y, c, m)
  if x < 0 or y < 0 or x >= W or y >= H then return end
  cell[y * W + x] = { c = c, m = m, o = layer }
end
local function get(x, y)
  if x < 0 or y < 0 or x >= W or y >= H then return false end
  return cell[y * W + x]
end
local function nextLayer() layer = layer + 1 end

-- Ramp entry for a light level in 0..1, dithered.
local function shade(r, light, x, y)
    return r[math.max(1, math.min(#r, math.floor(light * #r) + 1))]
end

-- Calls f(x, y, u, v) for each output pixel, u and v being its centre on the design grid.
local function each(f)
  for y = 0, H - 1 do
    for x = 0, W - 1 do f(x, y, (x + 0.5) / S, (y + 0.5) / S) end
  end
end

local function hash(x, y) return ((x * 73856093) ~ (y * 19349663)) % 97 end

-- A lit bundle of foliage (or stone), shaded like a sphere lit from the top left in crisp
-- bands: a bright cap, light and mid bands, and a dark crescent on the bottom right. Leaf
-- bundles get a scalloped rim and a few two-pixel leaf marks; stone stays smooth.
local function ball(cx, cy, r, rmp, vmax, squash, bias)
  vmax, squash, bias = vmax or 48, squash or 1, bias or 0
  local leafy, phase = rmp ~= STONE, (cx * 1.7 + cy * 2.3) % 6.28
  nextLayer()
  each(function(x, y, u, v)
    local dx, dy = u - cx, (v - cy) / squash
    local d, a = math.sqrt(dx * dx + dy * dy), math.atan(dy, dx)
    local rim = leafy and r * (1 + 0.09 * math.sin(a * math.max(5, math.floor(r * 0.9)) + phase)) or r
    if d > rim or v > vmax then return end
    local hx, hy = dx + r * 0.32, dy + r * 0.38  -- from the highlight centre
    local lit, n = math.sqrt(hx * hx + hy * hy) / r, #rmp
    local i = lit < 0.3 and bias >= 0 and n or lit < 0.32 and n - 1 or lit < 0.72 and n - 1 or lit < 1.08 and n - 2 or n - 3
    if (dx + dy) / r > 0.45 and d > rim - 1.8 then i = n - 3 end
    if leafy then
      local h = hash(x, y)
      if h < 4 and i >= n - 2 then i = i + 1 elseif h < 7 and i == n - 2 then i = i - 1 end
    end
    i = math.max(1, math.min(n, i + bias))
    local under = get(x, y)
    if under and under.m == rmp and d > rim - 1 and dx + dy > -r * 0.2 then
      put(x, y, rmp[1], rmp)  -- a dark rim where this bundle overlaps the one behind
    else
      put(x, y, rmp[i], rmp)
    end
  end)
end

local function trunkX(top, base, lean, v) return 16 + lean * math.max(0, (base - v) / math.max(1, base - top)) ^ 1.6 * 6 end

-- Shaded trunk with a root flare and bark streaks; lean bends the top sideways by 6 * lean.
local function trunk(top, base, half, rmp, lean)
  base, half, rmp, lean = base or 45, half or 2, rmp or BARK, lean or 0
  nextLayer()
  each(function(x, y, u, v)
    if v < top or v > base + 1 then return end
    local h = half + (v >= base - 2 and 0.8 or 0) + (v >= base and 0.8 or 0)
    local cx = trunkX(top, base, lean, v)
    if math.abs(u - cx) < h then
      local t = (u - (cx - h)) / (2 * h)
      local i = t < 0.25 and 4 or t < 0.6 and 3 or 2
      if (x * 5 + math.floor(y / 3)) % 7 == 0 and i > 2 then i = i - 1 end  -- streaks
      put(x, y, rmp[i], rmp)
    end
  end)
end

-- Bark just under foliage sits in its shade.
local function canopyShade()
  for x = 0, W - 1 do
    for y = H - 1, 1, -1 do
      local p = get(x, y)
      if p and (p.m == BARK or p.m == PALMBARK) then
        for k = 1, 3 do
          local q = get(x, y - k)
          if q and q.m ~= p.m then
            local m = p.m
            for i = 2, #m do if m[i] == p.c then p.c = m[i - 1] break end end
            break
          end
        end
      end
    end
  end
end

-- A crown of similar leaf bundles in rows (two on top, three across, two below), over a
-- filler bundle, drawn back to front so each one's shaded crescent separates it.
local function canopy(cx, cy, size, rmp, n, vmax)
  local r, j = size * 0.5, function() return (math.random() - 0.5) * size * 0.12 end
  local clumps = { { cx, cy, size * 0.82, -1 } }
  local layout = { { -0.32, -0.6 }, { 0.36, -0.55 }, { -0.62, -0.02 }, { 0.02, -0.08 }, { 0.62, 0 }, { -0.36, 0.48 }, { 0.38, 0.5 } }
  for k = 1, math.min(#layout, n or 7) do
    local l = layout[k]
    -- the crown as a whole is lit too: bundles on its top left a band brighter, bottom right darker
    local b = -(l[1] + l[2]) * 1.1
    clumps[#clumps + 1] = { cx + l[1] * size + j(), cy + l[2] * size + j(), r * (0.9 + math.random() * 0.2), b > 0.5 and 1 or b < -0.5 and -1 or 0 }
  end
  for _, c in ipairs(clumps) do ball(c[1], c[2], c[3], rmp, vmax or 40, 1, c[4]) end
end

-- Small four-pixel blossoms on the foliage, design-grid spots.
local function flowers(spots, colors)
  for k, s in ipairs(spots) do
    local x, y = math.floor(s[1] * S), math.floor(s[2] * S)
    local p = get(x, y)
    if p then
      local c = colors[(k - 1) % #colors + 1]
      put(x, y, c, p.m)
      if get(x + 1, y) then put(x + 1, y, c, p.m) end
      if get(x, y + 1) then put(x, y + 1, p.m[1], p.m) end
    end
  end
end

local function spots(n, x0, x1, y0, y1)
  local t = {}
  for i = 1, n do t[i] = { math.random(x0, x1), math.random(y0, y1) } end
  return t
end

local function oak(seed, leaves, blooms)
  math.randomseed(seed)
  new()
  leaves = leaves or LEAF
  trunk(31)
  canopy(16, 24, 10.5, leaves)
  canopyShade()
  if blooms then flowers(spots(9, 7, 24, 15, 33), blooms) end
end

local function lilyTree(seed)
  oak(seed)
  for _, b in ipairs{ { 9, 25 }, { 21, 23 }, { 13, 32 }, { 23, 31 }, { 17, 20 }, { 11, 18 }, { 19, 28 } } do
    local x, y = math.floor(b[1] * S), math.floor(b[2] * S)
    local p = get(x, y)
    if p and get(x, y + 2) then  -- a bell: white crown, flared lip, stem above
      put(x, y - 1, LEAF[1], p.m)
      put(x, y, WHITE, p.m)
      put(x - 1, y + 1, hex("e8ecf8"), p.m); put(x, y + 1, WHITE, p.m); put(x + 1, y + 1, hex("c8cce0"), p.m)
    end
  end
end

local function bush(seed, leaves, blooms)
  math.randomseed(seed)
  new()
  local lumps = { { 16, 38, 7.5 } }
  for _ = 1, 5 do lumps[#lumps + 1] = { 16 + math.random() * 14 - 7, 38 + math.random() * 5 - 3, 4 + math.random() * 1.5 } end
  table.sort(lumps, function(a, b) return a[2] < b[2] end)
  for _, c in ipairs(lumps) do ball(c[1], c[2], c[3], leaves, 45.4) end
  if blooms then flowers(spots(8, 8, 23, 31, 42), blooms) end
end

local function willow(seed, leaves)
  math.randomseed(seed)
  new()
  trunk(26)
  canopy(16, 18, 9.5, leaves, 6, 30)
  nextLayer()
  for x = math.floor(5 * S), math.floor(27 * S) do  -- fronds hang from the dome, longest in the middle
    local top
    for y = 0, H - 1 do if get(x, y) and get(x, y).m == leaves then top = y break end end
    if top and (x * 7 + seed) % 4 ~= 0 then
      local len = math.floor((14 + 8 * (1 - math.abs(x / S - 16) / 12)) * S) + math.random(-4, 4)
      for y = top + 8, math.min(math.floor(43 * S), top + 8 + len) do
        local sway = (math.floor(y / 6) + x) % 5 == 0 and 1 or 0
        local i = (y - top) > len - 2 and 2 or ((x + y // 4) % 7 == 0 and 5 or 2 + (x + y // 4) % 3)
        put(math.min(W - 1, x + sway), y, leaves[i], leaves)
      end
    end
  end
  canopyShade()
end

local function mushroom()
  new()
  each(function(x, y, u, v)  -- stalk
    if v >= 28 and v < 46 then
      local h = 3 + (v >= 43 and 0.8 or 0)
      local d = u - (16 - h)
      if d >= 0 and d < 2 * h then put(x, y, STALK[d < h * 0.4 and 4 or d < h * 1.2 and 3 or 2], STALK) end
    end
  end)
  nextLayer()
  each(function(x, y, u, v)  -- cap: a wide dome
    local dx, dy = (u - 16) / 14, (v - 29) / 18
    if v >= 10 and v < 30 and dx * dx + dy * dy <= 1 then
      put(x, y, shade(CAP, 0.66 - dx * 0.25 + dy * 0.35 - (dx * dx + dy * dy) * 0.3, x, y), CAP)
    end
  end)
  for x = 0, W - 1 do  -- gill rim
    local p = get(x, math.floor(29.5 * S))
    if p and p.m == CAP then put(x, math.floor(29.5 * S) + 1, CAP[2], CAP) end
  end
  for _, s in ipairs{ { 9, 19, 2 }, { 18, 14, 2 }, { 23, 22, 1 }, { 13, 25, 1 }, { 15, 11, 1 }, { 25, 26, 1 } } do
    local x0, y0, n = math.floor(s[1] * S), math.floor(s[2] * S), math.floor(s[3] * S + 0.5)
    for y = y0, y0 + n - 1 do
      for x = x0, x0 + n do
        if get(x, y) then put(x, y, (x == x0 + n or y == y0 + n - 1) and hex("f0c8dc") or hex("fff4fa"), CAP) end
      end
    end
  end
end

local function palm(seed, lean, nuts)
  math.randomseed(seed)
  new()
  trunk(14, 45, 1.2, PALMBARK, lean)
  for y = math.floor(16 * S), math.floor(44 * S), 4 do  -- ring scars
    for x = 0, W - 1 do
      local p = get(x, y)
      if p and p.m == PALMBARK then put(x, y, PALMBARK[2], PALMBARK) end
    end
  end
  local tx, ty = trunkX(14, 45, lean, 14), 13
  for k = 0, 7 do  -- fronds arc out and droop
    nextLayer()
    local a = -math.pi / 2 + (k - 3.5) * 0.5 + (math.random() * 0.2 - 0.1)
    for s = 0, 28 do
      local t = s / 28
      local fx, fy = tx + math.cos(a) * 11.5 * t, ty + math.sin(a) * 7 * t + 11 * t * t
      local w = (1.4 * (1 - t) + 0.5) * S
      for y = math.floor(fy * S - w), math.floor(fy * S + w) do
        for x = math.floor(fx * S - w), math.floor(fx * S + w) do
          local ox, oy = x + 0.5 - fx * S, y + 0.5 - fy * S
          if ox * ox + oy * oy <= w * w then
            local i = math.floor(5 - t * 3 - oy / S * 1.2 - k % 2)
            if (x + y + s) % 9 == 0 then i = i - 1 end  -- leaflet notches
            put(x, y, FROND[math.max(1, math.min(5, i))], FROND)
          end
        end
      end
    end
  end
  if nuts then
    nextLayer()
    for _, o in ipairs{ { -2, 2 }, { 1, 3 }, { 3, 1 } } do
      local x, y = math.floor((tx + o[1]) * S), math.floor((ty + o[2]) * S)
      put(x, y, hex("8e5c32"), BARK); put(x + 1, y, hex("6a4424"), BARK)
      put(x, y + 1, hex("6a4424"), BARK); put(x + 1, y + 1, hex("4a2c18"), BARK)
    end
  end
  canopyShade()
end

local function deadTree(seed)
  math.randomseed(seed)
  new()
  trunk(24)
  nextLayer()
  local function branch(x, y, a, len, w)
    for s = 1, len * 2 do
      x, y = x + math.cos(a) / 2, y + math.sin(a) / 2
      local r = w * 0.55 * S
      for py = math.floor(y * S - r), math.floor(y * S + r) do
        for px = math.floor(x * S - r), math.floor(x * S + r) do
          local ox, oy = px + 0.5 - x * S, py + 0.5 - y * S
          if ox * ox + oy * oy <= r * r then put(px, py, BARK[(ox < 0 or oy < -r * 0.4) and 4 or 3], BARK) end
        end
      end
      if s == len and w > 1 then branch(x, y, a + (math.random() < 0.5 and -0.7 or 0.7), len // 2, w - 1) end
    end
  end
  branch(15.5, 27, -math.pi / 2, 14, 2)
  branch(15, 31, -math.pi / 2 - 0.9, 11, 2)
  branch(17, 29, -math.pi / 2 + 0.9, 11, 2)
  nextLayer()
  for k = 0, 11 do  -- a few last leaves clinging on
    local x, y = math.random(8, 40), math.random(12, 45)
    if get(x, y) then put(x, y - 1, AUTUMN[3 + k % 3], AUTUMN) end
  end
end

local function pine(seed, snow, short)
  new()
  trunk(36)
  local tiers = short and { { 40, 11 }, { 33, 9 }, { 26, 7 }, { 20, 4.5 } } or { { 39, 12.5 }, { 31, 10.5 }, { 23, 8.5 }, { 15, 6.5 }, { 8, 4 } }
  for _, tr in ipairs(tiers) do  -- skirts, upper ones overlap
    local base, half = tr[1], tr[2]
    local height = math.floor(half * 1.25) + 3
    nextLayer()
    each(function(x, y, u, v)
      if v < base - height or v > base + 1 then return end
      local t = (v - (base - height)) / height
      local w = half * t + 0.5
      local dx = u - 16
      if math.abs(dx) <= w then
        local c = shade(PINE, 0.66 - dx / (half * 2.4) - t * 0.3, x, y)
        put(x, y, c, PINE)
      end
    end)
    for x = 0, W - 1 do  -- ragged hem
      local y = math.floor((base + 1) * S)
      if math.abs(x / S - 16) <= half and (x + seed) % 3 == 0 and get(x, y - 1) then put(x, y, PINE[1], PINE) end
    end
  end
  if snow then  -- caps on whatever of each skirt shows under the one above, thicker on the lit side
    local caps = {}
    for y = 0, H - 1 do
      for x = 0, W - 1 do
        local p = get(x, y)
        if p and p.m == PINE then
          local n = x < 26 and 3 or 1
          for k = 1, n do
            local q = get(x, y - k)
            if not q or q.o > p.o then caps[#caps + 1] = { x, y, k == 1 } break end
          end
        end
      end
    end
    for _, c in ipairs(caps) do put(c[1], c[2], c[3] and SNOW or hex("b8c8d8"), PINE) end
  end
  canopyShade()
end

local function boulder(seed)
  math.randomseed(seed)
  new()
  for _, c in ipairs{ { 16, 37, 9.5 }, { 11, 39, 6 }, { 22, 40, 5.5 } } do ball(c[1], c[2], c[3], STONE, 45.4, 0.8) end
  nextLayer()
  for _ = 1, 70 do  -- moss on the crown
    local x, y = math.random(10, 37), math.random(40, 53)
    if get(x, y) and not get(x, y - 1) then put(x, y, MOSS[3], MOSS); put(x, y + 1, MOSS[2], MOSS); put(x, y + 2, MOSS[1], MOSS) end
  end
  for _, c in ipairs{ { 18, 47, 7, 0.7 }, { 30, 55, 4, -1.2 } } do  -- cracks
    local x, y = c[1], c[2]
    for _ = 1, c[3] do
      if get(x, y) then put(x, y, STONE[1], STONE) end
      x, y = x + (math.random() < 0.5 and 1 or 0) * (c[4] > 0 and 1 or -1), y + 1
    end
  end
end

-- Paints the current cell into img at (ox, oy), outlining each material in its own darkest shade.
local function blit(img, ox, oy)
  local px = function(c) return app.pixelColor.rgba(c[1], c[2], c[3], 255) end
  for y = 0, H - 1 do
    for x = 0, W - 1 do
      local p = get(x, y)
      if p then
        img:drawPixel(ox + x, oy + y, px(p.c))
      else
        local best
        for _, d in ipairs{ { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
          local q = get(x + d[1], y + d[2])
          if q and (not best or q.o > best.o) then best = q end
        end
        if best then
          local c = best.m[1]
          img:drawPixel(ox + x, oy + y, px{ math.max(0, c[1] - 20), math.max(0, c[2] - 20), math.max(0, c[3] - 20) })
        end
      end
    end
  end
end

local TREES = {
  { function() oak(7) end, function() lilyTree(20) end, function() oak(33, BLOSSOM) end, function() bush(5, LEAF, { PINK, WHITE }) end },  -- Meadow
  { function() willow(3, WILLOW) end, function() willow(11, DUSK) end, mushroom, function() bush(9, WILLOW, { hex("c080f0") }) end },  -- Marsh
  { function() palm(1, 0.5, true) end, function() palm(2, -0.6, false) end, function() palm(6, 0.15, true) end,
    function() bush(12, FROND, { hex("ff5060"), GOLD }) end },  -- Atoll
  { function() oak(8, AUTUMN) end, function() oak(15, GOLDEN) end, function() deadTree(3) end, function() bush(14, AUTUMN) end },  -- Rat King's isle
  { function() pine(1, false) end, function() pine(2, true) end, function() boulder(5) end, function() pine(9, false, true) end },  -- Highlands
}

local spr = Sprite(W * 4, H * #TREES, ColorMode.RGB)
local img = spr.cels[1].image
for row, variants in ipairs(TREES) do
  for col, draw in ipairs(variants) do
    draw()
    blit(img, (col - 1) * W, (row - 1) * H)
  end
end
spr:saveAs(app.fs.joinPath(app.fs.currentPath, "assets", "world/tree.png"))
spr:close()
