// Convallaria, C++ / raylib port of the Godot game: the core survival loop on one floor.
#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "data.h"

namespace {

constexpr int SCREEN_W = 1280, SCREEN_H = 720;
constexpr float ZOOM = 2.f;
constexpr int TILE = 16, MAP_R = 100, BOUND = MAP_R + 15, MAP_N = BOUND * 2;
constexpr float SPAWN_RADIUS = 800, ACTIVE_RADIUS = 1000, DESPAWN_RADIUS = 3000;
constexpr int MAX_ENEMIES = 300, END_TIMES_EXTRA_CAP = 50;
constexpr float PLAYER_RADIUS = 9, MAGNET_RADIUS = 60, PICKUP_RADIUS = 15;
enum Facing { DOWN, LEFT, RIGHT, UP };

std::mt19937 rng{std::random_device{}()};
float rnd() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }
float rndr(float a, float b) { return a + (b - a) * rnd(); }
int rndi(int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng); }

const char* asset(const char* path) { return TextFormat("%s/%s", ASSET_DIR, path); }
float db(float d) { return powf(10.f, d / 20.f); }

int expForLevel(int level) { return int(15 + (level * 10) * (level * 0.4)); }

std::string fmtNum(float v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%.1f", roundf(v * 10.f) / 10.f);
    std::string s = buf;
    if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0) s.resize(s.size() - 2);
    return s;
}

// Fills the "%s" slot of an upgrade label and unescapes "%%".
std::string fillText(const char* text, float value) {
    std::string s = text;
    size_t i = s.find("%s");
    if (i == std::string::npos) return s;
    s.replace(i, 2, fmtNum(value));
    while ((i = s.find("%%")) != std::string::npos) s.replace(i, 2, "%");
    return s;
}

// Several aliases of one sound so hits can overlap; throttle drops spam.
struct Sfx {
    Sound base{};
    Sound alias[8]{};
    int next = 0;
    double last = 0;
    void load(Sound s) {
        base = s;
        for (auto& a : alias) a = LoadSoundAlias(base);
    }
    void play(float pitch, float volumeDb, double throttle = 0) {
        double now = GetTime();
        if (now - last < throttle) return;
        last = now;
        Sound& s = alias[next++ % 8];
        SetSoundPitch(s, pitch);
        SetSoundVolume(s, db(volumeDb));
        PlaySound(s);
    }
};

struct Assets {
    Texture2D player, slime, ratman, seed, projectile, aura;
    Font font;
    Sfx orb, levelup, hurt, wandShot, enemyShot, slimeHit, slimeDeath;
    Music music;
} A;

void loadAssets() {
    A.player = LoadTexture(asset("player/woman.png"));
    A.slime = LoadTexture(asset("enemies/blob.png"));
    A.ratman = LoadTexture(asset("enemies/ratman/ratman.png"));
    A.seed = LoadTexture(asset("drops/exp/seed.png"));
    A.projectile = LoadTexture(asset("weapons/wand/projectile.png"));
    A.aura = LoadTexture(asset("weapons/poison/poison_radius..png"));
    A.font = LoadFontEx(asset("ui/fonts/PixelifySans-VariableFont_wght.ttf"), 32, nullptr, 0);
    if (!IsFontValid(A.font)) A.font = GetFontDefault();
    A.orb.load(LoadSound(asset("player/orb.mp3")));
    A.levelup.load(LoadSound(asset("assets/audio/levelup.wav")));
    A.hurt.load(LoadSound(asset("player/hurt.mp3")));
    A.slimeHit.load(LoadSound(asset("enemies/slime.ogg")));
    A.slimeDeath.load(LoadSound(asset("enemies/slime.ogg")));
    // The Godot projectiles played these clips from 0.52s for 0.1s.
    for (auto [sfx, path] : {std::pair{&A.wandShot, "weapons/wand/shooting.mp3"}, std::pair{&A.enemyShot, "enemies/shooting.mp3"}}) {
        Wave w = LoadWave(asset(path));
        if (w.frameCount > unsigned(0.62f * w.sampleRate)) WaveCrop(&w, int(0.52f * w.sampleRate), int(0.62f * w.sampleRate));
        sfx->load(LoadSoundFromWave(w));
        UnloadWave(w);
    }
    A.music = LoadMusicStream(asset("ui/music.mp3"));
}

// ---------------------------------------------------------------- map

struct Map {
    std::vector<uint8_t> tiles;  // 0 water, 1 grass, 2 soil
    Texture2D tex{};
    Vector2 spawn{};

    uint8_t at(int cx, int cy) const {
        cx += BOUND, cy += BOUND;
        if (cx < 0 || cy < 0 || cx >= MAP_N || cy >= MAP_N) return 0;
        return tiles[cy * MAP_N + cx];
    }
    uint8_t at(Vector2 p) const { return at(int(floorf(p.x / TILE)), int(floorf(p.y / TILE))); }
    bool landAround(Vector2 p, int r) const {
        int cx = int(floorf(p.x / TILE)), cy = int(floorf(p.y / TILE));
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (!at(cx + dx, cy + dy)) return false;
        return true;
    }
};

const Color WATER = {58, 110, 165, 255}, GRASS = {98, 160, 72, 255}, SOIL = {139, 108, 66, 255};

// Same island recipe as world/map.gd: fbm Perlin noise with a radial falloff.
Map genMap() {
    Map m;
    m.tiles.assign(MAP_N * MAP_N, 0);
    Image noise = GenImagePerlinNoise(MAP_N, MAP_N, rndi(100000), rndi(100000), 0.04f * MAP_N);
    Color* px = LoadImageColors(noise);
    Image img = GenImageColor(MAP_N, MAP_N, BLANK);
    float best = 1e9f;
    for (int y = -MAP_R; y < MAP_R; y++)
        for (int x = -MAP_R; x < MAP_R; x++) {
            float d2 = float(x * x + y * y);
            if (d2 > MAP_R * MAP_R) continue;
            int i = (y + BOUND) * MAP_N + (x + BOUND);
            float v = px[i].r / 255.f * (1.f - powf(sqrtf(d2) / MAP_R, 2.5f));
            if (v <= 0.2f) continue;
            m.tiles[i] = v > 0.6f ? 2 : 1;
            ImageDrawPixel(&img, x + BOUND, y + BOUND, ColorBrightness(m.tiles[i] == 2 ? SOIL : GRASS, rndr(-0.05f, 0.05f)));
            if (d2 < best) best = d2, m.spawn = {(x + 0.5f) * TILE, (y + 0.5f) * TILE};
        }
    UnloadImageColors(px);
    UnloadImage(noise);
    m.tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return m;
}

