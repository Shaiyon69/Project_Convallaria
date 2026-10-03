// Game data tables, ported from core/data.gd and data/*.json.
#pragma once
#include <raylib.h>

#include <span>

constexpr unsigned char ch(float v) { return (unsigned char)((v > 1.f ? 1.f : v) * 255.f); }
constexpr Color rgb(float r, float g, float b) { return Color{ch(r), ch(g), ch(b), 255}; }

enum EnemyType { BASIC, RATMAN, SHOOTER, BRUTE, RUNNER, SWARM, TANK, DASHER, DEATH_SLIME, TOAD, SPIDER, SPIDERLING, SNAIL, CROW, MOLE, ENEMY_TYPE_COUNT };

// Signature moves. Lunges wind up (drawn grey) inside `range`, then dash `time` seconds at
// speed x `mult` toward where the player was; the slam is a shockwave of SLAM_RADIUS instead.
// A leap is a lunge that lands with a LEAP_RADIUS shockwave; a burrower tunnels unseen and
// surfaces beside you in a BURROW_RADIUS burst; a trail drops slime every cdMin..cdMax seconds.
enum Attack { ATK_NONE, ATK_SHOOT, ATK_DASH, ATK_CHARGE, ATK_STING, ATK_SLAM, ATK_LEAP, ATK_SWOOP, ATK_BURROW, ATK_TRAIL };
struct AttackDef { float range, windup, time, mult, cdMin, cdMax; };
constexpr AttackDef ATTACKS[] = {
    {0, 0, 0, 0, 0, 0},
    {350, 0, 0, 0, 4.5f, 7},       // shroom spore shot
    {220, 0.5f, 0.35f, 5, 2.5f, 4},  // hare hop-lunge
    {280, 0.8f, 0.6f, 5.5f, 4, 6},   // boar charge
    {90, 0.3f, 0.2f, 3.5f, 1.5f, 2.5f},  // bee sting dive
    {60, 0.9f, 0, 0, 3.5f, 5},       // golem ground slam
    {200, 0.6f, 0.45f, 3.5f, 2.5f, 4},  // toad leap
    {260, 0.45f, 0.5f, 4, 2.5f, 4},  // crow swoop
    {50, 0.7f, 0, 0, 3, 4},          // mole: surfaces after the wind-up
    {0, 0, 0, 0, 0.5f, 0.7f},        // snail slime trail
};
constexpr float SLAM_RADIUS = 72, LEAP_RADIUS = 34, BURROW_RADIUS = 44;

// How each one dies; every death flashes white and bursts into `gib`-coloured bits first.
enum Death { DIE_SLIME, DIE_RAT, DIE_POP, DIE_TOPPLE, DIE_FLIP, DIE_FALL, DIE_CRUMBLE, DIE_SPORES, DIE_SPLIT };  // split: into spiderlings

struct EnemyDef {
    const char* id;
    int health;
    float speed, scale;
    Color color;
    int damage, exp;
    float pitch;
    Attack attack;
    Death death;
    Color gib;
    bool ratman, deathSlime;
    const char* sprite;  // assets/enemies/<sprite>.png walk sheet; null draws the tinted slime
    bool flies = false;  // ignores terrain and trees
};

