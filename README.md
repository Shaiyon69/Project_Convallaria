<p align="center">
  <img src="mods/ExampleMod/assets/example_lily.svg" alt="Convallaria lily emblem" width="96">
  <br>
  <img src="ui/title.png" alt="Convallaria" width="384">
</p>

# Convallaria

Convallaria is a top-down survival action game made in Godot. You enter a shifting island map, fight off waves of enemies, collect experience, choose upgrades, and push through multiple floors toward a final boss encounter.

The game is built around short survival runs with roguelite progression. Each run starts from the menu shop, where you can choose a starting weapon and spend earned coins on permanent upgrades before heading back into the field.

## What You Do

- Survive enemy waves while they grow stronger over time.
- Collect experience seeds to level up during a run.
- Pick from random upgrades that improve health, speed, damage, fire rate, area size, regeneration, dodge, critical hits, elemental effects, and more.
- Find items such as Apple, Sprinkler, Beanie, and Goldfish for special passive effects.
- Earn silver during runs and coins for long-term progression.
- Travel through floor portals and keep climbing until the final boss.

## Combat And Progression

You do not need to aim most attacks manually. Choose a weapon, keep moving, manage space, and build your character through upgrade choices.

Current weapons include:

- Wand: fires magic projectiles.
- Poison Aura: damages enemies around you.

Enemy types include basic slimes, fast runners, shooters, brutes, swarm enemies, dashers, tanks, ratmen, and bosses. If you stay too long after a stage timer ends, dangerous death slimes begin spawning and the pressure ramps up quickly.

## Controls

Keyboard:

- `W`, `A`, `S`, `D`: Move
- `E`: Interact

The project also includes mobile touch controls, so Android builds can be played with an on-screen joystick and buttons.

## Between Runs

Before starting a run, use the shop to:

- Buy permanent upgrades.
- Choose your starting weapon.
- Review your base stats.
- Respec upgrades and refund spent coins.

Permanent upgrades can improve base health, damage, speed, regeneration, armor/thorns, evasion, and experience gain.

## Current Build Notes

Convallaria currently includes:

- Procedural island-style maps with different floor biomes.
- Horde events at set survival times.
- Chests, statues, portals, and pickups.
- A pause menu, options menu, game over screen, level-up choices, item popups, and victory flow.
- Data-driven enemies, items, upgrades, stage settings, and example mod support.
- Android export assets in `builds/`.

## Running The Project

Open the folder in Godot 4.7 or newer and run the main scene. The configured main scene is the game menu, and starting from there will take you into the pre-run shop before a new run begins.
