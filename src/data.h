// Game data tables, ported from core/data.gd and data/*.json.
#pragma once
#include <raylib.h>

#include <span>

constexpr unsigned char ch(float v) { return (unsigned char)((v > 1.f ? 1.f : v) * 255.f); }
constexpr Color rgb(float r, float g, float b) { return Color{ch(r), ch(g), ch(b), 255}; }

enum EnemyType { BASIC, RATMAN, SHOOTER, BRUTE, RUNNER, SWARM, TANK, DASHER, DEATH_SLIME, ENEMY_TYPE_COUNT };

struct EnemyDef {
    const char* id;
    int health;
    float speed, scale;
    Color color;
    int damage, exp;
    float pitch;
    bool shooter, ratman, deathSlime;
};

constexpr EnemyDef ENEMIES[ENEMY_TYPE_COUNT] = {
    {"basic", 30, 45.f, 1.0f, rgb(1, 1, 1), 10, 10, 1.0f, false, false, false},
    {"ratman", 20, 110.f, 1.0f, rgb(1, 1, 1), 12, 15, 1.2f, false, true, false},
    {"shooter", 20, 50.f, 0.9f, rgb(1, 0.831f, 0), 15, 20, 1.4f, true, false, false},
    {"brute", 90, 25.f, 1.5f, rgb(1, 0.4f, 0.4f), 25, 30, 0.6f, false, false, false},
    {"runner", 15, 85.f, 0.8f, rgb(0.2f, 0.9f, 0.2f), 5, 15, 1.5f, false, false, false},
    {"swarm", 5, 100.f, 0.5f, rgb(1, 0.5f, 0), 2, 5, 1.8f, false, false, false},
    {"tank", 300, 15.f, 2.0f, rgb(0.2f, 0.2f, 0.2f), 50, 100, 0.4f, false, false, false},
    {"dasher", 25, 40.f, 0.9f, rgb(0.1f, 0.8f, 0.8f), 15, 25, 1.3f, false, false, false},
    {"death_slime", 15000, 250.f, 3.0f, rgb(0.367f, 0, 0.367f), 500, 0, 0.2f, false, false, true},
};

struct SpawnChance { EnemyType type; float base, growth; };
constexpr SpawnChance MEADOW_SPAWNS[] = {
    {BASIC, 100, 0}, {RATMAN, 10, 15}, {RUNNER, 0, 25}, {SHOOTER, 0, 20},
    {SWARM, 0, 15}, {BRUTE, 0, 10}, {DASHER, 0, 8}, {TANK, 0, 3},
};
constexpr SpawnChance MARSH_SPAWNS[] = {
    {BASIC, 100, 0}, {SWARM, 20, 30}, {RUNNER, 10, 30}, {RATMAN, 10, 10}, {DASHER, 0, 10}, {BRUTE, 0, 5},
};
constexpr SpawnChance ATOLL_SPAWNS[] = {
    {BASIC, 100, 0}, {SHOOTER, 5, 25}, {RUNNER, 0, 20}, {DASHER, 5, 12}, {RATMAN, 10, 10}, {TANK, 0, 3},
};
constexpr SpawnChance HIGHLAND_SPAWNS[] = {
    {BASIC, 100, 0}, {RATMAN, 10, 15}, {BRUTE, 10, 20}, {SHOOTER, 0, 15}, {DASHER, 0, 8}, {TANK, 0, 6},
};

struct HordeEvent { int second; EnemyType type; int amount; };
constexpr HordeEvent HORDES[] = {
    {60, SWARM, 30}, {120, BRUTE, 15}, {180, DASHER, 40}, {300, TANK, 5}, {450, SWARM, 60},
};

// data/stage_settings.json, data/pickup_settings.json
constexpr float STAGE_DURATION = 600.f;
constexpr float DEATH_SLIME_BASE_INTERVAL = 5.f, DEATH_SLIME_MIN_INTERVAL = 0.1f;
constexpr float DEATH_SLIME_INTERVAL_MULT = 0.9f, DEATH_SLIME_GROWTH_SECONDS = 10.f;
constexpr float PICKUP_DESPAWN = 25.f, PICKUP_WARNING = 5.f, HEAL_DROP_CHANCE = 0.025f;
constexpr int HEAL_AMOUNT = 35;

