-- Hand-placed weapon art, rendered by headless Aseprite. Run from the repo root:
--   aseprite -b --script tools/weapons.lua
-- Grids are drawn pixel by pixel in the Godot icon style: 16x16, each material
-- outlined in its own darkest shade (no auto outline), light from the top-left.
-- '.' is empty, every other char is a palette key.

local function rgb(hex)
  return Color{ r = tonumber(hex:sub(1, 2), 16), g = tonumber(hex:sub(3, 4), 16), b = tonumber(hex:sub(5, 6), 16), a = 255 }
end

local function pal(t)
  local p = {}
  for k, v in pairs(t) do p[k] = rgb(v) end
  return p
end

local function save(spr, path)
  spr:saveAs(app.fs.joinPath(app.fs.currentPath, "assets", path))
  spr:close()
end

local function icon(path, rows, p)
  local spr = Sprite(16, 16, ColorMode.RGB)
  local img = spr.cels[1].image
  for y, row in ipairs(rows) do
    for x = 1, #row do
      local c = row:sub(x, x)
      if c ~= "." then img:drawPixel(x - 1, y - 1, p[c]) end
    end
  end
  save(spr, path)
end

-- Shared ramps, taken from the Godot axe/sword/wand icons.
local GLASS = { o = "3f5763", G = "9badb7", W = "d2eaf7" }
local WOOD = { c = "7b3d3a", k = "a96456", K = "d9916c" }
-- Poison: magenta like the Godot aura ring, hue-shifted (cool shadows, warm highlights).
local VENOM = { p = "4a1240", m = "8a2a7a", l = "c04aa8", h = "f08ad8" }

local function merge(...)
  local t = {}
  for _, s in ipairs{ ... } do for k, v in pairs(s) do t[k] = v end end
  return pal(t)
end

icon("weapons/poison/poison.png", {
  "................",
  "......cccc......",
  "......cKkc......",
  ".....occcco.....",
  "......oWGo......",
  "......oWGo......",
  ".....ooWGoo.....",
  "...ooWGGGGGoo...",
  "..oWWGGGlGGGGo..",
  "..oWlllllllllo..",
  ".ohmmmmmmmmmmpo.",
  ".ohmmmmmmlmmmpo.",
  ".olmmmmmmmmmppo.",
  "..opmmmmmmmppo..",
  "...oppppppppo...",
  "....oooooooo....",
}, merge(GLASS, WOOD, VENOM))

