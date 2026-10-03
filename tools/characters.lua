-- Player walk sheets (assets/player/<name>.png), rendered by headless Aseprite. Run from the repo root:
--   aseprite -b --script tools/characters.lua
-- Every character is built on one of the two hand-drawn Godot bodies, Lily (player/woman.png)
-- or Shy (player/shy.png): their four walk frames per view are copied with a palette swap, then
-- accessory grids (hats, crowns, masks) are stamped on top and outlined. Frames 1 and 3 of
-- the bodies bob down a pixel, and so do the accessories. `at` is the cell pixel of a grid's
-- top left per view (D, L, R, U); without an R entry the right view mirrors the left one.
-- In a grid '.' is empty and other chars are keys of the character's `wear` palette.

local CELL = 48
local BASES = {
  lily = { file = "player/woman.png", cols = { 0, 1, 2, 3 } },
  shy = { file = "player/shy.png", cols = { 2, 3, 4, 5 } },
}

local pc = app.pixelColor
local function hex(s) return pc.rgba(tonumber(s:sub(1, 2), 16), tonumber(s:sub(3, 4), 16), tonumber(s:sub(5, 6), 16), 255) end

local function rows(s)
  local out = {}
  for line in s:gmatch("[^\n]+") do
    line = line:match("^%s*(.-)%s*$")
    if #line > 0 then out[#out + 1] = line end
  end
  return out
end
local function mirror(g)
  local out = {}
  for i, line in ipairs(g) do out[i] = line:reverse() end
  return out
end

-- Lily's palette: outline 7b382f; hair e1b975 ba895b fbe568; bow 9d466e c94d82 e275a4;
-- skin fff1c1 f6ad8a; eyes and shoes a96456; dress 044669 045886 04354e 036ca8.
-- Shy's: hair 6785d1 (also its outline) aebee8 d0e1f5; skin fff1c1 f6ad8a e0936e, outline d7835b;
-- eyes b393c2, blush d7aeb7; scarf c93c49 c94e51 ca2635; shirt dbdded c7ccf1 424b93, outline 373f80;
-- button f4d480; shoes 372031 3e2e3a.
local CHARACTERS = {
  {
    file = "ivy", base = "lily",  -- Bramble Patch: moss-green hair, a leaf crown, marigold clip, leather dress
    swap = { ["7b382f"] = "3a2414", e1b975 = "5aa84a", ba895b = "3a8035", fbe568 = "9ad86a",
      ["9d466e"] = "c8743a", c94d82 = "ffb03a", e275a4 = "ffe07a",
      ["044669"] = "6a4020", ["045886"] = "8a5a30", ["04354e"] = "4a2a14", ["036ca8"] = "a8743a" },
    wear = { x = "8ad85a", X = "4a9a3a", v = "7a4e2c" },
    items = {
      { at = { D = { 19, 13 }, L = { 19, 13 }, R = { 18, 13 }, U = { 19, 13 } }, ol = "3a2414", grid = [[
        .x...x...x.
        xXx.xXx.xXx
        .X...X...X.
        vvvvvvvvvvv
      ]] },
    },
  },
  {
    file = "rowan", base = "lily",  -- Woodcutter's Axe: copper hair, green headband knotted at the side, red flannel
    swap = { ["7b382f"] = "4a1a10", e1b975 = "e0703a", ba895b = "b0482a", fbe568 = "f8a050",
      ["9d466e"] = "2e6a28", c94d82 = "4a9a3a", e275a4 = "7ad05a",
      ["044669"] = "a82828", ["045886"] = "d03a34", ["04354e"] = "6a1a1a", ["036ca8"] = "f06a5a" },
    wear = { x = "4a9a3a", X = "2e6a28" },
    items = {
      { at = { D = { 18, 17 }, L = { 18, 17 }, R = { 16, 17 }, U = { 17, 17 } }, ol = "4a1a10",
        grids = {
          D = "XxxxxxxxxxxxX",
          L = "XxxxxxxxxxxX..\n...........xX.\n............X.",
          R = "..XxxxxxxxxxxX\n.Xx...........\n.X............",
          U = "XxxxxxxxxxxxX",
        } },
    },
  },
  {
    file = "thistle", base = "lily",  -- Thorn Orbit: a spiky thistle puff of pink hair, downy white clip, sage dress
    swap = { ["7b382f"] = "4a1e4a", e1b975 = "e070e0", ba895b = "b048c0", fbe568 = "f8b0f8",
      ["9d466e"] = "b0b0c0", c94d82 = "e0e0ea", e275a4 = "ffffff",
      ["044669"] = "4a7a4a", ["045886"] = "6a9a5a", ["04354e"] = "2e4a2e", ["036ca8"] = "8ac06a" },
    wear = { h = "f8b0f8", H = "e070e0", k = "b048c0" },
    items = {
      { at = { D = { 19, 11 }, L = { 19, 11 }, R = { 18, 11 }, U = { 19, 11 } }, ol = "4a1e4a", grid = [[
        ..h..h..h..
        .hHhhHhhHh.
        hHkHkHkHkHh
        .kHkkHkkHk.
      ]] },
    },
  },
  {
    file = "nyx", base = "shy",  -- Petal Blade: violet hair, crescent pin, night mask and dark garb, crimson scarf
    swap = { ["6785d1"] = "2e1a48", aebee8 = "5a3a7a", d0e1f5 = "8a5ab0",
      dbdded = "3a3a58", c7ccf1 = "4e4e70", ["424b93"] = "24243a", ["373f80"] = "141424", b393c2 = "e04060" },
    wear = { m = "24243a", M = "34344e", y = "ffe08a" },
    items = {
      { at = { D = { 20, 23 }, L = { 19, 23 }, R = { 21, 23 }, U = { 0, 0 } }, ol = "141424",
        grids = { D = "MMMMMMMM\n.mmmmmm.", L = "MMMMMMM.\n.mmmmm..", R = ".MMMMMMM\n..mmmmm.", U = "" } },
      { at = { D = { 26, 16 }, L = { 26, 17 }, R = { 20, 17 }, U = { 23, 16 } }, ol = "2e1a48",
        grids = { D = "yy\ny.\nyy", L = "yy\ny.\nyy", R = "yy\n.y\nyy", U = "yy\ny.\nyy" } },
    },
  },
  {
    file = "hemlock", base = "shy",  -- Poison Aura: crooked witch hat with a venom band, lilac hair, nightshade robe
    swap = { ["6785d1"] = "6a5498", aebee8 = "a088d0", d0e1f5 = "d4c0f0", c93c49 = "3a8035", c94e51 = "4a9a2a",
      ca2635 = "6ac04a", dbdded = "6a3a8a", c7ccf1 = "8a5aa8", ["424b93"] = "4a2868", ["373f80"] = "2a1a40",
      fff1c1 = "f4f4e8", b393c2 = "3a9a4a" },
    wear = { x = "3e523e", X = "263226", y = "9af05a" },
    items = {
      { at = { D = { 15, 7 }, L = { 15, 7 }, U = { 15, 7 } }, ol = "161c16", grid = [[
        ..........xX......
        .........xxX......
        ........xxxX......
        .......xxxxX......
        ......xxxxxX......
        ......xxxxxxX.....
        .....xxxxxxxX.....
        .....yyyyyyyy.....
        ....xxxxxxxxxX....
        XxxxxxxxxxxxxxxxxX
        .XXXXXXXXXXXXXXXX.
      ]] },
    },
  },
  {
    file = "hazel", base = "shy",  -- Seed Mortar: straw sun hat with a sprout, chestnut hair, cream shirt and overalls
    swap = { ["6785d1"] = "5a3418", aebee8 = "8a5a2e", d0e1f5 = "b07a42", c93c49 = "3a8035", c94e51 = "4a9a3a",
      ca2635 = "6ac04a", dbdded = "f0e2c4", c7ccf1 = "c89a5a", ["424b93"] = "a07840", ["373f80"] = "5a3e20",
      b393c2 = "6a4a2a" },
    wear = { x = "f4d878", X = "c8a040", r = "4aa03a", y = "7ad05a" },
    items = {
      { at = { D = { 14, 10 }, L = { 14, 10 }, U = { 14, 10 } }, ol = "6a4a18", grid = [[
        .........y.y........
        ......xxxxyxxX......
        ......xxxxxxxX......
        ......xxxxxxxX......
        ......rrrrrrrr......
        .xxxxxxxxxxxxxxxxxX.
        ..XXXXXXXXXXXXXXXX..
      ]] },
    },
  },
}