constexpr EnemyDef ENEMIES[ENEMY_TYPE_COUNT] = {
    {"basic", 30, 55.f, 1.0f, rgb(1, 1, 1), 8, 10, 1.0f, ATK_NONE, DIE_SLIME, rgb(0.16f, 0.8f, 0.87f), false, false, nullptr},
    {"ratman", 20, 95.f, 1.0f, rgb(1, 1, 1), 9, 15, 1.2f, ATK_NONE, DIE_RAT, rgb(0.6f, 0.68f, 0.72f), true, false, nullptr},
    {"shooter", 20, 60.f, 0.9f, rgb(1, 1, 1), 8, 20, 1.4f, ATK_SHOOT, DIE_SPORES, rgb(1, 0.81f, 0.23f), false, false, "shroom"},
    {"brute", 90, 38.f, 1.5f, rgb(1, 1, 1), 20, 30, 0.6f, ATK_CHARGE, DIE_TOPPLE, rgb(0.71f, 0.33f, 0.24f), false, false, "boar"},
    {"runner", 15, 120.f, 0.8f, rgb(1, 1, 1), 6, 15, 1.5f, ATK_NONE, DIE_FLIP, rgb(0.36f, 0.76f, 0.25f), false, false, "beetle"},
    {"swarm", 5, 105.f, 0.5f, rgb(1, 1, 1), 3, 5, 1.8f, ATK_STING, DIE_FALL, rgb(1, 0.82f, 0.23f), false, false, "bee"},
    {"tank", 300, 26.f, 2.0f, rgb(1, 1, 1), 32, 100, 0.4f, ATK_SLAM, DIE_CRUMBLE, rgb(0.6f, 0.64f, 0.66f), false, false, "golem"},
    {"dasher", 25, 55.f, 0.9f, rgb(1, 1, 1), 12, 25, 1.3f, ATK_DASH, DIE_POP, rgb(0.37f, 0.84f, 0.82f), false, false, "hare"},
    {"death_slime", 15000, 250.f, 3.0f, rgb(0.367f, 0, 0.367f), 500, 0, 0.2f, ATK_NONE, DIE_SLIME, rgb(0.5f, 0.1f, 0.5f), false, true, nullptr},
    {"toad", 45, 50.f, 1.0f, rgb(1, 1, 1), 12, 18, 0.9f, ATK_LEAP, DIE_FLIP, rgb(0.45f, 0.7f, 0.3f), false, false, "toad"},
    {"spider", 28, 100.f, 0.9f, rgb(1, 1, 1), 10, 20, 1.3f, ATK_NONE, DIE_SPLIT, rgb(0.4f, 0.32f, 0.45f), false, false, "spider"},
    {"spiderling", 6, 125.f, 0.5f, rgb(1, 1, 1), 4, 3, 1.9f, ATK_NONE, DIE_POP, rgb(0.4f, 0.32f, 0.45f), false, false, "spider"},
    {"snail", 120, 24.f, 1.2f, rgb(1, 1, 1), 14, 30, 0.6f, ATK_TRAIL, DIE_POP, rgb(0.85f, 0.6f, 0.35f), false, false, "snail"},
    {"crow", 18, 85.f, 0.9f, rgb(1, 1, 1), 7, 15, 1.6f, ATK_SWOOP, DIE_FALL, rgb(0.25f, 0.25f, 0.35f), false, false, "crow", true},
    {"mole", 50, 70.f, 1.0f, rgb(1, 1, 1), 16, 25, 0.8f, ATK_BURROW, DIE_TOPPLE, rgb(0.45f, 0.35f, 0.3f), false, false, "mole"},
};

// Elites: any regular spawn may roll one. Blazing burns on touch and bursts into flame,
// frost chills on touch, gilded drop gold and silver.
enum Variant { V_NONE, V_SWIFT, V_GIANT, V_BLAZING, V_FROST, V_GILDED, VARIANT_COUNT };
struct VariantDef { const char* name; Color glow; float hp, damage, speed, scale, exp; };
constexpr VariantDef VARIANTS[VARIANT_COUNT] = {
    {"", rgb(1, 1, 1), 1, 1, 1, 1, 1},
    {"Swift", rgb(0.4f, 0.9f, 1), 0.8f, 1, 1.35f, 0.9f, 2},
    {"Giant", rgb(0.95f, 0.55f, 0.3f), 3, 1.5f, 0.8f, 1.45f, 3},
    {"Blazing", rgb(1, 0.45f, 0.1f), 1.8f, 1.2f, 1, 1.1f, 3},
    {"Frost", rgb(0.55f, 0.85f, 1), 1.8f, 1, 1, 1.1f, 3},
    {"Gilded", rgb(1, 0.85f, 0.2f), 2.5f, 1, 1.05f, 1.15f, 2},
};
constexpr float ELITE_BASE = 0.02f, ELITE_PER_FLOOR = 0.03f, ELITE_PER_MIN = 0.01f, ELITE_MAX = 0.3f;

