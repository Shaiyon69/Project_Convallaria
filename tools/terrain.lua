-- Terrain autotiles world/grass.png and world/soil.png (11x5 tiles of 16 px, laid out as AUTOTILE
-- in main.cpp) and the 4-frame world/water.png, rendered by headless Aseprite. Run from the repo root:
--   aseprite -b --script tools/terrain.lua
-- A tile's shape comes from its 8-neighbour mask, quadrant by quadrant: a quadrant with both
-- sides joined is full (with a rounded notch if its corner is not), one joined side gives a
-- straight edge, none a rounded outer corner. Edges are straight so neighbours always meet, and
-- a bottom inner notch is as tall as the cliff so it carries on. Grass facing south drops into a short earth
-- cliff with foam at its foot; soil sits on the grass with a dark rim. Lit from the top left.
-- The game tints all three per biome, so the colours stay fairly light.

local T = 16
local MASKS = {
  { 7, 31, 28, 4, 5, 29, 23, 20, 21, 221, -1 },
  { 199, 255, 124, 68, 197, 253, 247, 116, 245, 119, -1 },
  { 193, 241, 112, 64, 71, 127, 223, 92, 95, 87, 93 },
  { 1, 17, 16, 0, 65, 113, 209, 80, 81, 213, 117 },
  { 255, 255, 255, 255, 69, 125, 215, 84, 85, -1, -1 },
}
local R, B, L, TOP = 1, 4, 16, 64
local BR, BL, TL, TR = 2, 8, 32, 128

local pc = app.pixelColor
local function hex(s, a) return pc.rgba(tonumber(s:sub(1, 2), 16), tonumber(s:sub(3, 4), 16), tonumber(s:sub(5, 6), 16), a or 255) end
local function hash(x, y, k) return ((x * 73856093) ~ (y * 19349663) ~ (k * 83492791)) % 1000 end

local GRASS = { hex("2c5e3a"), hex("3f8a40"), hex("56a845"), hex("74bf4f"), hex("a2d866") }
local CLIFF = { hex("3a2a2a"), hex("5e4030"), hex("8a5e3e"), hex("b07e50") }
local SOIL = { hex("5a3a26"), hex("8a5a36"), hex("b07a48"), hex("c89460"), hex("e0b880") }
local FLOWERS = { hex("fff4f8"), hex("ffd23a"), hex("f4a0c0"), hex("a8c8ff") }
local FOAM, SPRAY = hex("e8f8ff", 220), hex("e8f8ff", 110)

local function has(m, bit) return m & bit ~= 0 end

-- Inside test for pixel (x, y) of a tile with mask m; inset is how far an open side is pulled
-- in (top, side, bottom), ignoreBottom treats the bottom as joined (for the cliff below grass).
local function inside(m, x, y, inset, ignoreBottom)
  if x < 0 or y < 0 or x >= T or y >= T then return false end
  local left, up = x < 8, y < 8
  local hSide = has(m, left and L or R)
  local vSide
  if up then vSide = has(m, TOP) else vSide = ignoreBottom or has(m, B) end
  local corner = up and (left and TL or TR) or (left and BL or BR)
  local cornerOn = has(m, corner) or (not up and ignoreBottom and hSide)
  local ex, ey = left and x or T - 1 - x, up and y or T - 1 - y  -- distance to the tile's side / top or bottom
  local iH = inset.side
  local iV = up and inset.top or inset.bottom
  if hSide and vSide then
    return cornerOn or ex >= iH or ey >= iV
  elseif hSide then
    return ey >= iV
  elseif vSide then
    return ex >= iH
  end
  local i0, i1, rr = inset.side, up and inset.top or inset.bottom, 4
  if ex < i0 or ey < i1 then return false end
  local cx, cy = math.max(0, i0 + rr - ex - 0.5), math.max(0, i1 + rr - ey - 0.5)
  return cx * cx + cy * cy <= rr * rr
end

local GRASS_INSET = { top = 3, side = 3, bottom = 7 }
local SOIL_INSET = { top = 3, side = 3, bottom = 3 }

-- Soft "v" tufts of light blades, kept off the tile border so tiles meet cleanly.
local function tuft(x, y, variant)
  for _, o in ipairs({ { -1, 0 }, { 1, 0 }, { 0, 1 } }) do
    local cx, cy = x - o[1], y - o[2]
    if cx > 1 and cx < T - 2 and cy > 1 and cy < T - 3 and hash(cx, cy, variant) % 23 == 0 then return true end
  end
  return false
end

