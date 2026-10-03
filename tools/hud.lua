-- HUD pieces for ui/hud.png, rendered by headless Aseprite. Run from the repo root:
--   aseprite -b --script tools/hud.lua
-- Drawn at 2 UI units per pixel in the menu_buttons.png palette. Atlas, left to right:
--   bar   12x10  nine-slice, border 3: outlined wood rim around a dark trough
--   fill   6x8   one segment of a bar's fill, white so it takes the bar colour as tint
--   slot  16x16  nine-slice, border 5: inset cell for weapons, items and the portrait
--   cell   8x8   nine-slice, border 2: item cell, its 16 px centre fits an icon at 2x
--   ring   8x8   nine-slice, border 2: white ring laid over a cell, tinted by item tier
--   arrow  9x9   off-screen marker, points right
--   tip   12x12  nine-slice, border 3: tooltip box
--   skull 10x10  kill counter
-- '.' is empty, every other char is a palette key.

local function rgb(hex, a)
  return Color{ r = tonumber(hex:sub(1, 2), 16), g = tonumber(hex:sub(3, 4), 16), b = tonumber(hex:sub(5, 6), 16), a = a or 255 }
end

local P = {
  o = rgb("251821"), n = rgb("412e3b"), d = rgb("2a1e26"), w = rgb("663931"), W = rgb("8f563b"), h = rgb("b8784e"),
  t = rgb("1a1116", 225), T = rgb("0e080c", 235),
  ["1"] = rgb("ffffff"), ["2"] = rgb("e0e0e0"), ["3"] = rgb("c4c4c4"), ["4"] = rgb("a0a0a0"), ["5"] = rgb("747474"),
  r = rgb("ffffff"), k = rgb("000000"),
}

-- { x in the atlas, rows }
local pieces = {
  { 0, {
    ".oooooooooo.",
    "ohhhhhhhhhWo",
    "ohooooooooWo",
    "ohoTTTTTTowo",
    "ohoTTTTTTowo",
    "ohoTTTTTTowo",
    "ohoTTTTTTowo",
    "oWoooooooowo",
    "oWwwwwwwwwwo",
    ".oooooooooo.",
  } },
  { 12, {
    "111112",
    "222223",
    "222223",
    "222223",
    "333334",
    "333334",
    "444445",
    "555555",
  } },
  { 18, {
    ".oooooooooooooo.",
    "ohhhhhhhhhhhhhWo",
    "ohWWWWWWWWWWWwwo",
    "ohWoooooooooowwo",
    "ohWoTTTTTTTTowwo",
    "ohWoTttttttnowwo",
    "ohWoTttttttnowwo",
    "ohWoTttttttnowwo",
    "ohWoTttttttnowwo",
    "ohWoTttttttnowwo",
    "ohWoTttttttnowwo",
    "ohWoTnnnnnnnowwo",
    "ohWoooooooooowwo",
    "oWwwwwwwwwwwwwwo",
    "owwwwwwwwwwwwwwo",
    ".oooooooooooooo.",
  } },
  { 34, {
    ".oooooo.",
    "oTTTTTTo",
    "oTttttTo",
    "oTttttTo",
    "oTttttTo",
    "oTttttTo",
    "oTTTTTTo",
    ".oooooo.",
  } },
  { 42, {
    "........",
    ".rrrrrr.",
    ".r....r.",
    ".r....r.",
    ".r....r.",
    ".r....r.",
    ".rrrrrr.",
    "........",
  } },
  { 50, {
    "kk.......",
    "k1kk.....",
    "k111kk...",
    "k11111kk.",
    "k1111111k",
    "k33333kk.",
    "k333kk...",
    "k3kk.....",
    "kk.......",
  } },
  { 59, {
    ".oooooooooo.",
    "onnnnnnnnndo",
    "onoooooooodo",
    "onoTTTTTTodo",
    "onoTTTTTTodo",
    "onoTTTTTTodo",
    "onoTTTTTTodo",
    "onoTTTTTTodo",
    "onoTTTTTTodo",
    "odoooooooodo",
    "oddddddddddo",
    ".oooooooooo.",
  } },
  { 71, {
    "..kkkkkk..",
    ".k111111k.",
    "k11111112k",
    "k1kk11kk2k",
    "k1kk11kk2k",
    "k11112113k",
    ".k111113k.",
    "..k1k1kk..",
    "..k3k3k...",
    "...kkk....",
  } },
}

local spr = Sprite(81, 16, ColorMode.RGB)
local img = spr.cels[1].image
for _, piece in ipairs(pieces) do
  for y, row in ipairs(piece[2]) do
    for x = 1, #row do
      local c = row:sub(x, x)
      if c ~= "." then img:drawPixel(piece[1] + x - 1, y - 1, P[c]) end
    end
  end
end
spr:saveAs(app.fs.joinPath(app.fs.currentPath, "assets", "ui/hud.png"))
spr:close()