// Enemy scaling. Damage grows gently so contact stays survivable deep into a run,
// and chase speed is capped below the player's base 165 so you can always outrun a pack.
constexpr float ENEMY_DMG_PER_FLOOR = 0.4f, ENEMY_DMG_PER_MIN = 0.06f;
constexpr float ENEMY_HP_PER_FLOOR = 1.1f;  // player damage snowballs, so later floors need much tougher enemies
constexpr float ENEMY_SPEED_PER_FLOOR = 8, ENEMY_SPEED_PER_MIN = 1.5f, ENEMY_SPEED_CAP = 145;
constexpr float ENEMY_SHOT_SPEED = 210;  // slow enough to sidestep

struct SpawnChance { EnemyType type; float base, growth; };
constexpr SpawnChance MEADOW_SPAWNS[] = {
    {BASIC, 100, 0}, {RATMAN, 10, 15}, {RUNNER, 0, 25}, {SHOOTER, 0, 12}, {SWARM, 0, 15}, {BRUTE, 0, 10},
    {DASHER, 0, 8}, {TANK, 0, 3}, {TOAD, 0, 8}, {CROW, 0, 6}, {MOLE, 0, 5},
};
constexpr SpawnChance MARSH_SPAWNS[] = {
    {BASIC, 100, 0}, {SWARM, 20, 30}, {RUNNER, 10, 30}, {RATMAN, 10, 10}, {DASHER, 0, 10}, {BRUTE, 0, 5},
    {TOAD, 10, 20}, {SNAIL, 5, 10}, {SPIDER, 0, 8},
};
constexpr SpawnChance ATOLL_SPAWNS[] = {
    {BASIC, 100, 0}, {SHOOTER, 5, 25}, {RUNNER, 0, 20}, {DASHER, 5, 12}, {RATMAN, 10, 10}, {TANK, 0, 3},
    {CROW, 5, 15}, {SNAIL, 0, 8}, {TOAD, 0, 10},
};
constexpr SpawnChance HIGHLAND_SPAWNS[] = {
    {BASIC, 100, 0}, {RATMAN, 10, 15}, {BRUTE, 10, 20}, {SHOOTER, 0, 15}, {DASHER, 0, 8}, {TANK, 0, 6},
    {SPIDER, 0, 12}, {MOLE, 0, 10}, {CROW, 0, 10},
};

struct HordeEvent { int second; EnemyType type; int amount; };
constexpr HordeEvent HORDES[] = {
    {60, SWARM, 30}, {120, BRUTE, 10}, {180, DASHER, 25}, {240, SPIDER, 15}, {300, TANK, 5}, {390, CROW, 30}, {450, SWARM, 60},
};

// data/stage_settings.json, data/pickup_settings.json
constexpr float STAGE_DURATION = 600.f;
constexpr float DEATH_SLIME_BASE_INTERVAL = 5.f, DEATH_SLIME_MIN_INTERVAL = 0.1f;
constexpr float DEATH_SLIME_INTERVAL_MULT = 0.9f, DEATH_SLIME_GROWTH_SECONDS = 10.f;
constexpr float PICKUP_DESPAWN = 25.f, PICKUP_WARNING = 5.f, HEAL_DROP_CHANCE = 0.025f;
constexpr int HEAL_AMOUNT = 35;

