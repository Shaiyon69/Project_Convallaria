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
- Spend silver on chests scattered across each island. Every chest holds a random item, and items stack without limit: attack speed, max HP, move speed, crit, regen, life on hit, burning hits, chain lightning, exploding kills, blocking, extra projectiles and executes. Defeating a guardian leaves a free chest.
- Each floor rolls a different island: Meadow, Marsh, Atoll or Highlands, each with its own shape and enemy mix.
- Find the corrupted portal on each island and summon its guardian. Defeat it to purify the portal and travel to the next floor.
- Reach the final island and defeat the Rat King.

## Between Runs

Gold you collect is banked when a run ends (death, victory, or quitting the run), and a victory adds 1000 more. Spend it in the shop:

- Buy permanent upgrades: base health, damage, movement speed, HP regen, thorns, dodge, coin multiplier and EXP gain.
- Choose your starting weapon.
- Review your base stats.
- Respec to refund every coin you spent on upgrades.

Progress is saved to `%APPDATA%/Convallaria/save.txt` on Windows (`~/Convallaria/save.txt` elsewhere).

Weapons:

- Wand: fires magic projectiles at the nearest enemy, with splash, pierce, bounce and multishot upgrades.
- Poison Aura: damages every enemy around you.

Enemy types include slimes, runners, shooters, brutes, swarms, dashers, tanks and ratmen. Horde events hit at set times. Stay on a floor past ten minutes, or after its guardian falls, and death slimes start pouring in.

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

The original Godot version (see git history before the C++ port) also had statues, an options menu, JSON mod support, and Android touch controls. These are next on the list.
