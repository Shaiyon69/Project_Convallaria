<p align="center">
  <img src="assets/ui/lily.svg" alt="Convallaria lily emblem" width="96">
  <br>
  <img src="assets/ui/title.png" alt="Convallaria" width="384">
</p>

# Convallaria

Convallaria is a top-down survival action game written in C++ with [raylib](https://www.raylib.com/). You land on a procedurally generated island, fight off waves of enemies, collect experience, choose upgrades, and climb through four floors to a final boss.

## What You Do

- Survive enemy waves that grow stronger over time.
- Collect experience seeds to level up, then pick one of three upgrades: health, speed, damage, fire rate, area size, regeneration, thorns, dodge, crits, fire and frost effects, and more.
- Grab power-ups: magnet, speed boost, bomb, healing, silver and gold.
- Spend silver on chests scattered across each island. Every chest holds a random item from 28, and items stack without limit: attack speed, max HP, move speed, crit chance and damage, regen, life on hit, burning and freezing hits, chain lightning, exploding kills, blocking, shields, armor, pierce, extra projectiles, executes, a knockback sprinkler, extra gold and silver, rerolls and a one-time revive. Defeating a guardian leaves a free chest.
- Each character fights with their own weapon; level-ups make it stronger.
- Each floor rolls a different island: Meadow, Marsh, Atoll or Highlands, each with its own shape and enemy mix.
- Find the corrupted portal on each island and summon its guardian: the Floor Guardian, the Hive Queen, the Stone Colossus, the Brood Mother or the Thornback Boar, each with its own attacks. Defeat it to purify the portal and travel to the next floor.
- Reach the final island and defeat the Rat King.

## Between Runs

Gold you collect is banked when a run ends (death, victory, or quitting the run), and a victory adds 1000 more. Spend it in the shop:

- Buy permanent upgrades: base health, damage, movement speed, HP regen, thorns, dodge, coin multiplier and EXP gain.
- Choose your character (each has their own weapon, a perk and a trade-off).
- Review your base stats.

Your lifetime kills, elite kills, chests, guardians, best floor, wins and banked gold unlock new characters and legendary items. Hover a locked one in the shop to see what it needs.
- Respec to refund every coin you spent on upgrades.

Progress is saved to `%APPDATA%/Convallaria/save.txt` on Windows (`~/Convallaria/save.txt` elsewhere).

Weapons (Lily: Storm Lily, Shy: Wand, Ivy: Bramble Patch, Rowan: Woodcutter's Axe, Nyx: Petal Blade, Hemlock: Poison Aura, Thistle: Thorn Orbit, Hazel: Seed Mortar):

- Wand: fires magic projectiles at the nearest enemy, with splash, pierce, bounce and multishot upgrades.
- Poison Aura: damages every enemy around you.
- Thorn Orbit: blades circle you and cut whatever they touch.
- Petal Blade: slashes a wide arc at the nearest enemy.
- Woodcutter's Axe: thrown out and back, cutting through everything on both ways.
- Seed Mortar: lobs seed pods that burst where they land.
- Storm Lily: lightning that jumps between enemies.
- Bramble Patch: grows thorny patches that tear at and slow enemies.

Enemy types include slimes, beetles, mushrooms, boars, bees, golems, hares, ratmen, leaping toads, spiders that split into spiderlings, slime-trailing snails, swooping crows and burrowing moles. Any of them can spawn as an elite (swift, giant, blazing, frost or gilded), more often on later floors. Horde events hit at set times. Stay on a floor past ten minutes, or after its guardian falls, and death slimes start pouring in.

## Controls

- `W` `A` `S` `D` or arrow keys: move
- `E`: open chests and use portals
- `1` `2` `3` or mouse: pick an upgrade
- `Esc`: pause (then `Q` to end the run and bank your gold)

## Building

You need CMake 3.20+ and a C++20 compiler. The first configure downloads raylib 5.5.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/convallaria          # ./build/convallaria --test runs the self-check
```

The executable loads its art and audio from `assets/` in this repository.

## Not Ported Yet

The original Godot version (see git history before the C++ port) also had statues, JSON mod support, and Android touch controls. These are next on the list.