// Level-up picks. `weight` sets how often one is offered; `flat` ones ignore rarity
// (uniques are always flat). Text: "%s" is the rolled value.
struct Upgrade { const char* id; const char* name; const char* text; float base; bool unique; int weight; };
constexpr Upgrade UPGRADES[] = {
    {"max_hp", "Vitality", "+%s%% Max HP", 12, false, 10},
    {"speed", "Swiftness", "+%s%% Move Speed", 6, false, 7},
    {"damage", "Might", "+%s%% Damage", 10, false, 10},
    {"fire_rate", "Haste", "+%s%% Attack Speed", 8, false, 10},
    {"aoe_size", "Bloom", "+%s%% Weapon Area", 10, false, 8},
    {"regeneration", "Renewal", "+%s HP Regen", 1, false, 8},
    {"thorns", "Bramble Skin", "Reflect %s%% Contact Damage", 40, false, 5},
    {"evasion", "Wisp Step", "+%s%% Dodge", 4, false, 6},
    {"crit_chance", "Keen Eye", "+%s%% Crit Chance", 4, false, 8},
    {"exp_boost", "Wisdom", "+%s%% EXP", 15, false, 6},
    {"multi_attack", "Twin Strike", "+1 Projectile, All Weapons", 1, false, 2},
    {"glass_cannon", "Glass Cannon", "+35% Damage, -20% Max HP", 35, true, 3},
    {"heavy_armor", "Heavy Armor", "+50% Thorns, +10% HP, -15% Speed", 50, true, 3},
    {"berserker", "Berserker", "+25% Attack Speed, -10% Dodge", 25, true, 3},
    {"vampiric_edge", "Vampiric Edge", "%s%% Chance Kills Heal 5%", 3, true, 3},
    {"magnet_training", "Magnetism", "+%s%% Pickup Range", 30, true, 3},
    {"precision", "Precision", "+%s%% Crit and Damage", 8, true, 3},
    {"momentum", "Momentum", "+%s%% Speed and Attack Speed", 6, true, 3},
    {"soul_harvest", "Soul Harvest", "+%s%% EXP, +1 HP Regen", 15, true, 3},
    {"fire_imbue", "Ember Touch", "Hits Burn", 0, true, 2},
    {"frost_imbue", "Frost Touch", "Hits Slow", 0, true, 2},
    {"pierce", "Piercing Thorns", "+1 Pierce, All Projectiles", 1, false, 2},
    {"armor", "Barkskin", "-%s%% Damage Taken", 5, false, 6},
    {"greed", "Fortune", "+%s%% Gold", 15, false, 5},
    {"second_wind", "Second Wind", "Revive Once at 50% HP", 0, true, 2},
    {"overgrowth", "Overgrowth", "+%s%% Weapon Area, -10% Attack Speed", 30, true, 3},
    {"static_charge", "Static Charge", "%s%% Chance Hits Chain Lightning", 10, true, 3},
    {"executioner", "Executioner", "+%s%% Damage to Enemies Under Half HP", 50, true, 3},
};
constexpr int MULTI_ATTACK = 10, PIERCE = 21;  // whole projectiles / pierces, never scaled by rarity

struct Rarity { const char* name; Color color; float mult; int weight; };
constexpr Rarity RARITIES[] = {
    {"Common", rgb(1, 1, 1), 1.0f, 62},
    {"Uncommon", rgb(0.2f, 0.8f, 0.2f), 1.4f, 25},
    {"Rare", rgb(0.2f, 0.5f, 1), 1.8f, 10},
    {"Legendary", rgb(1, 0.8f, 0.1f), 2.5f, 3},
};
constexpr Color UNIQUE_COLOR = rgb(0.75f, 0.4f, 1), WEAPON_COLOR = rgb(1, 0.6f, 0.2f);

// Silver chests get pricier with every one bought this run, so items trickle in.
constexpr int CHEST_BASE = 10, CHEST_PER_FLOOR = 8;
constexpr float CHEST_GROWTH = 0.25f;

// Every level grants these on top of the pick.
constexpr float LEVEL_HP = 5, LEVEL_DAMAGE = 0.02f;