// ---------------------------------------------------------------- entities

struct Enemy {
    uint32_t id = 0;
    Vector2 pos{}, vel{};
    float hp = 1, maxHp = 1, speed = 0, slowMul = 1, scale = 1, pitch = 1;
    Color color = WHITE;
    int damage = 0, exp = 0;
    bool shooter = false, ratman = false, dying = false, gone = false;
    int facing = DOWN, burnTicks = 0;
    float anim = 0, hurtT = 0, deathT = 0, shootT = 0, burnT = 0, burnDamage = 0, slowT = 0;
    float radius() const { return 6.f * scale; }
};

struct Shot {
    Vector2 pos, dir;
    float speed, size, life;
    int damage, pierce, ricochet;
    bool fire, frost, crit;
    std::vector<uint32_t> hit;
};
struct EnemyShot { Vector2 pos, dir; int damage; float life; };
enum SeedType { SEED_EXP, SEED_MAGNET, SEED_SPEED, SEED_BOMB, SEED_GOLD, SEED_SILVER, SEED_HEAL };
struct Seed { Vector2 pos; int type, amount; float life, speed = 0, glimmer; bool magnetic = false; };
struct DamageNumber { Vector2 pos; int value; float t; };

struct Weapon {
    WeaponId id = WAND;
    int level = 1, pierce = 0, ricochet = 0, projectile = 0;
    float damage = 1, size = 1, fireRate = 1, timer = 0, pulse = 0;
    const WeaponLevel& stats() const { return WEAPON_LEVELS[id][std::min(level, 3) - 1]; }
};

struct Player {
    Vector2 pos{};
    float speed = 165, maxHp = 250, hp = 250;
    int level = 1, exp = 0, expNext = 15, kills = 0, silver = 0, gold = 0;
    float dmgMul = 1, fireRateMul = 1, aoe = 1, expMul = 1, regen = 0, regenAcc = 0;
    float thorns = 0, evasion = 0, crit = 0, vampirism = 0, magnetScale = 1;
    bool imbueFire = false, imbueFrost = false, boosted = false, moving = false;
    float magnetT = 0, magnetScan = 0, speedT = 0, boostBase = 0, iframes = 0, anim = 0, time = 0;
    int facing = DOWN;
    std::vector<std::string> uniques;
    Weapon weapon;
};

// Counting-sort uniform grid over enemies, rebuilt every frame. Replaces Godot's
// physics broadphase for hits, separation and contact damage.
struct Grid {
    static constexpr float CELL = 32;
    static constexpr int W = MAP_N * TILE / int(CELL) + 1;
    static constexpr float ORIGIN = -BOUND * TILE;
    std::vector<int> start, items, cellOf, fill;

    static int cell(float v) { return std::clamp(int((v - ORIGIN) / CELL), 0, W - 1); }
    void build(const std::vector<Enemy>& es) {
        start.assign(W * W + 1, 0);
        cellOf.resize(es.size());
        items.resize(es.size());
        for (size_t i = 0; i < es.size(); i++) {
            cellOf[i] = cell(es[i].pos.y) * W + cell(es[i].pos.x);
            start[cellOf[i] + 1]++;
        }
        for (int c = 0; c < W * W; c++) start[c + 1] += start[c];
        fill.assign(start.begin(), start.end() - 1);
        for (size_t i = 0; i < es.size(); i++) items[fill[cellOf[i]]++] = int(i);
    }
    // Calls f(index) for enemies in cells touching the circle; stop early by returning true.
    template <class F>
    void query(Vector2 p, float r, F f) const {
        for (int cy = cell(p.y - r); cy <= cell(p.y + r); cy++)
            for (int cx = cell(p.x - r); cx <= cell(p.x + r); cx++) {
                int c = cy * W + cx;
                for (int k = start[c]; k < start[c + 1]; k++)
                    if (f(items[k])) return;
            }
    }
};

struct Option { bool buff; int index; float value; int rarity; std::string text; };
enum Buff { B_DAMAGE, B_FIRE_RATE, B_SIZE, B_RICOCHET, B_PROJECTILE };
enum class Mode { Title, Play, LevelUp, Paused, GameOver };

struct Game {
    Map map;
    Player p;
    std::vector<Enemy> enemies;
    std::vector<Shot> shots;
    std::vector<EnemyShot> enemyShots;
    std::vector<Seed> seeds;
    std::vector<DamageNumber> numbers;
    std::vector<Option> options;
    Grid grid;
    Mode mode = Mode::Title;
    uint32_t nextId = 1;
    float spawnT = 1, spawnWait = 1, difficultyT = 5, deathT = 0, deathWait = DEATH_SLIME_BASE_INTERVAL;
    int spawnCount = 1, lastSecond = -1;
    bool endTimes = false, quit = false;
};

void newRun(Game& g, WeaponId weapon) {
    if (g.map.tex.id) UnloadTexture(g.map.tex);
    g = Game{};
    g.map = genMap();
    g.p.pos = g.map.spawn;
    g.p.weapon.id = weapon;
    g.mode = Mode::Play;
}

float runMinutes(const Game& g) { return g.p.time / 60.f; }

void playAt(Sfx& s, const Game& g, Vector2 at, float pitch, float volumeDb, double throttle) {
    float fade = 1.f - Vector2Distance(at, g.p.pos) / 800.f;
    if (fade > 0) s.play(pitch, volumeDb + 20.f * log10f(fade), throttle);
}

// ---------------------------------------------------------------- combat

void hurtEnemy(Game& g, Enemy& e, int amount) {
    if (e.dying) return;
    e.hp -= amount;
    g.numbers.push_back({e.pos, amount, 0});
    if (e.hp <= 0) {
        e.dying = true;
        e.deathT = 0.4f;
        playAt(A.slimeDeath, g, e.pos, e.pitch, -10, 0.05);
    } else if (e.hurtT <= 0) {
        e.hurtT = 0.4f;
        playAt(A.slimeHit, g, e.pos, e.pitch, -5, 0.05);
    }
}