for _, ch in ipairs(CHARACTERS) do
  local base = BASES[ch.base]
  local src = Image{ fromFile = app.fs.joinPath(app.fs.currentPath, "assets", base.file) }
  local swap, wear = {}, {}
  for from, to in pairs(ch.swap) do swap[hex(from)] = hex(to) end
  for k, v in pairs(ch.wear) do wear[k] = hex(v) end
  local spr = Sprite(CELL * 4, CELL * 4, ColorMode.RGB)
  local img = spr.cels[1].image
  for row, view in ipairs{ "D", "L", "R", "U" } do
    for f = 0, 3 do
      local ox, oy = f * CELL, (row - 1) * CELL
      for y = 0, CELL - 1 do
        for x = 0, CELL - 1 do
          local c = src:getPixel(base.cols[f + 1] * CELL + x, (row - 1) * CELL + y)
          if pc.rgbaA(c) > 0 then img:drawPixel(ox + x, oy + y, swap[c] or c) end
        end
      end
      local bob = (f == 1 or f == 3) and 1 or 0
      for _, it in ipairs(ch.items) do
        local g, at
        if it.grids then
          g, at = rows(it.grids[view]), it.at[view]
        elseif view == "R" and not it.at.R then
          g = mirror(rows(it.grid))
          at = { CELL - it.at.L[1] - #g[1], it.at.L[2] }
        else
          g, at = rows(it.grid), it.at[view]
        end
        local placed = {}
        for r, line in ipairs(g) do
          for i = 1, #line do
            local k = line:sub(i, i)
            if k ~= "." then
              local x, y = at[1] + i - 1, at[2] + r - 1 + bob
              img:drawPixel(ox + x, oy + y, wear[k])
              placed[#placed + 1] = { x, y }
            end
          end
        end
        local ol = hex(it.ol)
        for _, p in ipairs(placed) do
          for _, d in ipairs{ { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } } do
            local x, y = p[1] + d[1], p[2] + d[2]
            if x >= 0 and y >= 0 and x < CELL and y < CELL and pc.rgbaA(img:getPixel(ox + x, oy + y)) == 0 then
              img:drawPixel(ox + x, oy + y, ol)
            end
          end
        end
      end
    end
  end
  spr:saveAs(app.fs.joinPath(app.fs.currentPath, "assets", "player/" .. ch.file .. ".png"))
  spr:close()
end