enum WeaponId { WAND, POISON_AURA, ORBIT, SWORD, AXE, MORTAR, LILY, BRAMBLE, WEAPON_COUNT };
// Orbit: wait is the per-enemy hit cooldown, speed the spin in radians per second.
// Sword: projectiles are slashes (alternating sides). Axe: speed is the throw speed.
// Lily: projectiles are lightning jumps after the first strike. Bramble: damage per tick.
struct WeaponLevel { int damage; float wait; int projectiles; float speed; float scale; };
constexpr const char* WEAPON_NAMES[WEAPON_COUNT] = {"Wand", "Poison Aura", "Thorn Orbit", "Petal Blade", "Woodcutter's Axe", "Seed Mortar", "Storm Lily", "Bramble Patch"};
constexpr const char* WEAPON_DESC[WEAPON_COUNT] = {
    "Fires bursting bolts at the nearest enemy.", "Poisons everything around you in pulses.", "Thorns circle you, cutting what they touch.",
    "Slashes a wide arc at the nearest enemy.", "Throws an axe that cuts through everything, out and back.", "Lobs seed pods that burst on landing.",
    "Calls lightning that jumps between enemies.", "Grows thorny patches that tear and slow enemies.",
};
constexpr WeaponLevel WEAPON_LEVELS[WEAPON_COUNT][3] = {
    {{20, 1.0f, 1, 400, 1}, {35, 0.8f, 1, 450, 1}, {55, 0.5f, 1, 550, 1}},
    {{18, 1.0f, 0, 0, 1.0f}, {30, 0.8f, 0, 0, 1.25f}, {46, 0.55f, 0, 0, 1.6f}},
    {{16, 0.5f, 2, 3, 1.0f}, {26, 0.45f, 3, 3.5f, 1.1f}, {40, 0.35f, 4, 4, 1.2f}},
    {{34, 0.9f, 1, 0, 1.0f}, {55, 0.8f, 1, 0, 1.15f}, {85, 0.65f, 2, 0, 1.3f}},
    {{30, 1.6f, 1, 260, 1.0f}, {48, 1.4f, 1, 280, 1.1f}, {75, 1.1f, 2, 300, 1.2f}},
    {{36, 1.8f, 1, 0, 1.0f}, {58, 1.5f, 1, 0, 1.2f}, {90, 1.25f, 2, 0, 1.4f}},
    {{26, 1.2f, 3, 0, 1.0f}, {40, 1.0f, 4, 0, 1.0f}, {62, 0.8f, 6, 0, 1.0f}},
    {{13, 1.8f, 1, 0, 1.0f}, {21, 1.5f, 2, 0, 1.2f}, {33, 1.2f, 3, 0, 1.4f}},
};

// Items: bought from chests with silver, stacked without limit (Risk of Rain style).
enum ItemId { IT_QUILL, IT_BARK, IT_BOOTS, IT_CLOVER, IT_SPROUT, IT_LEECH,
              IT_EMBER, IT_BELL, IT_SPORES, IT_CHARM, IT_CROWN, IT_SICKLE,
              IT_APPLE, IT_WHEAT, IT_BEANIE, IT_CORN, IT_TOMATO, IT_CAN, IT_KINDLING,
              IT_PUMPKIN, IT_HOE, IT_LURE, IT_STRAW, IT_SAPPHIRE, IT_CARP,
              IT_SPRINKLER, IT_KOI, IT_GOLDFISH, ITEM_COUNT };