void applyBurn(Enemy& e, float damage) {
    if (e.burnTicks > 0 || e.dying) return;
    e.burnTicks = 4, e.burnT = 0.5f, e.burnDamage = damage;
}

void applySlow(Enemy& e, float mult) {
    if (e.slowT > 0 || e.dying) return;
    e.slowT = 3, e.slowMul = mult;
}

void hurtPlayer(Game& g, int damage, Enemy* source) {
    Player& p = g.p;
    if (p.iframes > 0 || rnd() < p.evasion) return;
    p.hp -= damage;
    A.hurt.play(rndr(1.4f, 1.8f), -5);
    if (source && p.thorns > 0) hurtEnemy(g, *source, int(damage * p.thorns));
    if (p.hp <= 0) g.mode = Mode::GameOver;
    else p.iframes = 0.4f;
}

void buildOptions(Game& g);

void gainExp(Game& g, int amount) {
    Player& p = g.p;
    p.exp += int(amount * p.expMul);
    A.orb.play(rndr(1.4f, 1.7f), -12, 0.04);
    bool leveled = false;
    while (p.exp >= p.expNext) {
        p.exp -= p.expNext;
        p.level++;
        p.expNext = expForLevel(p.level);
        p.maxHp += 10;
        p.dmgMul += 0.05f;
        leveled = true;
    }
    if (leveled) {
        A.levelup.play(1, -12);
        buildOptions(g);
        g.mode = Mode::LevelUp;
    }
}

void heal(Player& p, float amount) { p.hp = std::min(p.hp + amount, p.maxHp); }

void addKill(Game& g) {
    g.p.kills++;
    if (g.p.vampirism > 0 && g.p.hp < g.p.maxHp && rnd() <= g.p.vampirism) heal(g.p, g.p.maxHp * 0.05f);
}

void dropSeed(Game& g, Vector2 pos, int exp) {
    const float p = 0.01f, c = 0.05f;
    float roll = rnd();
    Seed s{pos, SEED_EXP, exp, PICKUP_DESPAWN, 0, rndr(0.4f, 0.7f)};
    if (roll <= p) s.type = SEED_MAGNET;
    else if (roll <= p * 2) s.type = SEED_SPEED;
    else if (roll <= p * 3) s.type = SEED_BOMB;
    else if (roll <= p * 3 + c) s.type = SEED_GOLD, s.amount = 1;
    else if (roll <= p * 3 + c * 2) s.type = SEED_SILVER, s.amount = 1;
    else if (roll <= p * 3 + c * 2 + HEAL_DROP_CHANCE) s.type = SEED_HEAL, s.amount = HEAL_AMOUNT;
    g.seeds.push_back(s);
}

int weightedEnemy(float minutes) {
    float weights[std::size(SPAWN_CHANCES)], total = 0;
    for (size_t i = 0; i < std::size(SPAWN_CHANCES); i++)
        total += weights[i] = std::max(0.f, SPAWN_CHANCES[i].base + SPAWN_CHANCES[i].growth * minutes);
    if (total <= 0) return BASIC;
    float roll = rnd() * total, acc = 0;
    for (size_t i = 0; i < std::size(SPAWN_CHANCES); i++)
        if (roll <= (acc += weights[i])) return SPAWN_CHANCES[i].type;
    return BASIC;
}

bool spawnEnemy(Game& g, int type, int extraCap) {
    if (int(g.enemies.size()) >= MAX_ENEMIES + extraCap) return false;
    Vector2 pos{};
    bool ok = false;
    for (int a = 0; a < 50 && !ok; a++) {
        float ang = rndr(0, 2 * PI);
        pos = Vector2Add(g.p.pos, {cosf(ang) * SPAWN_RADIUS, sinf(ang) * SPAWN_RADIUS});
        ok = g.map.landAround(pos, 1);
    }
    if (!ok) return false;
    if (type < 0) type = weightedEnemy(runMinutes(g));

    const EnemyDef& d = ENEMIES[type];
    Enemy e;
    e.id = g.nextId++;
    e.pos = pos;
    e.shootT = rndr(2, 4);
    e.anim = rndr(0, 1);
    if (g.p.time >= STAGE_DURATION || d.deathSlime) {
        // Overtime: death slimes keep scaling the longer you stay.
        float over = std::max(0.f, g.p.time - STAGE_DURATION), om = over / 60.f;
        float c = std::max(0.f, 0.4f - over * 0.003f);
        e.hp = float(int(int(30 + over * 25) * (1 + om * 0.5f)));
        e.damage = int(int(12 + over * 1.5f) * (1 + om * 0.5f));
        e.speed = (80 + over * 2) * (1 + om * 0.2f);
        e.scale = (1.2f + over * 0.015f) * (1 + om * 0.1f);
        e.exp = int(10 + over * 0.5f);
        e.color = rgb(c, 0, c);
        e.pitch = std::max(0.2f, 1.f - over * 0.01f);
    } else {
        float m = runMinutes(g);
        e.hp = float(int(d.health * (1 + m * 0.3f)));
        e.damage = int(d.damage * (1 + m * 0.15f));
        e.speed = d.speed + m * 3;
        e.scale = d.scale;
        e.exp = d.exp;
        e.color = d.color;
        e.pitch = d.pitch;
        e.shooter = d.shooter;
        e.ratman = d.ratman;
        if (e.ratman) {
            float v = rnd();
            if (v > 0.75f) e.scale *= 1.25f, e.hp = float(int(e.hp * 1.5f)), e.speed *= 0.75f, e.color = rgb(0.7f, 0.6f, 0.6f);
            else if (v < 0.25f) e.scale *= 0.8f, e.hp = float(int(e.hp * 0.6f)), e.speed *= 1.4f, e.color = rgb(1.2f, 1.1f, 1.1f);
        }
    }
    e.maxHp = e.hp;
    g.enemies.push_back(e);
    return true;
}

// ---------------------------------------------------------------- level up

const char* BUFF_TEXT[] = {"+%s%% Damage", "+%s%% Fire Rate", "+%s%% Size", "+%s Bounce", "+%s Projectile"};
const float BUFF_VALUE[] = {0.15f, 0.10f, 0.15f, 1, 1};