struct Upgrade { const char* id; const char* text; float base; bool unique; };
constexpr Upgrade UPGRADES[] = {
    {"max_hp", "+%s%% Max HP", 15, false},
    {"speed", "+%s%% Speed", 8, false},
    {"damage", "+%s%% Damage", 15, false},
    {"fire_rate", "+%s%% Fire Rate", 10, false},
    {"aoe_size", "+%s%% Weapon Area", 15, false},
    {"regeneration", "+%s HP Regen", 2, false},
    {"thorns", "Reflect %s%% Damage", 50, false},
    {"evasion", "+%s%% Dodge", 5, false},
    {"crit_chance", "+%s%% Crit Chance", 5, false},
    {"glass_cannon", "Glass Cannon: +40% Dmg, -20% HP", 40, true},
    {"heavy_armor", "Heavy Armor: +50% Thorns, -15% Speed", 50, true},
    {"berserker", "Berserker: +30% Fire Rate, -10% Evasion", 30, true},
    {"vampiric_edge", "Vampiric Edge: +%s%% Kill Leech", 3, true},
    {"magnet_training", "+%s%% Pickup Range", 25, true},
    {"precision", "Precision: +%s%% Crit and Damage", 8, true},
    {"momentum", "Momentum: +%s%% Speed and Fire Rate", 6, true},
    {"soul_harvest", "Soul Harvest: +%s%% EXP and +1 Regen", 15, true},
    {"exp_boost", "+%s%% EXP", 20, false},
    {"multi_attack", "+%s Attack Strike", 1, false},
    {"fire_imbue", "Add Fire Damage (Burn)", 0, true},
    {"frost_imbue", "Add Frost Damage (Slow)", 0, true},
};

struct Rarity { const char* id; Color color; float mult; int weight; };
constexpr Rarity RARITIES[] = {
    {"white", rgb(1, 1, 1), 1.0f, 60},
    {"green", rgb(0.2f, 0.8f, 0.2f), 1.5f, 25},
    {"blue", rgb(0.2f, 0.5f, 1), 2.0f, 12},
    {"gold", rgb(1, 0.8f, 0.1f), 3.0f, 3},
};

enum WeaponId { WAND, POISON_AURA, ORBIT, WEAPON_COUNT };
// Orbit: wait is the per-enemy hit cooldown, speed the spin in radians per second.
struct WeaponLevel { int damage; float wait; int projectiles; float speed; float scale; };
constexpr const char* WEAPON_NAMES[WEAPON_COUNT] = {"Wand", "Poison Aura", "Thorn Orbit"};
constexpr WeaponLevel WEAPON_LEVELS[WEAPON_COUNT][3] = {
    {{20, 1.0f, 1, 400, 1}, {35, 0.8f, 1, 450, 1}, {55, 0.5f, 1, 550, 1}},
    {{15, 1.0f, 0, 0, 1.0f}, {25, 0.8f, 0, 0, 1.25f}, {40, 0.5f, 0, 0, 1.6f}},
    {{12, 0.6f, 2, 3, 1.0f}, {20, 0.5f, 2, 3.5f, 1.1f}, {32, 0.4f, 3, 4, 1.2f}},
};

// Items: bought from chests with silver, stacked without limit (Risk of Rain style).
enum ItemId { IT_QUILL, IT_BARK, IT_BOOTS, IT_CLOVER, IT_SPROUT, IT_LEECH,
              IT_EMBER, IT_BELL, IT_SPORES, IT_CHARM, IT_CROWN, IT_SICKLE, ITEM_COUNT };
struct ItemDef { const char* name; const char* desc; int tier; };
constexpr ItemDef ITEMS[ITEM_COUNT] = {
    {"Quick Quill", "+12% attack speed", 0},
    {"Oak Bark", "+30 max HP", 0},
    {"Fleet Boots", "+10% move speed", 0},
    {"Four-Leaf Clover", "+8% crit chance", 0},
    {"Green Sprout", "+1.5 HP regen", 0},
    {"Leech Seed", "Hits heal 1 HP", 0},
    {"Ember Stone", "15% chance on hit to burn", 1},
    {"Storm Bell", "20% chance on hit to zap 3 enemies (+2 per stack)", 1},
    {"Volatile Spores", "Kills explode (bigger per stack)", 1},
    {"Ward Charm", "15% chance to block a hit (stacks with falloff)", 1},
    {"Lily Crown", "+1 projectile and orb", 2},
    {"Reaper's Sickle", "Execute enemies under 13% HP (stacks with falloff)", 2},
};
struct ItemTier { const char* name; Color color; int weight; };
constexpr ItemTier ITEM_TIERS[] = {
    {"Common", rgb(1, 1, 1), 70},
    {"Uncommon", rgb(0.2f, 0.8f, 0.2f), 25},
    {"Legendary", rgb(1, 0.3f, 0.2f), 5},
};

constexpr int MAX_FLOORS = 4;

// Floor boss base stats (the "boss" entry of data/enemies.json).
constexpr int BOSS_HEALTH = 5000, BOSS_DAMAGE = 100;
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