struct ItemDef { const char* name; const char* desc; int tier; const char* icon; };  // icon: assets/player/items/<icon>.png
constexpr ItemDef ITEMS[ITEM_COUNT] = {
    {"Quick Quill", "+12% attack speed", 0, "quill"},
    {"Oak Bark", "+30 max HP", 0, "bark"},
    {"Fleet Boots", "+10% move speed", 0, "boots"},
    {"Four-Leaf Clover", "+8% crit chance", 0, "clover"},
    {"Green Sprout", "+1.5 HP regen", 0, "sprout"},
    {"Leech Seed", "Hits heal 1 HP", 0, "leech"},
    {"Ember Stone", "15% chance on hit to burn", 1, "ember"},
    {"Storm Bell", "20% chance on hit to zap 3 enemies (+2 per stack)", 1, "bell"},
    {"Volatile Spores", "Kills explode (bigger per stack)", 1, "spores"},
    {"Ward Charm", "15% chance to block a hit (stacks with falloff)", 1, "charm"},
    {"Lily Crown", "+1 projectile and orb", 2, "crown"},
    {"Reaper's Sickle", "Execute enemies under 13% HP (stacks with falloff)", 2, "sickle"},
    {"Crisp Apple", "Level ups heal 8% max HP", 0, "apple"},
    {"Golden Wheat", "+10% EXP", 0, "wheat"},
    {"Knit Beanie", "+20% pickup range", 0, "beanie"},
    {"Corn Kernel", "Projectiles pierce +1 enemy", 0, "corn"},
    {"Ripe Tomato", "Kills have a 2% chance to drop healing", 0, "tomato"},
    {"Watering Can", "+8% weapon area", 0, "water"},
    {"Kindling", "Burns deal +50% damage", 0, "wood"},
    {"Pumpkin Shell", "Blocks a hit every 12s (faster per stack)", 1, "pumpkin"},
    {"Rusty Hoe", "Crits deal +50% damage", 1, "hoe"},
    {"Lucky Lure", "+1 reroll now and every floor", 1, "fish"},
    {"Straw Hat", "Take 10% less damage (stacks with falloff)", 1, "straw"},
    {"Frost Sapphire", "8% chance on hit to freeze (stacks with falloff)", 1, "blue"},
    {"Old Carp", "Kills have a 3% chance to drop silver", 1, "grayfish"},
    {"Sprinkler", "Every 4s a spray knocks back, slows and hurts nearby enemies", 2, "sprinkler"},
    {"Koi of Fortune", "+50% gold, +1 silver per pickup", 2, "silverfish"},
    {"Golden Goldfish", "Revive once at 50% HP (used up)", 2, "goldfish"},
};
// Level-up card art for each of UPGRADES, borrowed from the item icons.
constexpr ItemId UPGRADE_ICONS[] = {
    IT_BARK, IT_BOOTS, IT_HOE, IT_QUILL, IT_CAN, IT_SPROUT, IT_PUMPKIN, IT_CHARM, IT_CLOVER, IT_WHEAT, IT_CROWN,
    IT_SAPPHIRE, IT_STRAW, IT_EMBER, IT_LEECH, IT_BEANIE, IT_CLOVER, IT_BOOTS, IT_APPLE, IT_EMBER, IT_SAPPHIRE,
    IT_CORN, IT_STRAW, IT_KOI, IT_GOLDFISH, IT_SPRINKLER, IT_BELL, IT_SICKLE,
};
static_assert(std::size(UPGRADE_ICONS) == std::size(UPGRADES));

struct ItemTier { const char* name; Color color; int weight; };
constexpr ItemTier ITEM_TIERS[] = {
    {"Common", rgb(1, 1, 1), 70},
    {"Uncommon", rgb(0.2f, 0.8f, 0.2f), 25},
    {"Legendary", rgb(1, 0.3f, 0.2f), 5},
};

// Playable characters: walk sheets in assets/player/, picked in the shop; each has a small perk
// and its own weapon, the only one it ever holds.
struct CharacterDef { const char* name; const char* sprite; const char* perk; WeaponId weapon; };
constexpr CharacterDef CHARACTERS[] = {
    {"Lily", "woman", "+10% EXP, +1 reroll", LILY},
    {"Shy", "shy_walk", "+15% move speed, +30% pickup range, -10% max HP", WAND},
    {"Ivy", "ivy", "+20% max HP, +1.5 HP regen, -8% move speed", BRAMBLE},
    {"Rowan", "rowan", "+15% damage, +10% weapon area, -10% attack speed", AXE},
    {"Nyx", "nyx", "+8% crit chance, +50% crit damage, -15% max HP", SWORD},
    {"Hemlock", "hemlock", "+20% weapon area, +10% armor, -10% move speed", POISON_AURA},
    {"Thistle", "thistle", "+25% thorns, +10% evasion, -10% damage", ORBIT},
    {"Hazel", "hazel", "+25% gold, +10% attack speed, -10% max HP", MORTAR},
};
constexpr int CHARACTER_COUNT = int(std::size(CHARACTERS));