int rollRarity() {
    int roll = rndi(100), acc = 0;
    for (int i = 0; i < int(std::size(RARITIES)); i++)
        if (roll < (acc += RARITIES[i].weight)) return i;
    return 0;
}

bool hasUnique(const Player& p, const char* id) {
    return std::find(p.uniques.begin(), p.uniques.end(), id) != p.uniques.end();
}

void buildOptions(Game& g) {
    struct Entry { bool buff; int index; };
    std::vector<Entry> pool;
    for (int i = 0; i < int(std::size(UPGRADES)); i++)
        if (!(UPGRADES[i].unique && hasUnique(g.p, UPGRADES[i].id))) pool.push_back({false, i});
    const Weapon& w = g.p.weapon;
    if (w.level < 99) {
        std::vector<int> buffs = {B_DAMAGE, B_FIRE_RATE, B_SIZE};
        if (w.id == WAND) buffs.insert(buffs.end(), {B_RICOCHET, B_PROJECTILE});
        for (int i = 0; i < 4; i++) pool.push_back({true, buffs[rndi(int(buffs.size()))]});
    }
    std::shuffle(pool.begin(), pool.end(), rng);

    g.options.clear();
    std::vector<std::pair<bool, int>> used;
    for (const Entry& en : pool) {
        if (g.options.size() >= 3) break;
        if (std::find(used.begin(), used.end(), std::pair{en.buff, en.index}) != used.end()) continue;
        used.push_back({en.buff, en.index});
        int r = rollRarity();
        if (!en.buff) {
            float v = UPGRADES[en.index].base * RARITIES[r].mult;
            g.options.push_back({false, en.index, v, r, fillText(UPGRADES[en.index].text, v)});
        } else {
            float v = BUFF_VALUE[en.index] * RARITIES[r].mult;
            float shown = en.index <= B_SIZE ? v * 100 : v;
            const char* text = en.index == B_SIZE ? (w.id == WAND ? "+%s%% Splash Size" : "+%s%% Aura Radius") : BUFF_TEXT[en.index];
            std::string label = std::string(WEAPON_NAMES[w.id]) + " Lv." + std::to_string(w.level + 1) + "\n" + fillText(text, shown);
            g.options.push_back({true, en.index, v, r, label});
        }
    }
}

void applyOption(Game& g, const Option& o) {
    Player& p = g.p;
    float v = o.value, pct = v / 100.f;
    if (o.buff) {
        Weapon& w = p.weapon;
        w.level++;
        switch (o.index) {
            case B_DAMAGE: w.damage += v; break;
            case B_FIRE_RATE: w.fireRate += v; break;
            case B_SIZE: w.size += v; break;
            case B_RICOCHET: w.ricochet += int(v); break;
            case B_PROJECTILE: w.projectile += int(v); break;
        }
        return;
    }
    std::string id = UPGRADES[o.index].id;
    auto fasterFire = [&](float d) { p.fireRateMul = std::max(0.2f, p.fireRateMul - d); };
    if (id == "max_hp") { float inc = p.maxHp * pct; p.maxHp += inc; p.hp += inc; }
    else if (id == "speed") p.speed += p.speed * pct;
    else if (id == "damage") p.dmgMul += pct;
    else if (id == "fire_rate") fasterFire(pct);
    else if (id == "aoe_size") p.aoe += pct;
    else if (id == "fire_imbue") p.imbueFire = true;
    else if (id == "frost_imbue") p.imbueFrost = true;
    else if (id == "regeneration") p.regen += v;
    else if (id == "thorns") p.thorns += pct;
    else if (id == "evasion") p.evasion += pct;
    else if (id == "crit_chance") p.crit += pct;
    else if (id == "exp_boost") p.expMul += pct;
    else if (id == "multi_attack") p.weapon.projectile += int(v);
    else if (id == "glass_cannon") { p.dmgMul += pct; p.maxHp -= p.maxHp * 0.2f; p.hp = std::min(p.hp, p.maxHp); }
    else if (id == "heavy_armor") { p.thorns += pct; p.speed -= p.speed * 0.15f; p.maxHp += p.maxHp * 0.1f; p.hp += p.maxHp * 0.1f; }
    else if (id == "berserker") { fasterFire(pct); p.evasion = std::max(0.f, p.evasion - 0.1f); }
    else if (id == "vampiric_edge") p.vampirism += pct;
    else if (id == "magnet_training") p.magnetScale += pct;
    else if (id == "precision") { p.crit += pct; p.dmgMul += pct; }
    else if (id == "momentum") { p.speed += p.speed * pct; fasterFire(pct); }
    else if (id == "soul_harvest") { p.expMul += pct; p.regen += 1; }
    if (UPGRADES[o.index].unique && !hasUnique(p, id.c_str())) p.uniques.push_back(id);
}

// ---------------------------------------------------------------- update

void updatePlayer(Game& g, float dt) {
    Player& p = g.p;
    p.time += dt;
    Vector2 in = {float(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - float(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)),
                  float(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) - float(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))};
    if (Vector2Length(in) > 1) in = Vector2Normalize(in);
    p.moving = in.x != 0 || in.y != 0;
    if (p.moving) {
        p.facing = fabsf(in.x) > fabsf(in.y) ? (in.x > 0 ? RIGHT : LEFT) : (in.y > 0 ? DOWN : UP);
        p.anim += dt;
        Vector2 nx = {p.pos.x + in.x * p.speed * dt, p.pos.y};
        if (g.map.at(nx)) p.pos.x = nx.x;
        Vector2 ny = {p.pos.x, p.pos.y + in.y * p.speed * dt};
        if (g.map.at(ny)) p.pos.y = ny.y;
    }

    if (p.regen > 0 && p.hp < p.maxHp) {
        p.regenAcc += p.regen * dt;
        if (p.regenAcc >= 1) {
            float whole = floorf(p.regenAcc);
            heal(p, whole);
            p.regenAcc -= whole;
        }
    }

    if (p.magnetT > 0) {
        p.magnetT -= dt;
        if ((p.magnetScan -= dt) <= 0) {
            p.magnetScan = 0.2f;
            for (Seed& s : g.seeds)
                if (s.type == SEED_EXP || s.type >= SEED_GOLD) s.magnetic = true;
        }
    }
    if (p.speedT > 0 && (p.speedT -= dt) <= 0) p.speed = p.boostBase, p.boosted = false;

    if (p.iframes > 0) {
        p.iframes -= dt;
        return;
    }
    g.grid.query(p.pos, PLAYER_RADIUS + 60, [&](int i) {
        Enemy& e = g.enemies[i];
        if (e.dying || Vector2Distance(e.pos, p.pos) > PLAYER_RADIUS + e.radius()) return false;
        hurtPlayer(g, e.damage, &e);
        return true;
    });
}