local function grassTile(img, ox, oy, m, variant)
  local g = {}
  for y = 0, T - 1 do
    g[y] = {}
    for x = 0, T - 1 do g[y][x] = inside(m, x, y, GRASS_INSET) end
  end
  local function G(x, y) return x < 0 or y < 0 or x >= T or y >= T or g[y][x] end  -- off-tile counts as joined
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local c
      if g[y][x] then
        local edge = not (G(x - 1, y) and G(x + 1, y) and G(x, y - 1) and G(x, y + 1))
        c = GRASS[3]
        if edge then
          c = GRASS[2]
        elseif not G(x, y - 2) then
          c = GRASS[4]  -- lit lip just inside north edges
        elseif tuft(x, y, variant) then
          c = GRASS[4]
        elseif hash(x, y, variant) < 25 then
          c = GRASS[2]
        end
        if variant > 0 and m == 255 and x > 1 and x < T - 2 and y > 1 and y < T - 2 and hash(x, y, variant * 7) < 6 then
          c = FLOWERS[(x + y + variant) % #FLOWERS + 1]
        end
      elseif inside(m, x, y, GRASS_INSET, true) and y >= 8 then
        -- the cliff below a south edge: a shadowed lip, then striped earth, foam at the foot
        local depth = 0
        while depth < 8 and not g[math.max(0, y - depth - 1)][x] do depth = depth + 1 end
        local foot = not inside(m, x, y + 1, GRASS_INSET, true) or y == T - 1
        if y >= T - 2 then
          c = y == T - 2 and FOAM or SPRAY
        elseif depth == 0 then
          c = CLIFF[1]
        elseif depth == 1 then
          c = CLIFF[2]
        else
          c = (x + (depth > 3 and 1 or 0)) % 3 == 0 and CLIFF[2] or (x % 3 == 1 and CLIFF[4] or CLIFF[3])
        end
        if foot and y < T - 2 then c = CLIFF[1] end
      end
      if c then img:drawPixel(ox + x, oy + y, c) end
    end
  end
end

local function soilTile(img, ox, oy, m, variant)
  local g = {}
  for y = 0, T - 1 do
    g[y] = {}
    for x = 0, T - 1 do g[y][x] = inside(m, x, y, SOIL_INSET) end
  end
  local function G(x, y) return x < 0 or y < 0 or x >= T or y >= T or g[y][x] end
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      if g[y][x] then
        local c = SOIL[3]
        local h = hash(x, y, variant + 11)
        if not (G(x - 1, y) and G(x + 1, y) and G(x, y - 1) and G(x, y + 1)) then
          c = SOIL[1]
        elseif not G(x, y - 2) or not G(x - 2, y) then
          c = SOIL[2]  -- the rim's inner shadow falls on the top and left, as the soil sits low
        elseif h < 90 then
          c = SOIL[4]
        elseif h < 130 then
          c = SOIL[2]
        end
        -- pebbles: a light stone with a dark underside, away from the tile border
        if x > 1 and x < T - 2 and y > 1 and y < T - 3 and h % 41 == 0 then
          img:drawPixel(ox + x, oy + y + 1, SOIL[2])
          c = SOIL[5]
        end
        img:drawPixel(ox + x, oy + y, c)
      end
    end
  end
end

local function atlas(name, draw)
  local spr = Sprite(T * 11, T * 5, ColorMode.RGB)
  local img = spr.cels[1].image
  for row, masks in ipairs(MASKS) do
    for col, m in ipairs(masks) do
      if m >= 0 then
        local variant = (m == 255 and row == 5) and col or 0  -- the four plain fill variants
        draw(img, (col - 1) * T, (row - 1) * T, m, variant)
      end
    end
  end
  spr:saveAs(app.fs.joinPath(app.fs.currentPath, "assets", name))
  spr:close()
end
atlas("world/grass.png", grassTile)
atlas("world/soil.png", soilTile)

-- Water: four frames of drifting ripples and glints, seamless across tiles.
local WATER = { hex("2a9cc4"), hex("36b6d6"), hex("6cd4ea"), hex("d0f8fc") }
local spr = Sprite(T * 4, T, ColorMode.RGB)
local img = spr.cels[1].image
for f = 0, 3 do
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local c = WATER[2]
      -- two families of short ripple dashes, sliding sideways a pixel per frame
      local a, b = (x + f * 2 + (y // 8) * 5) % 16, (x - f * 2 + (y // 8) * 3 + 8) % 16
      if y % 8 == 2 and a < 4 then c = WATER[3] end
      if y % 8 == 3 and a > 0 and a < 5 then c = WATER[1] end
      if y % 8 == 6 and b < 3 then c = WATER[3] end
      if (x * 7 + y * 13 + f * 5) % 61 == 0 then c = WATER[4] end  -- glints
      img:drawPixel(f * T + x, y, c)
    end
  end
end
spr:saveAs(app.fs.joinPath(app.fs.currentPath, "assets", "world/water.png"))
spr:close()