constexpr int MAX_FLOORS = 4;

// Floor boss base stats (the "boss" entry of data/enemies.json).
constexpr int BOSS_HEALTH = 5000, BOSS_DAMAGE = 60;
constexpr float BOSS_SPEED = 35.f;

struct Biome { Color grass, water, soil; };
constexpr Biome BIOMES[] = {
    {rgb(1, 1, 1), rgb(1, 1, 1), rgb(1, 1, 1)},
    {rgb(0.8f, 0.5f, 0.8f), rgb(0.9f, 0.2f, 0.5f), rgb(0.5f, 0.3f, 0.5f)},
    {rgb(0.5f, 0.8f, 1), rgb(0.2f, 0.5f, 1), rgb(0.3f, 0.5f, 0.8f)},
    {rgb(0.9f, 0.8f, 0.4f), rgb(0.9f, 0.5f, 0.2f), rgb(0.8f, 0.6f, 0.3f)},
    {rgb(0.4f, 0.4f, 0.4f), rgb(0.8f, 0.1f, 0.1f), rgb(0.2f, 0.2f, 0.2f)},
};

// Island recipes. Fbm noise times a falloff that peaks at `ring` (0 = disc,
// 0.6 = atoll with a lagoon); noise above `cut` is land.
struct MapConfig {
    const char* name;
    int radius;
    float noiseScale, cut, ring;
    int chests, biome;
    std::span<const SpawnChance> spawns;
};
constexpr MapConfig MAPS[] = {  // floor 1 is always the first; later floors pick from the rest
    {"Meadow", 100, 0.04f, 0.2f, 0, 10, 0, MEADOW_SPAWNS},
    {"Marsh", 100, 0.07f, 0.26f, 0, 10, 1, MARSH_SPAWNS},
    {"Atoll", 110, 0.05f, 0.15f, 0.6f, 10, 2, ATOLL_SPAWNS},
    {"Highlands", 120, 0.03f, 0.15f, 0, 12, 4, HIGHLAND_SPAWNS},
};
constexpr MapConfig FINAL_MAP = {"Rat King's Isle", 60, 0.04f, 0.2f, 0, 0, 3, MEADOW_SPAWNS};

// Floor guardians: the Godot one (sprite -1, coloured per floor by FLOOR_BOSSES) or a giant
// of a regular kind (its behaviour; `sheet` is its own look in enemies/) with its own pattern. hp and speed multiply the guardian base stats.
enum BossPattern { BP_GUARDIAN, BP_HIVE, BP_COLOSSUS, BP_BROOD, BP_BOAR };
struct BossDef { const char* name; BossPattern pattern; int sprite; const char* sheet; float scale, hp, speed; Color enraged, glow; };
constexpr BossDef BOSSES[] = {
    {"Floor Guardian", BP_GUARDIAN, -1, nullptr, 2, 1, 1, rgb(1, 1, 1), rgb(1, 1, 1)},
    {"Hive Queen", BP_HIVE, SWARM, "hive_queen", 3.2f, 0.85f, 1.3f, rgb(1, 0.6f, 0.3f), rgb(1, 0.9f, 0.3f)},
    {"Stone Colossus", BP_COLOSSUS, TANK, "colossus", 2.6f, 1.3f, 0.8f, rgb(0.9f, 0.4f, 0.3f), rgb(1, 0.6f, 0.2f)},
    {"Brood Mother", BP_BROOD, SPIDER, "brood_mother", 3.4f, 1, 1.1f, rgb(0.8f, 0.3f, 0.8f), rgb(1, 0.5f, 1)},
    {"Thornback Boar", BP_BOAR, BRUTE, "thornback", 2.6f, 1, 1, rgb(1, 0.3f, 0.2f), rgb(1, 0.6f, 0.3f)},
};
constexpr int BOSS_COUNT = int(std::size(BOSSES));