void updateSpawner(Game& g, float dt) {
    Player& p = g.p;
    if (g.endTimes) {
        if ((g.deathT -= dt) <= 0) {
            float over = std::max(0.f, p.time - STAGE_DURATION);
            int n = 1 + int(over / DEATH_SLIME_GROWTH_SECONDS);
            for (int i = 0; i < n; i++)
                if (!spawnEnemy(g, DEATH_SLIME, END_TIMES_EXTRA_CAP)) break;
            g.deathWait = std::max(DEATH_SLIME_MIN_INTERVAL, g.deathWait * DEATH_SLIME_INTERVAL_MULT);
            g.deathT = g.deathWait;
        }
        return;
    }
    int second = int(p.time);
    if (second != g.lastSecond) {
        g.lastSecond = second;
        for (const HordeEvent& h : HORDES)
            if (h.second == second)
                for (int i = 0; i < h.amount; i++)
                    if (!spawnEnemy(g, h.type, 0)) break;
    }
    if (p.time >= STAGE_DURATION) {
        g.endTimes = true;
        g.deathT = g.deathWait;
        return;
    }
    if ((g.spawnT -= dt) <= 0) {
        g.spawnT = g.spawnWait;
        for (int i = 0; i < g.spawnCount; i++)
            if (!spawnEnemy(g, -1, 0)) break;
    }
    if ((g.difficultyT -= dt) <= 0) {
        g.difficultyT = 5;
        if (g.spawnWait > 0.5f) g.spawnWait -= 0.05f;
        else g.spawnCount++;
    }
}

void updateWeapon(Game& g, float dt) {
    Player& p = g.p;
    Weapon& w = p.weapon;
    const WeaponLevel& lv = w.stats();
    w.pulse = std::max(0.f, w.pulse - dt);
    float wait = std::max(0.05f, lv.wait * p.fireRateMul / w.fireRate);
    if ((w.timer += dt) < wait) return;
    w.timer = 0;
    int damage = int(roundf(lv.damage * p.dmgMul * w.damage));

    if (w.id == POISON_AURA) {
        w.pulse = 0.3f;
        float radius = 48 * lv.scale * p.aoe * w.size;
        g.grid.query(p.pos, radius + 60, [&](int i) {
            Enemy& e = g.enemies[i];
            if (e.dying || Vector2Distance(e.pos, p.pos) > radius + e.radius()) return false;
            hurtEnemy(g, e, damage);
            if (p.imbueFire) applyBurn(e, damage * 0.2f);
            if (p.imbueFrost) applySlow(e, 0.5f);
            return false;
        });
        return;
    }

    const Enemy* target = nullptr;
    float best = 800;
    for (const Enemy& e : g.enemies) {
        float d = Vector2Distance(e.pos, p.pos);
        if (!e.dying && d < best) best = d, target = &e;
    }
    if (!target) return;
    Vector2 dir = Vector2Normalize(Vector2Subtract(target->pos, p.pos));
    int count = lv.projectiles + w.projectile;
    A.wandShot.play(1, -10, 0.05);
    for (int i = 0; i < count; i++) {
        bool crit = rnd() <= 0.05f + p.crit;
        Vector2 d = Vector2Rotate(dir, (i - (count - 1) / 2.f) * 15 * DEG2RAD);
        g.shots.push_back({p.pos, d, lv.speed, crit ? 1.5f * w.size : w.size, 3, crit ? damage * 2 : damage,
                           w.pierce, w.ricochet, p.imbueFire, p.imbueFrost, crit, {}});
    }
}

void explode(Game& g, const Shot& s) {
    float radius = 35 * s.size;
    g.grid.query(s.pos, radius + 60, [&](int i) {
        Enemy& e = g.enemies[i];
        if (e.dying || Vector2Distance(e.pos, s.pos) > radius + e.radius()) return false;
        bool wasFull = e.hp >= e.maxHp;
        hurtEnemy(g, e, s.damage);
        if (wasFull && e.hp <= 0) g.p.crit += 0.001f;  // one-shot kills sharpen your crits
        if (s.fire) applyBurn(e, s.damage * 0.2f);
        if (s.frost) applySlow(e, 0.5f);
        return false;
    });
}

void updateShots(Game& g, float dt) {
    for (Shot& s : g.shots) {
        s.pos = Vector2Add(s.pos, Vector2Scale(s.dir, s.speed * dt));
        s.life -= dt;
        if (s.life <= 0 || Vector2Distance(s.pos, g.p.pos) > 900) continue;
        int hit = -1;
        g.grid.query(s.pos, 6 * s.size + 60, [&](int i) {
            const Enemy& e = g.enemies[i];
            if (e.dying || Vector2Distance(e.pos, s.pos) > 6 * s.size + e.radius()) return false;
            if (std::find(s.hit.begin(), s.hit.end(), e.id) != s.hit.end()) return false;
            hit = i;
            return true;
        });
        if (hit < 0) continue;
        s.hit.push_back(g.enemies[hit].id);
        explode(g, s);
        if (s.pierce > 0) { s.pierce--; continue; }
        if (s.ricochet > 0) {
            s.ricochet--;
            const Enemy* next = nullptr;
            float best = 400 * s.size;
            for (const Enemy& e : g.enemies) {
                float d = Vector2Distance(e.pos, s.pos);
                if (!e.dying && d < best && std::find(s.hit.begin(), s.hit.end(), e.id) == s.hit.end()) best = d, next = &e;
            }
            if (next) { s.dir = Vector2Normalize(Vector2Subtract(next->pos, s.pos)); continue; }
        }
        s.life = 0;
    }
    std::erase_if(g.shots, [&](const Shot& s) { return s.life <= 0 || Vector2Distance(s.pos, g.p.pos) > 900; });

    for (EnemyShot& s : g.enemyShots) {
        s.pos = Vector2Add(s.pos, Vector2Scale(s.dir, 250 * dt));
        s.life -= dt;
        if (Vector2Distance(s.pos, g.p.pos) < PLAYER_RADIUS + 6) {
            hurtPlayer(g, s.damage, nullptr);
            s.life = 0;
        }
    }
    std::erase_if(g.enemyShots, [](const EnemyShot& s) { return s.life <= 0; });
}