-- Aura: four 48x48 frames side by side, stretched to the weapon radius and drawn
-- translucent. Bright rim, dithered falloff inward that shimmers, bubbles that
-- rise, swell and pop.
do
  local p = pal(VENOM)
  local spr = Sprite(48 * 4, 48, ColorMode.RGB)
  local img = spr.cels[1].image
  -- x, y, radius, phase: each bubble is on frame (f + phase) % 4 of its life, the last one a pop.
  local bubbles = { { 14, 18, 2, 0 }, { 31, 14, 1, 1 }, { 33, 32, 2, 2 }, { 17, 35, 1, 3 }, { 25, 24, 1, 1 }, { 22, 12, 1, 2 } }
  for f = 0, 3 do
    for y = 0, 47 do
      for x = 0, 47 do
        local d = math.sqrt((x - 23.5) ^ 2 + (y - 23.5) ^ 2)
        local c
        if d >= 24 then c = nil
        elseif d >= 23 then c = p.m
        elseif d >= 22 then c = p.l
        elseif d >= 20 then c = (x + y + f) % 2 == 0 and p.m or nil
        elseif d >= 17 then c = ((x + f // 2) % 2 == 0 and (y + f // 2) % 2 == 0) and p.p or nil
        end
        for _, b in ipairs(bubbles) do
          local age = (f + b[4]) % 4
          local by = b[2] - age * 2
          local bd = math.sqrt((x - b[1]) ^ 2 + (y - by) ^ 2)
          if age < 3 then
            if bd <= b[3] + age * 0.5 + 0.5 then c = (x < b[1] or y < by) and p.h or p.l end
          elseif math.abs(x - b[1]) == math.abs(y - by) and math.abs(x - b[1]) == b[3] + 1 then
            c = p.h  -- pop: four specks flung diagonally
          end
        end
        if c then img:drawPixel(f * 48 + x, y, c) end
      end
    end
  end
  save(spr, "weapons/poison/poison_radius..png")
end

-- Wand bolt: four 16x16 frames side by side, drawn rotated along its flight.
-- The Godot glow ball stays as the core; a sparkle flips between + and x.
do
  local p = pal{ c = "fefeed", b = "fdecbc", a = "f4d480", d = "c08a40" }
  local core = {
    ".......aa.......",
    "......abba......",
    ".....abccba.....",
    ".....abccba.....",
    "......abba......",
    ".......aa.......",
  }
  local spr = Sprite(16 * 4, 16, ColorMode.RGB)
  local img = spr.cels[1].image
  local function px(f, x, y, c) img:drawPixel(f * 16 + x, y, p[c]) end
  for f = 0, 3 do
    for y, row in ipairs(core) do
      for x = 1, #row do
        local c = row:sub(x, x)
        if c ~= "." then px(f, x - 1, y + 4, c) end
      end
    end
    local len = (f == 0 or f == 3) and 3 or 2
    local ramp = { "b", "a", "d" }
    if f % 2 == 0 then  -- + rays, two pixels wide like the core
      for k = 1, len do
        local c = ramp[k + 3 - len]
        for _, o in ipairs{ 7, 8 } do
          px(f, o, 5 - k, c); px(f, o, 10 + k, c); px(f, 5 - k, o, c); px(f, 10 + k, o, c)
        end
      end
    else  -- x rays off the core's cut corners
      for k = 0, len - 1 do
        local c = ramp[k + 4 - len]
        px(f, 5 - k, 5 - k, c); px(f, 10 + k, 5 - k, c); px(f, 5 - k, 10 + k, c); px(f, 10 + k, 10 + k, c)
      end
    end
  end
  save(spr, "weapons/wand/projectile.png")
end

-- Seed Mortar: a split pea pod, peas showing. Its shot is one pea.
local POD = { g = "24561e", G = "5bc23f", L = "a8e06a", P = "8ad04a", p = "c8f08a", W = "f4ffe0" }
icon("weapons/mortar/mortar.png", {
  "................",
  "............cc..",
  "...........cKc..",
  "..........gGc...",
  ".........gGLg...",
  "........gGpPg...",
  ".......gLPPpg...",
  "......gGpPGg....",
  ".....gLPPpg.....",
  "....gGpPGg......",
  "...gLPPpg.......",
  "..gGGGGg........",
  ".gGGLgg.........",
  ".ggg............",
  "................",
  "................",
}, merge(POD, WOOD))
icon("weapons/mortar/pod.png", {
  "................",
  "................",
  "................",
  "................",
  "......gggg......",
  ".....gPPpPg.....",
  "....gPpWPPPg....",
  "....gPWPPPPg....",
  "....gPPPPPPg....",
  "....gPPPPPgg....",
  ".....gPPPgg.....",
  "......gggg......",
  "................",
  "................",
  "................",
  "................",
}, pal(POD))

-- Storm Lily: a white lily on its stem, a bolt of lightning beside it.
local LILY = { s = "6a7a9a", W = "ffffff", w = "d8e0f0", y = "ffd23a", g = "24561e", G = "5bc23f", Y = "fff07a", B = "c8a01c" }
icon("weapons/lily/lily.png", {
  "................",
  "....s.....s.....",
  "...sWs...sWs....",
  "...sWWs.sWWs....",
  "....sWWsWWs.....",
  ".....swywws.....",
  "....sWWyWWs.....",
  "...sWWsysWWs....",
  "....ss.g.ss.....",
  "......gG...BB...",
  "......gG..BYB...",
  ".....gG..BYB....",
  ".....gG...BYB...",
  "....gG...BYB....",
  "....gG...BB.....",
  "................",
}, pal(LILY))

-- Bramble Patch: a curl of thorny vine with a berry.
local BRAMBLE = { v = "3a1e24", V = "8a4040", t = "e8e0b0", l = "3a8a2a", L = "7ad04a", r = "6a1020", R = "c82a4a", h = "ff9ab0" }
icon("weapons/bramble/bramble.png", {
  "................",
  "......vvvv......",
  "....vvVVVVvv....",
  "...vVtvvvvtVv...",
  "...vVv....vVv...",
  "..tvVv....vVvt..",
  "...vVv..llvVv...",
  "...vVvvlLLvVv...",
  "....vVVvlvVv....",
  "..rr.vvVVVvt....",
  ".rRRr..vVv......",
  ".rRhr..vVvt.....",
  "..rr..tvVv......",
  "......vVv.......",
  "......vv........",
  "................",
}, pal(BRAMBLE))

-- The patch it grows: four 32x32 frames of a ring of vines whose thorns stir,
-- with sprigs inside. Stretched to the patch radius.
do
  local p = pal(BRAMBLE)
  local spr = Sprite(32 * 4, 32, ColorMode.RGB)
  local img = spr.cels[1].image
  local function px(f, x, y, c)
    x, y = math.floor(x + 0.5), math.floor(y + 0.5)
    if x >= 0 and x < 32 and y >= 0 and y < 32 then img:drawPixel(f * 32 + x, y, p[c]) end
  end
  for f = 0, 3 do
    for i = 0, 179 do  -- the vine ring, lumpy and wavering
      local a = i * math.pi / 90
      local r = 12 + 1.3 * math.sin(a * 5 + f * math.pi / 2)
      px(f, 15.5 + math.cos(a) * r, 15.5 + math.sin(a) * r, "v")
      px(f, 15.5 + math.cos(a) * (r - 1), 15.5 + math.sin(a) * (r - 1), i % 9 < 5 and "V" or "v")
    end
    for k = 0, 11 do  -- thorns point out, alternating in and out of view
      local a = k * math.pi / 6 + f * 0.13
      local r = 12 + 1.3 * math.sin(a * 5 + f * math.pi / 2)
      if (k + f) % 2 == 0 then
        px(f, 15.5 + math.cos(a) * (r + 1), 15.5 + math.sin(a) * (r + 1), "t")
        px(f, 15.5 + math.cos(a) * (r + 2), 15.5 + math.sin(a) * (r + 2), "t")
      else
        px(f, 15.5 + math.cos(a) * (r - 2), 15.5 + math.sin(a) * (r - 2), "t")
      end
    end
    for k = 0, 4 do  -- sprigs with a leaf, swaying
      local a = k * 2 * math.pi / 5 + 0.4
      local bx, by = 15.5 + math.cos(a) * 6, 15.5 + math.sin(a) * 6
      local sway = (f % 2 == 0) and 1 or -1
      px(f, bx, by, "v"); px(f, bx, by - 1, "V"); px(f, bx + sway, by - 2, "V")
      px(f, bx + sway, by - 3, "L"); px(f, bx + sway * 2, by - 3, "l")
    end
    px(f, 15, 15, "R"); px(f, 16, 15, "R"); px(f, 15, 16, "r"); px(f, 16, 16, "R"); px(f, 15, 15, "h")
  end
  save(spr, "weapons/bramble/patch.png")
end