// Per-floor guardian look and phase-two buffs (enemies/boss.gd).
struct FloorBoss { const char* music; Color color, enragedColor, glow; float speedMult, damageMult, specialWait; };
constexpr FloorBoss FLOOR_BOSSES[] = {
    {"world/level1.mp3", rgb(0.6f, 0.5f, 0.4f), rgb(0.8f, 0.7f, 0.2f), rgb(1, 0.9f, 0.4f), 1.3f, 1.2f, 6.0f},
    {"world/level2.wav", rgb(0.4f, 0.2f, 0.6f), rgb(0.8f, 0.2f, 0.8f), rgb(1, 0.4f, 1), 1.5f, 1.3f, 4.5f},
    {"world/level3.mp3", rgb(0.8f, 0.2f, 0.2f), rgb(1, 0, 0), rgb(1, 0.4f, 0.2f), 1.8f, 1.4f, 3.5f},
};

// Permanent shop upgrades (core/data.gd permanent_upgrades).
enum PermId { PERM_MAX_HP, PERM_DAMAGE, PERM_SPEED, PERM_REGEN, PERM_ARMOR, PERM_EVASION, PERM_GREED, PERM_EXP_GAIN, PERM_COUNT };
struct PermUpgrade { const char* id; const char* name; int maxLevel, baseCost; float costMult, boost; };
constexpr PermUpgrade PERM_UPGRADES[PERM_COUNT] = {
    {"max_hp", "Base Health", 5, 100, 1.5f, 10},
    {"damage", "Base Damage", 5, 250, 2.0f, 0.05f},
    {"speed", "Movement Speed", 5, 150, 1.5f, 15},
    {"regeneration", "HP Regen", 3, 300, 2.5f, 0.5f},
    {"armor", "Thorns Armor", 3, 400, 2.0f, 0.1f},
    {"evasion", "Dodge Chance", 3, 500, 3.0f, 0.02f},
    {"greed", "Coin Multiplier", 3, 500, 3.0f, 0.2f},
    {"exp_gain", "EXP Gain %", 5, 200, 1.8f, 0.10f},
};

// Lifetime stats, banked with each run; unlocks are thresholds on them.
enum Stat { ST_KILLS, ST_ELITES, ST_CHESTS, ST_GUARDIANS, ST_FLOOR, ST_WINS, ST_GOLD, STAT_COUNT };
constexpr const char* STAT_IDS[STAT_COUNT] = {"kills", "elites", "chests", "guardians", "best_floor", "wins", "gold"};
enum UnlockKind { UL_CHARACTER, UL_ITEM };
struct UnlockDef { UnlockKind kind; int index; Stat stat; int need; const char* how; };
constexpr UnlockDef UNLOCKS[] = {
    {UL_CHARACTER, 2, ST_FLOOR, 3, "Reach floor 3"},
    {UL_CHARACTER, 3, ST_KILLS, 2000, "Slay 2000 enemies"},
    {UL_CHARACTER, 4, ST_WINS, 1, "Defeat the Rat King"},
    {UL_CHARACTER, 5, ST_ELITES, 40, "Slay 40 elite enemies"},
    {UL_CHARACTER, 6, ST_CHESTS, 10, "Open 10 chests"},
    {UL_CHARACTER, 7, ST_GOLD, 1500, "Bank 1500 gold in total"},
    {UL_ITEM, IT_SPRINKLER, ST_GUARDIANS, 1, "Defeat a floor guardian"},
    {UL_ITEM, IT_KOI, ST_GOLD, 3000, "Bank 3000 gold in total"},
    {UL_ITEM, IT_GOLDFISH, ST_FLOOR, 4, "Reach the Rat King's isle"},
    {UL_ITEM, IT_SAPPHIRE, ST_ELITES, 15, "Slay 15 elite enemies"},
};