void updateEnemies(Game& g, float dt) {
    Player& p = g.p;
    for (size_t i = 0; i < g.enemies.size(); i++) {
        Enemy& e = g.enemies[i];
        if (e.dying) { e.deathT -= dt; continue; }
        if (e.burnTicks > 0 && (e.burnT -= dt) <= 0) {
            e.burnTicks--, e.burnT = 0.5f;
            hurtEnemy(g, e, int(e.burnDamage));
            if (e.dying) continue;
        }
        if (e.slowT > 0 && (e.slowT -= dt) <= 0) e.slowMul = 1;

        Vector2 to = Vector2Subtract(p.pos, e.pos);
        float dist = Vector2Length(to);
        if (dist > DESPAWN_RADIUS) { e.gone = true; continue; }
        Vector2 dir = dist > 0 ? Vector2Scale(to, 1 / dist) : Vector2{};
        float speed = e.speed * e.slowMul;

        if (e.hurtT > 0) {
            e.hurtT -= dt;
            e.vel = Vector2Scale(e.vel, powf(0.95f, dt * 60));
        } else if (dist > ACTIVE_RADIUS) {
            e.vel = Vector2Scale(dir, speed);
        } else if (e.shooter && dist <= 350) {
            e.vel = {};
            if ((e.shootT -= dt) <= 0) {
                e.shootT = rndr(3.5f, 6);
                g.enemyShots.push_back({e.pos, dir, e.damage, 4});
                playAt(A.enemyShot, g, e.pos, 1, -9, 0.05);
            }
        } else {
            // Soft separation from the closest few neighbours.
            Vector2 push{};
            int n = 0;
            g.grid.query(e.pos, e.radius() + 60, [&](int j) {
                if (j == int(i)) return false;
                const Enemy& o = g.enemies[j];
                Vector2 away = Vector2Subtract(e.pos, o.pos);
                float d = Vector2Length(away);
                if (d >= e.radius() + o.radius()) return false;
                push = Vector2Add(push, d > 0 ? Vector2Scale(away, 1 / d) : Vector2{rndr(-1, 1), rndr(-1, 1)});
                return ++n >= 3;
            });
            Vector2 want = Vector2Add(Vector2Scale(dir, speed), Vector2Scale(Vector2Normalize(push), 20));
            e.vel = Vector2ClampValue(want, 0, speed);
        }

        Vector2 step = Vector2Scale(e.vel, dt);
        if (dist > ACTIVE_RADIUS) {
            e.pos = Vector2Add(e.pos, step);
        } else {
            if (g.map.at(Vector2{e.pos.x + step.x, e.pos.y})) e.pos.x += step.x;
            if (g.map.at(Vector2{e.pos.x, e.pos.y + step.y})) e.pos.y += step.y;
        }
        if (Vector2LengthSqr(e.vel) > 0 && e.hurtT <= 0) {
            e.facing = fabsf(e.vel.x) > fabsf(e.vel.y) ? (e.vel.x > 0 ? RIGHT : LEFT) : (e.vel.y > 0 ? DOWN : UP);
            e.anim += dt;
        }
    }

    for (size_t i = g.enemies.size(); i-- > 0;) {
        Enemy& e = g.enemies[i];
        if (e.dying && e.deathT <= 0) {
            addKill(g);
            dropSeed(g, e.pos, e.exp);
        } else if (!e.gone) {
            continue;
        }
        e = g.enemies.back();
        g.enemies.pop_back();
    }
}

void collectSeed(Game& g, const Seed& s) {
    Player& p = g.p;
    switch (s.type) {
        case SEED_EXP: gainExp(g, s.amount); break;
        case SEED_MAGNET: A.orb.play(1.3f, -5); p.magnetT = 5, p.magnetScan = 0; break;
        case SEED_SPEED:
            A.orb.play(1.3f, -5);
            if (!p.boosted) p.boostBase = p.speed, p.speed *= 1.5f, p.boosted = true;
            p.speedT = 5;
            break;
        case SEED_BOMB:
            A.orb.play(1.3f, -5);
            g.grid.query(s.pos, 600, [&](int i) {
                if (Vector2Distance(g.enemies[i].pos, s.pos) <= 600) hurtEnemy(g, g.enemies[i], 150);
                return false;
            });
            break;
        case SEED_GOLD: p.gold += s.amount; A.orb.play(2, -5, 0.04); break;
        case SEED_SILVER: p.silver += s.amount; A.orb.play(2, -5, 0.04); break;
        case SEED_HEAL:
            if (p.hp < p.maxHp) heal(p, float(s.amount)), A.orb.play(1.8f, -6, 0.04);
            break;
    }
}

void updateSeeds(Game& g, float dt) {
    Player& p = g.p;
    for (Seed& s : g.seeds) {
        s.life -= dt;
        float d = Vector2Distance(s.pos, p.pos);
        bool pullable = s.type == SEED_EXP || s.type >= SEED_GOLD;
        if (pullable && d < MAGNET_RADIUS * p.magnetScale) s.magnetic = true;
        if (s.magnetic && d > 0) {
            s.speed = std::min(900.f, s.speed + 2000 * dt);
            s.pos = Vector2MoveTowards(s.pos, p.pos, s.speed * dt);
            d = Vector2Distance(s.pos, p.pos);
        }
        if (d < PICKUP_RADIUS) {
            s.life = -1, s.magnetic = false;
            collectSeed(g, s);
        }
    }
    std::erase_if(g.seeds, [](const Seed& s) { return s.life <= 0 && !s.magnetic; });
}

// ---------------------------------------------------------------- draw

void drawFrame(Texture2D tex, int col, int row, Vector2 pos, float scale, Color tint) {
    float size = 48 * scale;
    DrawTexturePro(tex, {col * 48.f, row * 48.f, 48, 48}, {pos.x, pos.y, size, size}, {size / 2, size / 2}, 0, tint);
}

