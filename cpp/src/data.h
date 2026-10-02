// Game data tables, ported from core/data.gd and data/*.json.
#pragma once
#include <raylib.h>

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
constexpr SpawnChance SPAWN_CHANCES[] = {
    {BASIC, 100, 0}, {RATMAN, 10, 15}, {RUNNER, 0, 25}, {SHOOTER, 0, 20},
    {SWARM, 0, 15}, {BRUTE, 0, 10}, {DASHER, 0, 8}, {TANK, 0, 3},
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

enum WeaponId { WAND, POISON_AURA };
struct WeaponLevel { int damage; float wait; int projectiles; float speed; float scale; };
constexpr const char* WEAPON_NAMES[] = {"Wand", "Poison Aura"};
constexpr WeaponLevel WEAPON_LEVELS[2][3] = {
    {{20, 1.0f, 1, 400, 1}, {35, 0.8f, 1, 450, 1}, {55, 0.5f, 1, 550, 1}},
    {{15, 1.0f, 0, 0, 1.0f}, {25, 0.8f, 0, 0, 1.25f}, {40, 0.5f, 0, 0, 1.6f}},
};