void drawWorld(const Game& g, Camera2D cam) {
    const Player& p = g.p;
    BeginMode2D(cam);
    float extent = MAP_N * TILE;
    DrawTexturePro(g.map.tex, {0, 0, float(MAP_N), float(MAP_N)}, {-BOUND * TILE * 1.f, -BOUND * TILE * 1.f, extent, extent}, {}, 0, WHITE);

    static const Color SEED_TINT[] = {WHITE, rgb(1, .2f, .2f), rgb(.2f, .5f, 1), rgb(.1f, .1f, .1f), rgb(1, .8f, .1f), rgb(.8f, .8f, .85f), rgb(.7f, .25f, 1)};
    static const float SEED_SCALE[] = {1.2f, 1.8f, 1.6f, 1.7f, 1.5f, 1.4f, 1.6f};
    for (const Seed& s : g.seeds) {
        float t = float(GetTime());
        float alpha = s.life < PICKUP_WARNING && !s.magnetic ? 0.6f + 0.4f * sinf(t * 20) : 0.75f + 0.25f * sinf(t * PI / s.glimmer);
        float size = 10 * SEED_SCALE[s.type];
        DrawTexturePro(A.seed, {0, 0, 48, 48}, {s.pos.x, s.pos.y, size, size}, {size / 2, size / 2}, 0, Fade(SEED_TINT[s.type], alpha));
    }

    if (p.weapon.id == POISON_AURA) {
        float r = 48 * p.weapon.stats().scale * p.aoe * p.weapon.size;
        DrawTexturePro(A.aura, {0, 0, 48, 48}, {p.pos.x, p.pos.y, r * 2, r * 2}, {r, r}, 0, Fade(WHITE, 0.35f + p.weapon.pulse));
    }

    for (const Enemy& e : g.enemies) {
        Color tint = e.burnTicks > 0 ? rgb(1, .4f, .1f) : e.slowT > 0 ? rgb(.3f, .8f, 1) : e.color;
        int walk = int(e.anim * 5) % 4;
        if (e.ratman) {
            if (e.hurtT > 0) tint = RED;
            drawFrame(A.ratman, walk, e.facing, e.pos, e.scale, e.dying ? Fade(tint, e.deathT / 0.4f) : tint);
        } else if (e.dying) {
            drawFrame(A.slime, 12 + (e.deathT < 0.2f), 0, e.pos, e.scale, tint);
        } else {
            drawFrame(A.slime, e.hurtT > 0 ? 10 + (e.hurtT < 0.2f) : 6 + walk, e.facing, e.pos, e.scale, tint);
        }
        if (!e.dying && e.hp < e.maxHp) {
            float w = 16 * e.scale, y = e.pos.y + 10 * e.scale;
            DrawRectangleV({e.pos.x - w / 2, y}, {w, 2}, Fade(BLACK, 0.6f));
            DrawRectangleV({e.pos.x - w / 2, y}, {w * e.hp / e.maxHp, 2}, RED);
        }
    }

    Color tint = p.boosted ? rgb(.5f, .8f, 1) : WHITE;
    if (p.iframes > 0) tint = Fade(tint, int(p.iframes * 10) % 2 ? 0.3f : 1.f);
    drawFrame(A.player, p.moving ? int(p.anim * 5) % 4 : 0, p.facing, p.pos, 1, tint);

    for (const Shot& s : g.shots) {
        float size = 16 * s.size;
        DrawTexturePro(A.projectile, {0, 0, 16, 16}, {s.pos.x, s.pos.y, size, size}, {size / 2, size / 2},
                       atan2f(s.dir.y, s.dir.x) * RAD2DEG, s.crit ? rgb(1, .8f, .1f) : WHITE);
    }
    for (const EnemyShot& s : g.enemyShots) DrawCircleV(s.pos, 4, rgb(1, .3f, .2f));
    for (const DamageNumber& n : g.numbers) {
        const char* txt = TextFormat("%d", n.value);
        Vector2 at = {n.pos.x - 4, n.pos.y - 14 - n.t * 30};
        DrawTextEx(A.font, txt, {at.x + 0.5f, at.y + 0.5f}, 8, 0, Fade(BLACK, 1 - n.t / 0.6f));
        DrawTextEx(A.font, txt, at, 8, 0, Fade(WHITE, 1 - n.t / 0.6f));
    }
    EndMode2D();
}

void text(const char* s, float x, float y, float size, Color c, bool center = false) {
    if (center) x -= MeasureTextEx(A.font, s, size, 1).x / 2;
    DrawTextEx(A.font, s, {x + 2, y + 2}, size, 1, Fade(BLACK, 0.6f));
    DrawTextEx(A.font, s, {x, y}, size, 1, c);
}

void bar(float x, float y, float w, float h, float frac, Color c) {
    DrawRectangleRec({x - 2, y - 2, w + 4, h + 4}, Fade(BLACK, 0.6f));
    DrawRectangleRec({x, y, w * std::clamp(frac, 0.f, 1.f), h}, c);
}

void drawHud(const Game& g) {
    const Player& p = g.p;
    float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
    bar(0, 0, sw, 8, float(p.exp) / p.expNext, rgb(.3f, .85f, .5f));
    bar(20, 24, 260, 20, p.hp / p.maxHp, rgb(.85f, .2f, .25f));
    text(TextFormat("%d / %d", int(std::max(0.f, p.hp)), int(p.maxHp)), 30, 24, 20, WHITE);
    text(TextFormat("Lv %d", p.level), 20, 50, 24, WHITE);
    int secs = int(p.time);
    text(TextFormat("%02d:%02d", secs / 60, secs % 60), sw / 2, 20, 36, g.endTimes ? rgb(1, .3f, .3f) : WHITE, true);
    text(TextFormat("Kills %d", p.kills), sw - 200, 20, 22, WHITE);
    text(TextFormat("Silver %d", p.silver), sw - 200, 44, 22, rgb(.8f, .8f, .85f));
    text(TextFormat("Gold %d", p.gold), sw - 200, 68, 22, rgb(1, .8f, .1f));
    text(TextFormat("%s Lv.%d", WEAPON_NAMES[p.weapon.id], p.weapon.level), 20, sh - 34, 20, WHITE);
    text(TextFormat("%d FPS  %zu enemies", GetFPS(), g.enemies.size()), sw - 260, sh - 30, 18, Fade(WHITE, 0.6f));
}

Rectangle optionRect(int i) {
    float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
    float w = 320, h = 170, gap = 30, x0 = sw / 2 - (3 * w + 2 * gap) / 2;
    return {x0 + i * (w + gap), sh / 2 - h / 2 + 20, w, h};
}

void drawOverlay(const Game& g) {
    float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
    if (g.mode == Mode::Play) return;
    DrawRectangle(0, 0, int(sw), int(sh), Fade(BLACK, g.mode == Mode::Title ? 0.75f : 0.55f));
    switch (g.mode) {
        case Mode::Title:
            text("Convallaria", sw / 2, sh / 2 - 140, 72, rgb(.95f, .95f, .85f), true);
            text("Choose your starting weapon", sw / 2, sh / 2 - 30, 28, WHITE, true);
            text("[1] Wand        [2] Poison Aura", sw / 2, sh / 2 + 20, 28, rgb(1, .8f, .1f), true);
            text("WASD to move  -  Esc to pause", sw / 2, sh / 2 + 90, 20, Fade(WHITE, 0.7f), true);
            break;
        case Mode::LevelUp: {
            text("LEVEL UP!", sw / 2, sh / 2 - 150, 48, rgb(1, .8f, .1f), true);
            for (int i = 0; i < int(g.options.size()); i++) {
                Rectangle r = optionRect(i);
                const Option& o = g.options[i];
                bool hover = CheckCollisionPointRec(GetMousePosition(), r);
                DrawRectangleRec(r, hover ? rgb(.22f, .22f, .28f) : rgb(.13f, .13f, .17f));
                DrawRectangleLinesEx(r, 3, RARITIES[o.rarity].color);
                text(TextFormat("[%d]  %s", i + 1, RARITIES[o.rarity].id), r.x + 16, r.y + 14, 18, RARITIES[o.rarity].color);
                text(o.text.c_str(), r.x + 16, r.y + 56, 24, WHITE);
            }
            break;
        }
        case Mode::Paused:
            text("Paused", sw / 2, sh / 2 - 60, 56, WHITE, true);
            text("Esc to resume  -  Q to quit", sw / 2, sh / 2 + 10, 24, Fade(WHITE, 0.8f), true);
            break;
        case Mode::GameOver: {
            int secs = int(g.p.time);
            text("You Died", sw / 2, sh / 2 - 100, 64, rgb(1, .3f, .3f), true);
            text(TextFormat("Survived %02d:%02d  -  Level %d  -  %d kills", secs / 60, secs % 60, g.p.level, g.p.kills), sw / 2, sh / 2, 26, WHITE, true);
            text("R to retry  -  Esc for title", sw / 2, sh / 2 + 60, 22, Fade(WHITE, 0.8f), true);
            break;
        }
        default: break;
    }
}

void handleInput(Game& g) {
    switch (g.mode) {
        case Mode::Title:
            if (IsKeyPressed(KEY_ONE)) newRun(g, WAND);
            if (IsKeyPressed(KEY_TWO)) newRun(g, POISON_AURA);
            if (IsKeyPressed(KEY_ESCAPE)) g.quit = true;
            break;
        case Mode::Play:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) g.mode = Mode::Paused;
            break;
        case Mode::Paused:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) g.mode = Mode::Play;
            if (IsKeyPressed(KEY_Q)) g.mode = Mode::Title;
            break;
        case Mode::LevelUp:
            for (int i = 0; i < int(g.options.size()); i++)
                if (IsKeyPressed(KEY_ONE + i) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), optionRect(i)))) {
                    applyOption(g, g.options[i]);
                    g.mode = Mode::Play;
                    break;
                }
            break;
        case Mode::GameOver:
            if (IsKeyPressed(KEY_R)) newRun(g, g.p.weapon.id);
            if (IsKeyPressed(KEY_ESCAPE)) g.mode = Mode::Title;
            break;
    }
}

#define CHECK(c) if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); return 1; }
int selftest() {
    CHECK(expForLevel(2) == 31);
    CHECK(fillText("+%s%% Max HP", 22.5f) == "+22.5% Max HP");
    CHECK(fillText("+%s%% Max HP", 15) == "+15% Max HP");
    CHECK(fillText("Glass Cannon: +40% Dmg, -20% HP", 40) == "Glass Cannon: +40% Dmg, -20% HP");
    std::vector<Enemy> es(3);
    es[0].pos = {0, 0}, es[1].pos = {20, 0}, es[2].pos = {500, 500};
    Grid grid;
    grid.build(es);
    int found = 0;
    grid.query({0, 0}, 25, [&](int i) { found += i == 0 || i == 1; return i == 2; });
    CHECK(found == 2);
    for (int i = 0; i < 1000; i++) { int t = weightedEnemy(0); CHECK(t == BASIC || t == RATMAN); }
    puts("selftest ok");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "--test")) return selftest();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "Convallaria");
    InitAudioDevice();
    SetExitKey(KEY_NULL);
    loadAssets();
    SetMusicVolume(A.music, 0.4f);
    PlayMusicStream(A.music);

    Game g;
    g.map = genMap();
    g.p.pos = g.map.spawn;

    while (!WindowShouldClose() && !g.quit) {
        float dt = std::min(GetFrameTime(), 1 / 30.f);
        UpdateMusicStream(A.music);
        handleInput(g);
        if (g.mode == Mode::Play) {
            g.grid.build(g.enemies);
            updatePlayer(g, dt);
            updateSpawner(g, dt);
            updateWeapon(g, dt);
            updateShots(g, dt);
            updateSeeds(g, dt);
            updateEnemies(g, dt);  // last: removes enemies, invalidating grid indices
            for (DamageNumber& n : g.numbers) n.t += dt;
            std::erase_if(g.numbers, [](const DamageNumber& n) { return n.t > 0.6f; });
        }

        Camera2D cam{{GetScreenWidth() / 2.f, GetScreenHeight() / 2.f}, g.p.pos, 0, ZOOM};
        BeginDrawing();
        ClearBackground(WATER);
        drawWorld(g, cam);
        if (g.mode != Mode::Title) drawHud(g);
        drawOverlay(g);
        EndDrawing();
    }
    CloseAudioDevice();
    CloseWindow();
}
