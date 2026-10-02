// Convallaria: top-down survival roguelite, C++ / raylib.
#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "data.h"

namespace {

constexpr int SCREEN_W = 1280, SCREEN_H = 720;
constexpr float ZOOM = 2.f;
constexpr int TILE = 16, MAP_R = 120, BOUND = MAP_R + 15, MAP_N = BOUND * 2;  // MAP_R: largest MapConfig radius
constexpr float SPAWN_RADIUS = 800, ACTIVE_RADIUS = 1000, DESPAWN_RADIUS = 3000;
constexpr int MAX_ENEMIES = 300, END_TIMES_EXTRA_CAP = 50;
constexpr float PLAYER_RADIUS = 9, MAGNET_RADIUS = 60, PICKUP_RADIUS = 15, PORTAL_RANGE = 30;
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

float musicVolume = 1, sfxVolume = 1;  // set from the profile by applyVolume

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
        SetSoundVolume(s, db(volumeDb) * sfxVolume);
        PlaySound(s);
    }
};

Sound loadClip(const char* path, float from, float to) {
    Wave w = LoadWave(asset(path));
    if (w.frameCount > unsigned(to * w.sampleRate)) WaveCrop(&w, int(from * w.sampleRate), int(to * w.sampleRate));
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

struct Assets {
    Texture2D characters[CHARACTER_COUNT], enemySprites[ENEMY_TYPE_COUNT], thorn, slime, ratman, guardian, portal, seed, projectile, aura, title, chest;
    Texture2D water, grass, soil, tree, titleBg, menuButton, weaponSlot, weaponIcons[WEAPON_COUNT], itemIcons[ITEM_COUNT];
    Sfx orb, levelup, hurt, wandShot, enemyShot, slimeHit, slimeDeath, ratmanDeath, win, hover, click;
    Music music{};
    std::string musicPath;
    float musicDb = 0;
} A;

void loadAssets() {
    for (int i = 0; i < CHARACTER_COUNT; i++) A.characters[i] = LoadTexture(asset(TextFormat("player/%s.png", CHARACTERS[i].sprite)));
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        if (ENEMIES[i].sprite) A.enemySprites[i] = LoadTexture(asset(TextFormat("enemies/%s.png", ENEMIES[i].sprite)));
    A.thorn = LoadTexture(asset("weapons/thorn/orb.png"));
    A.slime = LoadTexture(asset("enemies/blob.png"));
    A.ratman = LoadTexture(asset("enemies/ratman/ratman.png"));
    A.guardian = LoadTexture(asset("enemies/bob.png"));
    A.portal = LoadTexture(asset("world/portal.png"));
    A.seed = LoadTexture(asset("drops/exp/seed.png"));
    A.projectile = LoadTexture(asset("weapons/wand/projectile.png"));
    A.aura = LoadTexture(asset("weapons/poison/poison_radius..png"));
    A.chest = LoadTexture(asset("drops/chest/chest.png"));
    A.water = LoadTexture(asset("world/water.png"));
    A.grass = LoadTexture(asset("world/grass.png"));
    A.soil = LoadTexture(asset("world/soil.png"));
    A.tree = LoadTexture(asset("world/tree.png"));
    A.titleBg = LoadTexture(asset("ui/title_background.png"));
    A.menuButton = LoadTexture(asset("ui/menu_buttons.png"));
    A.weaponSlot = LoadTexture(asset("weapons/weaponslot.png"));
    A.weaponIcons[WAND] = LoadTexture(asset("weapons/wand/wand.png"));
    A.weaponIcons[POISON_AURA] = LoadTexture(asset("weapons/poison/poison.png"));
    A.weaponIcons[ORBIT] = LoadTexture(asset("weapons/thorn/thorn.png"));
    for (int i = 0; i < ITEM_COUNT; i++) A.itemIcons[i] = LoadTexture(asset(TextFormat("player/items/%s.png", ITEMS[i].icon)));
    A.orb.load(LoadSound(asset("player/orb.mp3")));
    A.levelup.load(LoadSound(asset("audio/levelup.wav")));
    A.hurt.load(LoadSound(asset("player/hurt.mp3")));
    A.slimeHit.load(LoadSound(asset("enemies/slime.ogg")));
    A.slimeDeath.load(LoadSound(asset("enemies/slime.ogg")));
    A.win.load(LoadSound(asset("ui/win.mp3")));
    A.title = LoadTexture(asset("ui/title.png"));
    A.hover.load(loadClip("ui/menu_hover.mp3", 0.62f, 10.f));
    A.click.load(loadClip("ui/menu_click.mp3", 0.62f, 10.f));
    // The Godot game played these clips from an offset.
    A.wandShot.load(loadClip("weapons/wand/shooting.mp3", 0.52f, 0.62f));
    A.enemyShot.load(loadClip("enemies/shooting.mp3", 0.52f, 0.62f));
    A.ratmanDeath.load(loadClip("enemies/ratman/ratman.ogg", 0.70f, 10.f));
}

void playMusic(const char* path, float volumeDb) {
    if (A.musicPath == path && IsMusicValid(A.music)) return;
    if (IsMusicValid(A.music)) UnloadMusicStream(A.music);
    A.music = LoadMusicStream(asset(path));
    A.musicPath = path;
    A.musicDb = volumeDb;
    SetMusicVolume(A.music, db(volumeDb) * musicVolume);
    PlayMusicStream(A.music);
}

void stopMusic() {
    if (IsMusicValid(A.music)) UnloadMusicStream(A.music);
    A.music = Music{};
    A.musicPath.clear();
}

// ---------------------------------------------------------------- profile (coins + shop, saved to disk)

struct Profile {
    int coins = 0;
    WeaponId weapon = WAND;
    int character = 0;
    int levels[PERM_COUNT]{};
    float volume[3] = {1, 1, 1};  // master, music, sfx
} profile;

std::filesystem::path savePath() {
    const char* base = getenv("APPDATA");
    if (!base) base = getenv("HOME");
    return (base ? std::filesystem::path(base) / "Convallaria" : std::filesystem::path(".")) / "save.txt";
}

// Writes to a temp file first so a crash mid-save can't wipe the old save.
bool saveProfile(const Profile& pr, const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp);
        out << "coins " << pr.coins << "\nweapon " << int(pr.weapon) << "\ncharacter " << pr.character << "\n";
        out << "volume " << pr.volume[0] << " " << pr.volume[1] << " " << pr.volume[2] << "\n";
        for (int i = 0; i < PERM_COUNT; i++) out << "upgrade " << PERM_UPGRADES[i].id << " " << pr.levels[i] << "\n";
        if (!out) return false;
    }
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

Profile loadProfile(const std::filesystem::path& path) {
    Profile pr;
    std::ifstream in(path);
    std::string key;
    while (in >> key) {
        if (key == "coins") in >> pr.coins;
        else if (key == "weapon") { int w = 0; in >> w; pr.weapon = w >= 0 && w < WEAPON_COUNT ? WeaponId(w) : WAND; }
        else if (key == "character") { int c = 0; in >> c; pr.character = c >= 0 && c < CHARACTER_COUNT ? c : 0; }
        else if (key == "volume") {
            for (float& v : pr.volume) in >> v, v = std::clamp(v, 0.f, 1.f);
        } else if (key == "upgrade") {
            std::string id;
            int level = 0;
            in >> id >> level;
            for (int i = 0; i < PERM_COUNT; i++)
                if (id == PERM_UPGRADES[i].id) pr.levels[i] = std::clamp(level, 0, PERM_UPGRADES[i].maxLevel);
        }
        if (!in) break;
    }
    pr.coins = std::max(0, pr.coins);
    return pr;
}

void save() {
    if (!saveProfile(profile, savePath())) TraceLog(LOG_WARNING, "Could not write save file %s", savePath().string().c_str());
}

int upgradeCost(const Profile& pr, int i) {
    const PermUpgrade& u = PERM_UPGRADES[i];
    if (pr.levels[i] >= u.maxLevel) return -1;
    return int(u.baseCost * powf(u.costMult, float(pr.levels[i])));
}

bool buyUpgrade(Profile& pr, int i) {
    int cost = upgradeCost(pr, i);
    if (cost < 0 || pr.coins < cost) return false;
    pr.coins -= cost;
    pr.levels[i]++;
    return true;
}

int respecRefund(const Profile& pr) {
    int total = 0;
    for (int i = 0; i < PERM_COUNT; i++)
        for (int l = 0; l < pr.levels[i]; l++) total += int(PERM_UPGRADES[i].baseCost * powf(PERM_UPGRADES[i].costMult, float(l)));
    return total;
}

void respec(Profile& pr) {
    pr.coins += respecRefund(pr);
    std::fill(std::begin(pr.levels), std::end(pr.levels), 0);
}

float permBoost(const Profile& pr, PermId id) { return pr.levels[id] * PERM_UPGRADES[id].boost; }

// ---------------------------------------------------------------- map

// The island is a 16 px tile grid drawn with the Godot tilemap art: animated water,
// then grass and soil autotiles. Edge tiles only fill the quarters whose three
// neighbours are land, so a point is walkable when the four tiles around its
// nearest tile corner are all land; that is exactly the grass you see.
// A tree's trunk blocks movement within TREE_W x TREE_H of its base.
constexpr float TREE_W = 17, TREE_H = 8;

struct Map {
    const MapConfig* cfg = &MAPS[0];
    std::vector<uint8_t> field;  // height per tile, 0-255
    std::vector<int8_t> grass, soil;  // autotile atlas index per tile, -1 for none
    std::vector<int> treeAt;  // index into trees per tile, -1 for none
    std::vector<Vector2> trees;
    uint8_t cut = 0;
    Vector2 spawn{}, portal{};
    std::vector<Vector2> chests;
    Color water = WHITE, grassTint = WHITE, soilTint = WHITE;

    static int tileOf(float v) { return int(floorf(v / TILE)) + MAP_N / 2; }
    bool land(int x, int y) const { return x >= 0 && y >= 0 && x < MAP_N && y < MAP_N && field[y * MAP_N + x] > cut; }
    bool at(Vector2 p) const {
        int vx = int(roundf(p.x / TILE)) + MAP_N / 2, vy = int(roundf(p.y / TILE)) + MAP_N / 2;
        return land(vx - 1, vy - 1) && land(vx, vy - 1) && land(vx - 1, vy) && land(vx, vy);
    }
    bool blocked(Vector2 p) const {  // by a tree trunk
        if (treeAt.empty()) return false;
        for (int y = tileOf(p.y) - 1; y <= tileOf(p.y) + 1; y++)
            for (int x = tileOf(p.x) - 1; x <= tileOf(p.x) + 1; x++) {
                int t = x >= 0 && y >= 0 && x < MAP_N && y < MAP_N ? treeAt[y * MAP_N + x] : -1;
                if (t >= 0 && fabsf(p.x - trees[t].x) < TREE_W && fabsf(p.y - trees[t].y) < TREE_H) return true;
            }
        return false;
    }
    // Something already inside a trunk (spawned or pushed there) may walk out.
    bool walk(Vector2 from, Vector2 to) const { return at(to) && (!blocked(to) || blocked(from)); }
    bool landAround(Vector2 p, int r) const {
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (!at({p.x + dx * TILE, p.y + dy * TILE})) return false;
        return true;
    }
};

int cellIndex(Vector2 p) { return std::clamp(Map::tileOf(p.y), 0, MAP_N - 1) * MAP_N + std::clamp(Map::tileOf(p.x), 0, MAP_N - 1); }

// Godot terrain peering bits of each tile in world/grass.png and soil.png (11 x 5 atlas,
// -1 unused). Bits: right, bottom-right, bottom, bottom-left, left, top-left, top, top-right.
constexpr int AUTOTILE[5][11] = {
    {7, 31, 28, 4, 5, 29, 23, 20, 21, 221, -1},
    {199, 255, 124, 68, 197, 253, 247, 116, 245, 119, -1},
    {193, 241, 112, 64, 71, 127, 223, 92, 95, 87, 93},
    {1, 17, 16, 0, 65, 113, 209, 80, 81, 213, 117},
    {255, 255, 255, 255, 69, 125, 215, 84, 85, -1, -1},
};

// Picks the atlas tile for each set cell from its 8 neighbours, as Godot's
// set_cells_terrain_connect does; corners only count when both sides do.
std::vector<int8_t> autotile(const std::vector<bool>& set) {
    static const std::array<int8_t, 256> lookup = [] {
        std::array<int8_t, 256> l{};
        for (int y = 4; y >= 0; y--)
            for (int x = 10; x >= 0; x--)  // first in reading order wins, so 255 is the plain 1:1
                if (AUTOTILE[y][x] >= 0) l[AUTOTILE[y][x]] = int8_t(y * 11 + x);
        return l;
    }();
    std::vector<int8_t> out(set.size(), -1);
    auto on = [&](int x, int y) { return x >= 0 && y >= 0 && x < MAP_N && y < MAP_N && set[y * MAP_N + x]; };
    for (int y = 0; y < MAP_N; y++)
        for (int x = 0; x < MAP_N; x++) {
            if (!set[y * MAP_N + x]) continue;
            bool r = on(x + 1, y), b = on(x, y + 1), l = on(x - 1, y), t = on(x, y - 1);
            int m = r | (r && b && on(x + 1, y + 1)) << 1 | b << 2 | (b && l && on(x - 1, y + 1)) << 3 | l << 4 |
                    (l && t && on(x - 1, y - 1)) << 5 | t << 6 | (t && r && on(x + 1, y - 1)) << 7;
            int8_t tile = lookup[m];
            if (m == 255 && rnd() < 0.4f / 1.4f) tile = int8_t(44 + rndi(4));  // the four plain variants weigh 0.1 each
            out[y * MAP_N + x] = tile;
        }
    return out;
}

// A tile corner is walkable when its four tiles are land (see Map::at); its index is its bottom-right tile's.
bool walkable(const Map& m, int v) { int x = v % MAP_N, y = v / MAP_N; return m.land(x - 1, y - 1) && m.land(x, y - 1) && m.land(x - 1, y) && m.land(x, y); }

// Walkable corners 4-connected to `start` (movement is per axis).
std::vector<int> landmass(const Map& m, int start, std::vector<bool>& seen) {
    std::vector<int> out, stack;
    if (seen[start] || !walkable(m, start)) return out;
    stack.push_back(start);
    seen[start] = true;
    while (!stack.empty()) {
        int c = stack.back();
        stack.pop_back();
        out.push_back(c);
        for (int n : {c - 1, c + 1, c - MAP_N, c + MAP_N})
            if (!seen[n] && walkable(m, n)) seen[n] = true, stack.push_back(n);
    }
    return out;
}

int vertexIndex(Vector2 p) { return (int(roundf(p.y / TILE)) + MAP_N / 2) * MAP_N + int(roundf(p.x / TILE)) + MAP_N / 2; }
Vector2 vertexPos(int v) { return {float(v % MAP_N - MAP_N / 2) * TILE, float(v / MAP_N - MAP_N / 2) * TILE}; }

// Same island recipe as the Godot map: fbm Perlin noise with a radial falloff, land
// above the cut and soil above 0.6. Only the biggest walkable region is kept so the
// portal and chests are always reachable; trees cover 1.5% of it like map.gd.
Map genMap(const MapConfig& cfg) {
    Map m;
    m.cfg = &cfg;
    const Biome& biome = BIOMES[cfg.biome];
    m.water = biome.water, m.grassTint = biome.grass, m.soilTint = biome.soil;
    m.cut = uint8_t(cfg.cut * 255);
    float radius = float(cfg.radius);
    m.field.assign(MAP_N * MAP_N, 0);
    Image noise = GenImagePerlinNoise(MAP_N, MAP_N, rndi(100000), rndi(100000), cfg.noiseScale * MAP_N);
    Color* px = LoadImageColors(noise);
    for (int y = 0; y < MAP_N; y++)
        for (int x = 0; x < MAP_N; x++) {
            float dx = x + 0.5f - MAP_N / 2, dy = y + 0.5f - MAP_N / 2, d = sqrtf(dx * dx + dy * dy) / radius;
            if (d >= 1) continue;
            float falloff = 1.f - powf(fabsf(d - cfg.ring) / (1.f - cfg.ring), 2.5f);
            m.field[y * MAP_N + x] = uint8_t(std::clamp(px[y * MAP_N + x].r / 255.f * falloff, 0.f, 1.f) * 255);
        }
    UnloadImageColors(px);
    UnloadImage(noise);

    // Sink land that doesn't touch the biggest region; repeat in case that strands a pocket.
    std::vector<int> biggest;
    for (int pass = 0; pass < 8; pass++) {
        std::vector<bool> seen(m.field.size());
        biggest.clear();
        int total = 0;
        for (int v = MAP_N + 1; v < MAP_N * (MAP_N - 1); v++) {
            std::vector<int> part = landmass(m, v, seen);
            total += int(part.size());
            if (part.size() > biggest.size()) biggest.swap(part);
        }
        if (total == int(biggest.size())) break;
        std::vector<bool> keep(m.field.size());
        for (int v : biggest)
            for (int t : {v, v - 1, v - MAP_N, v - MAP_N - 1}) keep[t] = true;
        for (int i = 0; i < int(m.field.size()); i++)
            if (!keep[i]) m.field[i] = std::min(m.field[i], m.cut);
    }

    std::vector<bool> grass(m.field.size()), soil(m.field.size());
    for (int i = 0; i < int(m.field.size()); i++) grass[i] = m.field[i] > m.cut, soil[i] = m.field[i] > 153;
    m.grass = autotile(grass);
    m.soil = autotile(soil);

    std::vector<Vector2> land;
    float best = 1e9f;
    for (int v : biggest) {
        land.push_back(vertexPos(v));
        float d2 = Vector2LengthSqr(land.back());
        if (d2 < best) best = d2, m.spawn = land.back();
    }
    // Tiles kept clear of trees: around the spawn, the portal and each chest.
    std::vector<bool> reserved(m.field.size());
    auto reserve = [&](Vector2 c, int r) {
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) reserved[cellIndex({c.x + dx * TILE, c.y + dy * TILE})] = true;
    };
    reserve(m.spawn, 5);
    m.portal = m.spawn;
    for (int a = 0; a < 200 && !land.empty(); a++) {
        Vector2 c = land[rndi(int(land.size()))];
        if (m.landAround(c, 3) && Vector2Distance(c, m.spawn) > 9 * TILE) { m.portal = c; break; }
    }
    reserve(m.portal, 3);
    for (int a = 0; a < cfg.chests * 20 && int(m.chests.size()) < cfg.chests && !land.empty(); a++) {
        Vector2 c = land[rndi(int(land.size()))];
        if (m.landAround(c, 1) && Vector2Distance(c, m.spawn) > 6 * TILE && Vector2Distance(c, m.portal) > 4 * TILE)
            m.chests.push_back(c), reserve(c, 1);
    }
    m.treeAt.assign(m.field.size(), -1);
    int target = int(land.size() * 0.015f);
    for (int a = 0; a < target * 4 && int(m.trees.size()) < target; a++) {
        Vector2 c = Vector2Add(land[rndi(int(land.size()))], {TILE / 2.f, TILE / 2.f});  // a tile centre
        int i = cellIndex(c);
        if (reserved[i] || !m.landAround(c, 1)) continue;
        reserve(c, 1);
        m.treeAt[i] = int(m.trees.size());
        m.trees.push_back(Vector2Add(c, {rndr(-6, 6), rndr(-6, 6)}));
    }
    return m;
}

// ---------------------------------------------------------------- entities

enum EnemyKind { NORMAL, GUARDIAN, FINAL_BOSS };

struct Enemy {
    uint32_t id = 0;
    EnemyKind kind = NORMAL;
    Vector2 pos{}, vel{}, dashDir{};
    float hp = 1, maxHp = 1, speed = 0, slowMul = 1, scale = 1, pitch = 1;
    Color color = WHITE, glow = WHITE;
    int type = BASIC, damage = 0, exp = 0, phase = 1, portal = -1, lastHit = 0;
    bool shooter = false, ratman = false, dying = false, gone = false, enraged = false, fired = false;
    int facing = DOWN, burnTicks = 0;
    float anim = 0, hurtT = 0, deathT = 0, shootT = 0, burnT = 0, burnDamage = 0, slowT = 0;
    float castT = 0, dashT = 0, transformT = 0, specialT = 0, specialWait = 0, orbitT = 0;
    float flank = 0;  // radians off the direct line while closing in, so packs surround
    float radius() const { return 6.f * scale; }
    bool boss() const { return kind != NORMAL; }
    float missing() const { return 1.f - hp / maxHp; }
};

struct Shot {
    Vector2 pos, dir;
    float speed, size, life;
    int damage, pierce, ricochet;
    bool fire, frost, crit;
    std::vector<uint32_t> hit;
};
struct EnemyShot { Vector2 pos, dir; int damage; float life, scale; Color color; };
enum SeedType { SEED_EXP, SEED_MAGNET, SEED_SPEED, SEED_BOMB, SEED_GOLD, SEED_SILVER, SEED_HEAL };
struct Seed { Vector2 pos; int type, amount; float life, speed = 0, glimmer; bool magnetic = false; };
struct DamageNumber { Vector2 pos; int value; float t; };
struct Chest { Vector2 pos; int cost; };
struct Blast { Vector2 pos; float radius; int damage; float t; };  // queued, lands next frame
struct Bolt { Vector2 a, b; float t; };
struct Portal { Vector2 pos; enum { CORRUPTED, COMBAT, PURIFIED } state = CORRUPTED; };

struct Weapon {
    WeaponId id = WAND;
    int level = 1, pierce = 0, ricochet = 0, projectile = 0;
    float damage = 1, size = 1, fireRate = 1, timer = 0, pulse = 0, spin = 0;
    const WeaponLevel& stats() const { return WEAPON_LEVELS[id][std::min(level, 3) - 1]; }
};

struct Player {
    Vector2 pos{};
    float speed = 165, maxHp = 250, hp = 250;
    int level = 1, exp = 0, expNext = 15, kills = 0, silver = 0, pendingLevels = 0;
    float dmgMul = 1, fireRateMul = 1, aoe = 1, expMul = 1, regen = 0, regenAcc = 0;
    float thorns = 0, evasion = 0, crit = 0, vampirism = 0, magnetScale = 1, coinMul = 1;
    float gold = 0;  // collected this run, banked into the profile when the run ends
    bool imbueFire = false, imbueFrost = false, moving = false;
    float magnetT = 0, magnetScan = 0, speedT = 0, iframes = 0, anim = 0;
    float time = 0;  // on this floor; the run total lives in Game::runTime
    int facing = DOWN, character = 0;
    std::vector<std::string> uniques;
    std::vector<Weapon> weapons;
    int items[ITEM_COUNT]{};
};

// Counting-sort uniform grid over enemies, rebuilt every frame. Serves hits,
// separation and contact damage.
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

enum OptionKind { OPT_UPGRADE, OPT_BUFF, OPT_WEAPON };
struct Option { int kind, index, weapon; float value; int rarity; std::string text; };
enum Buff { B_DAMAGE, B_FIRE_RATE, B_SIZE, B_RICOCHET, B_PROJECTILE };
enum class Mode { Title, Shop, Play, LevelUp, ItemGet, Paused, GameOver, Victory };

struct Game {
    Map map;
    Player p;
    std::vector<Enemy> enemies, pending;  // pending: spawned mid-update, appended after
    std::vector<Shot> shots;
    std::vector<EnemyShot> enemyShots;
    std::vector<Seed> seeds;
    std::vector<DamageNumber> numbers;
    std::vector<Option> options;
    std::vector<Portal> portals;
    std::vector<Chest> chests;
    std::vector<Blast> blasts;
    std::vector<Bolt> bolts;
    std::string toast;
    Color toastColor = WHITE;
    float toastT = 0;
    Grid grid;
    Mode mode = Mode::Title;
    uint32_t nextId = 1;
    int floor = 1;
    float runTime = 0;
    float spawnT = 1, spawnWait = 1, difficultyT = 5, deathT = 0, deathWait = DEATH_SLIME_BASE_INTERVAL;
    int spawnCount = 1, lastSecond = -1;
    bool endTimes = false, bossSummoned = false, bossDefeated = false, banked = false, quit = false;
    float finalBossT = -1, victoryT = -1;
    int runGold = 0, gotItem = -1;  // gotItem: shown by the item popup
};

int chestCost(int floor) { return 10 + 8 * (floor - 1); }

void showToast(Game& g, std::string text, Color c) { g.toast = std::move(text), g.toastColor = c, g.toastT = 3; }

void startFloor(Game& g, int floor, Player player, float runTime) {  // player by value: g is reset below
    const MapConfig* prev = g.map.cfg;
    g = Game{};
    g.floor = floor;
    g.runTime = runTime;
    g.p = player;
    g.p.time = 0;
    const MapConfig* cfg = &FINAL_MAP;
    if (floor == 1) cfg = &MAPS[0];
    else if (floor < MAX_FLOORS) do cfg = &MAPS[1 + rndi(int(std::size(MAPS)) - 1)]; while (cfg == prev);
    g.map = genMap(*cfg);
    g.p.pos = g.map.spawn;
    for (Vector2 c : g.map.chests) g.chests.push_back({c, chestCost(floor)});
    // Later floors open with the spawner already part-ramped instead of a quiet first minute.
    g.spawnWait = std::max(0.5f, 1 - 0.25f * (floor - 1)), g.spawnCount = floor;
    if (floor < MAX_FLOORS) g.portals.push_back({g.map.portal});
    else g.finalBossT = 2.5f;
    g.mode = Mode::Play;
    showToast(g, TextFormat("Floor %d  -  %s", floor, cfg->name), WHITE);
    playMusic("ui/music.mp3", -8);
}

// A fresh character with the shop upgrades applied.
void newRun(Game& g) {
    Player p;
    p.weapons.push_back({profile.weapon});
    p.character = profile.character;
    p.maxHp += permBoost(profile, PERM_MAX_HP);
    p.hp = p.maxHp;
    p.dmgMul += permBoost(profile, PERM_DAMAGE);
    p.speed += permBoost(profile, PERM_SPEED);
    p.regen += permBoost(profile, PERM_REGEN);
    p.thorns += permBoost(profile, PERM_ARMOR);
    p.evasion += permBoost(profile, PERM_EVASION);
    p.coinMul += permBoost(profile, PERM_GREED);
    p.expMul += permBoost(profile, PERM_EXP_GAIN);
    startFloor(g, 1, p, 0);
}

// Moves the gold collected this run into the saved profile, once per run.
void bankRun(Game& g) {
    if (g.banked) return;
    g.banked = true;
    g.runGold = int(g.p.gold);
    profile.coins += g.runGold;
    g.p.gold = 0;
    save();
}

float runMinutes(const Game& g) { return g.runTime / 60.f; }

void playAt(Sfx& s, const Game& g, Vector2 at, float pitch, float volumeDb, double throttle) {
    float fade = 1.f - Vector2Distance(at, g.p.pos) / 800.f;
    if (fade > 0) s.play(pitch, volumeDb + 20.f * log10f(fade), throttle);
}

// ---------------------------------------------------------------- combat

void enrage(Game& g, Enemy& e) {
    e.phase = 2, e.enraged = true, e.transformT = 1.2f, e.castT = 0, e.dashT = 0;
    if (e.kind == GUARDIAN) {
        const FloorBoss& fb = FLOOR_BOSSES[std::min(g.floor, 3) - 1];
        e.color = fb.enragedColor, e.glow = fb.glow;
        e.speed *= fb.speedMult;
        e.damage = int(e.damage * fb.damageMult);
        e.specialWait = fb.specialWait;
        e.scale *= 1.3f;
    } else {
        e.color = rgb(0.8f, 0.1f, 0.1f), e.glow = rgb(1, 0.2f, 0);
        e.speed *= 1.3f;
        e.damage = int(e.damage * 1.3f);
        e.scale *= 1.25f;
    }
}

void hurtEnemy(Game& g, Enemy& e, int amount) {
    if (e.dying) return;
    e.hp -= amount;
    e.lastHit = amount;
    g.numbers.push_back({e.pos, amount, 0});
    if (e.boss()) {
        if (e.phase == 1 && e.hp <= e.maxHp / 2 && e.hp > 0) enrage(g, e);
        if (e.kind == FINAL_BOSS) e.specialWait = std::max(1.5f, 7.f - e.missing() * 5.5f);
        if (IsMusicValid(A.music)) SetMusicPitch(A.music, 1.f + e.missing() * (e.kind == FINAL_BOSS ? 0.6f : 0.5f));
    }
    if (e.hp <= 0) {
        e.dying = true;
        e.deathT = e.ratman ? 0.8f : 0.4f;
        if (e.ratman) playAt(A.ratmanDeath, g, e.pos, e.pitch * rndr(0.9f, 1.1f), e.boss() ? -10 : -15, 0.05);
        else playAt(A.slimeDeath, g, e.pos, e.pitch, e.boss() ? 0 : -10, 0.05);
        if (e.boss()) stopMusic();
    } else if (e.hurtT <= 0 && e.castT <= 0 && e.transformT <= 0 && e.dashT <= 0) {
        e.hurtT = e.ratman || e.boss() ? 0.15f : 0.25f;
        if (!e.ratman && !e.boss()) e.vel = Vector2Scale(Vector2Normalize(Vector2Subtract(e.pos, g.p.pos)), 140 / e.scale);  // knockback
        if (!e.ratman) playAt(A.slimeHit, g, e.pos, e.pitch, -5, 0.05);
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

// Chance that grows with stacks but never reaches 1 (Risk of Rain's hyperbolic stacking).
float hyper(float per, int stacks) { return 1.f - 1.f / (1.f + per * stacks); }

void hurtPlayer(Game& g, int damage, Enemy* source) {
    Player& p = g.p;
    if (p.iframes > 0) return;
    if (rnd() < std::min(p.evasion, 0.6f) || rnd() < hyper(0.15f, p.items[IT_CHARM])) return;  // dodge caps at 60%
    p.hp -= damage;
    A.hurt.play(rndr(1.4f, 1.8f), -5);
    if (source && p.thorns > 0) hurtEnemy(g, *source, int(damage * p.thorns));
    if (p.hp <= 0) g.mode = Mode::GameOver, bankRun(g);
    else p.iframes = 0.5f;
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
        p.pendingLevels++;  // one pick per level, even when several land at once
        leveled = true;
    }
    if (leveled && g.mode == Mode::Play) {  // not over a death or an open pick
        A.levelup.play(1, -12);
        buildOptions(g);
        g.mode = Mode::LevelUp;
    }
}

void heal(Player& p, float amount) { p.hp = std::min(p.hp + amount, p.maxHp); }

int rollItem() {
    int roll = rndi(100), tier = 0;
    for (int acc = 0; tier < 2 && roll >= (acc += ITEM_TIERS[tier].weight); tier++) {}
    std::vector<int> pool;
    for (int i = 0; i < ITEM_COUNT; i++)
        if (ITEMS[i].tier == tier) pool.push_back(i);
    return pool[rndi(int(pool.size()))];
}

void grantItem(Game& g, int id) {
    Player& p = g.p;
    p.items[id]++;
    if (id == IT_BARK) p.maxHp += 30, p.hp += 30;
    g.gotItem = id;
    if (g.mode == Mode::Play) g.mode = Mode::ItemGet;  // the Godot ItemGetPopup pauses
    A.levelup.play(1.5f, -10);
}

// Storm Bell: arcs to the closest few enemies around `from`.
void zap(Game& g, const Enemy& from, int damage, int targets) {
    std::vector<std::pair<float, int>> near;
    g.grid.query(from.pos, 150, [&](int i) {
        const Enemy& e = g.enemies[i];
        float d = Vector2Distance(e.pos, from.pos);
        if (&e != &from && !e.dying && d <= 150) near.push_back({d, i});
        return false;
    });
    std::sort(near.begin(), near.end());
    for (int k = 0; k < std::min(targets, int(near.size())); k++) {
        Enemy& e = g.enemies[near[k].second];
        g.bolts.push_back({from.pos, e.pos, 0});
        hurtEnemy(g, e, damage);
    }
}

// A weapon hit: damage plus every on-hit effect. Effects call hurtEnemy
// directly so they can't proc each other.
void onHit(Game& g, Enemy& e, int damage, bool fire, bool frost) {
    const Player& p = g.p;
    hurtEnemy(g, e, damage);
    if (fire || rnd() < 0.15f * p.items[IT_EMBER]) applyBurn(e, damage * 0.2f);
    if (frost) applySlow(e, 0.5f);
    if (p.items[IT_LEECH]) heal(g.p, float(p.items[IT_LEECH]));
    if (!e.dying && !e.boss() && e.hp < e.maxHp * hyper(0.15f, p.items[IT_SICKLE])) hurtEnemy(g, e, int(ceilf(e.hp)));
    if (p.items[IT_BELL] && rnd() < 0.2f) zap(g, e, std::max(1, int(damage * 0.6f)), 1 + 2 * p.items[IT_BELL]);
}

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

int weightedEnemy(std::span<const SpawnChance> table, float minutes) {
    auto weight = [&](const SpawnChance& c) { return std::max(0.f, c.base + c.growth * minutes); };
    float total = 0;
    for (const SpawnChance& c : table) total += weight(c);
    if (total <= 0) return BASIC;
    float roll = rnd() * total, acc = 0;
    for (const SpawnChance& c : table)
        if (roll <= (acc += weight(c))) return c.type;
    return BASIC;
}

Enemy makeEnemy(Game& g, Vector2 pos) {
    Enemy e;
    e.id = g.nextId++;
    e.pos = pos;
    e.shootT = rndr(2, 4);
    e.anim = rndr(0, 1);
    e.flank = rndr(-0.7f, 0.7f);
    return e;
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
    if (type < 0) type = weightedEnemy(g.map.cfg->spawns, runMinutes(g));

    const EnemyDef& d = ENEMIES[type];
    float floorMult = 1 + (g.floor - 1) * 0.3f;
    Enemy e = makeEnemy(g, pos);
    if (g.p.time >= STAGE_DURATION || g.bossDefeated || d.deathSlime) {
        // Overtime: death slimes keep scaling the longer you stay.
        float over = std::max(0.f, g.p.time - STAGE_DURATION);
        if (g.bossDefeated && over == 0) over = 30;
        float om = over / 60.f, c = std::max(0.f, 0.4f - over * 0.003f);
        e.hp = float(int(int((30 + over * 25) * floorMult) * (1 + om * 0.5f)));
        e.damage = int(int((12 + over * 1.5f) * floorMult) * (1 + om * 0.5f));
        e.speed = (80 + over * 2) * (1 + om * 0.2f);
        e.scale = (1.2f + over * 0.015f) * (1 + om * 0.1f);
        e.exp = int((10 + over * 0.5f) * floorMult);
        e.color = rgb(c, 0, c);
        e.pitch = std::max(0.2f, 1.f - over * 0.01f);
    } else {
        float m = runMinutes(g), floorHp = 1 + (g.floor - 1) * ENEMY_HP_PER_FLOOR;
        e.hp = float(int(d.health * floorHp * (1 + m * 0.3f)));
        e.damage = int(d.damage * (1 + (g.floor - 1) * ENEMY_DMG_PER_FLOOR) * (1 + m * ENEMY_DMG_PER_MIN));
        e.speed = d.speed + m * ENEMY_SPEED_PER_MIN + (g.floor - 1) * ENEMY_SPEED_PER_FLOOR;
        e.scale = d.scale;
        e.exp = int(d.exp * floorMult);
        e.color = d.color;
        e.pitch = d.pitch;
        e.shooter = d.shooter;
        e.ratman = d.ratman;
        if (e.ratman) {
            float v = rnd();
            if (v > 0.75f) e.scale *= 1.25f, e.hp = float(int(e.hp * 1.5f)), e.speed *= 0.75f, e.color = rgb(0.7f, 0.6f, 0.6f);
            else if (v < 0.25f) e.scale *= 0.8f, e.hp = float(int(e.hp * 0.6f)), e.speed *= 1.4f, e.color = rgb(1.2f, 1.1f, 1.1f);
        }
        e.speed = std::min(e.speed, ENEMY_SPEED_CAP);
    }
    e.type = type;
    e.maxHp = e.hp;
    g.enemies.push_back(e);
    return true;
}

void startEndTimes(Game& g) {
    g.endTimes = true;
    g.deathT = g.deathWait;
    if (!g.bossSummoned) {
        // Last chance: a portal opens near the player.
        for (int a = 0; a < 200; a++) {
            float ang = rndr(0, 2 * PI);
            Vector2 at = Vector2Add(g.p.pos, {cosf(ang) * SPAWN_RADIUS * 0.5f, sinf(ang) * SPAWN_RADIUS * 0.5f});
            if (g.map.landAround(at, 3)) { g.portals.push_back({at}); break; }
        }
    }
    playMusic("enemies/bob.mp3", -10);
}

void summonGuardian(Game& g, int portal) {
    Portal& pt = g.portals[portal];
    pt.state = Portal::COMBAT;
    g.bossSummoned = true;

    std::vector<Vector2> spots;
    for (int dy = -8; dy <= 8; dy++)
        for (int dx = -8; dx <= 8; dx++) {
            Vector2 at = {pt.pos.x + dx * TILE, pt.pos.y + dy * TILE};
            if (g.map.at(at) && Vector2Distance(at, g.p.pos) > 150) spots.push_back(at);
        }
    Enemy e = makeEnemy(g, spots.empty() ? pt.pos : spots[rndi(int(spots.size()))]);
    const FloorBoss& fb = FLOOR_BOSSES[std::min(g.floor, 3) - 1];
    float m = runMinutes(g);
    int f = g.floor;
    e.kind = GUARDIAN;
    e.portal = portal;
    e.hp = float(int(int(BOSS_HEALTH * (1 + m * 0.2f)) * (1 + (f - 1) * 2.5f + m * 0.25f)));
    e.maxHp = e.hp;
    e.damage = int(BOSS_DAMAGE * (1 + (f - 1) * ENEMY_DMG_PER_FLOOR) * (1 + m * 0.05f));
    e.speed = BOSS_SPEED + (f - 1) * 20;
    e.scale = 2;
    e.color = fb.color;
    e.pitch = 0.3f;
    e.specialWait = e.specialT = 6;
    g.enemies.push_back(e);
    playMusic(fb.music, -10);
}

void spawnFinalBoss(Game& g) {
    Enemy e = makeEnemy(g, Vector2Add(g.p.pos, {0, -500}));
    float m = runMinutes(g), mult = 1 + m * 0.5f + g.floor * 0.5f;
    e.kind = FINAL_BOSS;
    e.ratman = e.shooter = true;
    e.hp = float(int(int(BOSS_HEALTH * mult * 5) * 0.35f * (1 + m * 0.4f)));
    e.maxHp = e.hp;
    e.damage = int(BOSS_DAMAGE * 1.2f * (1 + m * 0.04f));
    e.speed = BOSS_SPEED * 1.5f + m * 2;
    e.scale = 4.5f;
    e.color = rgb(0.8f, 0.05f, 0.1f);
    e.pitch = 0.5f;
    e.exp = 1500;
    e.shootT = rndr(2, 3.5f);
    e.specialWait = e.specialT = 7;
    g.enemies.push_back(e);
    playMusic("world/finalboss.ogg", -8);
}

void bossShot(Game& g, const Enemy& e, Vector2 dir, int damage, float scale, Color color) {
    g.enemyShots.push_back({e.pos, dir, damage, 4, scale, color});
    playAt(A.enemyShot, g, e.pos, 1, -9, 0.05);
}

void guardianSpecial(Game& g, Enemy& e, Vector2 dir) {
    int n = e.enraged ? 8 : 4;
    for (int k = 0; k < n; k++)
        bossShot(g, e, Vector2Rotate({1, 0}, k * 2 * PI / n), int(e.damage * 0.4f), e.enraged ? 1.2f : 1, e.color);
    for (int k = 0; k < (e.enraged ? 2 : 1); k++) {
        Enemy m = makeEnemy(g, Vector2Add(e.pos, {rndr(-100, 100), rndr(-100, 100)}));
        m.hp = m.maxHp = float(int(e.maxHp * 0.03f) + 10);
        m.speed = e.speed * 1.1f;
        m.damage = int(e.damage * 0.3f);
        m.scale = 0.6f;
        m.color = e.color;
        g.pending.push_back(m);
    }
    e.dashT = 0.35f;
    e.dashDir = dir;
}

void finalBossSpecial(Game& g, Enemy& e, Vector2 dir) {
    float miss = e.missing();
    int n = 5 + int(miss * 12) + (e.enraged ? 4 : 0);
    float spread = (15 + miss * 25) * DEG2RAD, scale = (1 + miss * 1.5f) * (e.enraged ? 1.2f : 1);
    for (int k = 0; k < n; k++)
        bossShot(g, e, Vector2Rotate(dir, (k - (n - 1) / 2.f) * spread), int(e.damage * (1 + scale * 0.4f)), scale,
                 e.enraged ? rgb(1, 0.2f, 0.2f) : rgb(0.8f, 0, 0.8f));
    for (int k = 0; k < 3 + int(miss * 6) + (e.enraged ? 3 : 0); k++) {
        Enemy m = makeEnemy(g, Vector2Add(e.pos, {rndr(-200, 200), rndr(-200, 200)}));
        m.hp = m.maxHp = float(80 + g.floor * 20);
        m.speed = e.speed * 1.1f;
        m.damage = int(e.damage * 0.3f);
        m.scale = 0.8f;
        m.color = rgb(0.3f, 0.3f, 0.3f);
        m.pitch = 1.5f;
        m.ratman = m.shooter = true;
        m.shootT = rndr(2, 3.5f);
        g.pending.push_back(m);
    }
}

// Boss movement and attack patterns, ported from boss.gd and ratman.gd.
void updateBoss(Game& g, Enemy& e, Vector2 dir, float dist, float dt) {
    if (e.transformT > 0) { e.transformT -= dt; e.vel = {}; return; }
    if (e.kind == GUARDIAN) {
        if (e.castT > 0) {
            e.vel = {};
            if ((e.castT -= dt) <= 0) guardianSpecial(g, e, dir);
        } else if (e.dashT > 0) {
            e.dashT -= dt;
            e.vel = Vector2Scale(e.dashDir, e.speed * 4);
        } else if (e.hurtT > 0) {
            e.hurtT -= dt;
            e.vel = Vector2Scale(e.vel, powf(0.95f, dt * 60));
        } else {
            e.vel = Vector2Scale(dir, e.speed * e.slowMul);
        }
        if (e.castT <= 0 && e.dashT <= 0 && (e.specialT -= dt) <= 0) {
            e.specialT = e.specialWait;
            if (e.hurtT <= 0) e.castT = 0.4f;
        }
        return;
    }
    // Final boss: casts are 0.5s wind-up, burst, 0.5s recovery.
    e.hurtT -= dt;
    if (e.castT > 0) {
        e.vel = {};
        e.castT -= dt;
        if (!e.fired && e.castT <= 0.5f) { e.fired = true; finalBossSpecial(g, e, dir); }
        return;
    }
    float miss = e.missing();
    e.vel = Vector2Scale(dir, e.speed * e.slowMul + miss * 80);
    if (dist <= 400 + miss * 200 && (e.shootT -= dt) <= 0) {
        float s = 1 + miss * 0.8f;
        bossShot(g, e, dir, int(e.damage * (1 + s * 0.4f)), s, e.enraged ? rgb(1, 0.2f, 0.2f) : rgb(0.8f, 0, 0.8f));
        e.shootT = std::max(0.3f, (rndr(2, 3.5f) - miss * 2) * (e.enraged ? 0.5f : 1));
    }
    if ((e.specialT -= dt) <= 0) e.specialT = e.specialWait, e.castT = 1, e.fired = false;
}

// ---------------------------------------------------------------- level up

const char* BUFF_TEXT[] = {"+%s%% Damage", "+%s%% Fire Rate", "+%s%% Size", "+%s Bounce", "+%s Projectile"};
const char* SIZE_TEXT[WEAPON_COUNT] = {"+%s%% Splash Size", "+%s%% Aura Radius", "+%s%% Orbit Size"};
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

Weapon* findWeapon(Player& p, int id) {
    for (Weapon& w : p.weapons)
        if (w.id == id) return &w;
    return nullptr;
}

void buildOptions(Game& g) {
    Player& p = g.p;
    std::vector<Option> pool;  // value, rarity and text are filled in once picked
    for (int i = 0; i < int(std::size(UPGRADES)); i++)
        if (!(UPGRADES[i].unique && hasUnique(p, UPGRADES[i].id))) pool.push_back({OPT_UPGRADE, i, 0});
    for (const Weapon& w : p.weapons) {
        if (w.level >= 99) continue;
        std::vector<int> buffs = {B_DAMAGE, B_FIRE_RATE, B_SIZE};
        if (w.id == WAND) buffs.insert(buffs.end(), {B_RICOCHET, B_PROJECTILE});
        if (w.id == ORBIT) buffs.push_back(B_PROJECTILE);
        for (int i = 0; i < 3; i++) pool.push_back({OPT_BUFF, buffs[rndi(int(buffs.size()))], w.id});
    }
    for (int id = 0; id < WEAPON_COUNT; id++)
        if (!findWeapon(p, id)) pool.insert(pool.end(), 2, Option{OPT_WEAPON, id, id});
    std::shuffle(pool.begin(), pool.end(), rng);

    g.options.clear();
    for (Option o : pool) {
        if (g.options.size() >= 3) break;
        auto same = [&](const Option& x) { return x.kind == o.kind && x.index == o.index && x.weapon == o.weapon; };
        if (std::any_of(g.options.begin(), g.options.end(), same)) continue;
        o.rarity = rollRarity();
        if (o.kind == OPT_UPGRADE) {
            o.value = UPGRADES[o.index].base * RARITIES[o.rarity].mult;
            o.text = fillText(UPGRADES[o.index].text, o.value);
        } else if (o.kind == OPT_BUFF) {
            const Weapon& w = *findWeapon(p, o.weapon);
            o.value = BUFF_VALUE[o.index] * RARITIES[o.rarity].mult;
            float shown = o.index <= B_SIZE ? o.value * 100 : o.value;
            const char* text = o.index == B_SIZE ? SIZE_TEXT[w.id] : BUFF_TEXT[o.index];
            o.text = std::string(WEAPON_NAMES[w.id]) + " Lv." + std::to_string(w.level + 1) + "\n" + fillText(text, shown);
        } else {
            o.rarity = 2;
            o.text = std::string("New Weapon\n") + WEAPON_NAMES[o.index];
        }
        g.options.push_back(o);
    }
}

void applyOption(Game& g, const Option& o) {
    Player& p = g.p;
    float v = o.value, pct = v / 100.f;
    if (o.kind == OPT_WEAPON) {
        p.weapons.push_back({WeaponId(o.index)});
        return;
    }
    if (o.kind == OPT_BUFF) {
        Weapon& w = *findWeapon(p, o.weapon);
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
    else if (id == "multi_attack") for (Weapon& w : p.weapons) w.projectile += int(v);
    else if (id == "glass_cannon") { p.dmgMul += pct; p.maxHp -= p.maxHp * 0.2f; p.hp = std::min(p.hp, p.maxHp); }
    else if (id == "heavy_armor") { float inc = p.maxHp * 0.1f; p.thorns += pct; p.speed -= p.speed * 0.15f; p.maxHp += inc; p.hp += inc; }
    else if (id == "berserker") { fasterFire(pct); p.evasion = std::max(0.f, p.evasion - 0.1f); }
    else if (id == "vampiric_edge") p.vampirism += pct;
    else if (id == "magnet_training") p.magnetScale += pct;
    else if (id == "precision") { p.crit += pct; p.dmgMul += pct; }
    else if (id == "momentum") { p.speed += p.speed * pct; fasterFire(pct); }
    else if (id == "soul_harvest") { p.expMul += pct; p.regen += 1; }
    if (UPGRADES[o.index].unique && !hasUnique(p, id.c_str())) p.uniques.push_back(id);
}

// ---------------------------------------------------------------- update

int nearPortal(const Game& g) {
    for (int i = 0; i < int(g.portals.size()); i++)
        if (g.portals[i].state != Portal::COMBAT && Vector2Distance(g.portals[i].pos, g.p.pos) < PORTAL_RANGE) return i;
    return -1;
}

int nearChest(const Game& g) {
    for (int i = 0; i < int(g.chests.size()); i++)
        if (Vector2Distance(g.chests[i].pos, g.p.pos) < PORTAL_RANGE) return i;
    return -1;
}

void openChest(Game& g, int i) {
    Chest c = g.chests[i];
    if (g.p.silver < c.cost) {
        showToast(g, TextFormat("Need %d silver", c.cost), GRAY);
        return;
    }
    g.p.silver -= c.cost;
    g.chests.erase(g.chests.begin() + i);
    grantItem(g, rollItem());
}

void updatePlayer(Game& g, float dt) {
    Player& p = g.p;
    p.time += dt;
    g.runTime += dt;
    Vector2 in = {float(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - float(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)),
                  float(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) - float(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))};
    if (Vector2Length(in) > 1) in = Vector2Normalize(in);
    p.moving = in.x != 0 || in.y != 0;
    if (p.moving) {
        p.facing = fabsf(in.x) > fabsf(in.y) ? (in.x > 0 ? RIGHT : LEFT) : (in.y > 0 ? DOWN : UP);
        p.anim += dt;
        float speed = p.speed * (1 + 0.1f * p.items[IT_BOOTS]) * (p.speedT > 0 ? 1.5f : 1);
        Vector2 nx = {p.pos.x + in.x * speed * dt, p.pos.y};
        if (g.map.walk(p.pos, nx)) p.pos.x = nx.x;
        Vector2 ny = {p.pos.x, p.pos.y + in.y * speed * dt};
        if (g.map.walk(p.pos, ny)) p.pos.y = ny.y;
    }

    float regen = p.regen + 1.5f * p.items[IT_SPROUT];
    if (regen > 0 && p.hp < p.maxHp) {
        p.regenAcc += regen * dt;
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
    p.speedT = std::max(0.f, p.speedT - dt);

    if (p.iframes > 0) {
        p.iframes -= dt;
        return;
    }
    Enemy* hitter = nullptr;  // the hardest hitter touching you, not whichever the grid lists first
    g.grid.query(p.pos, PLAYER_RADIUS + 120, [&](int i) {
        Enemy& e = g.enemies[i];
        if (!e.dying && Vector2Distance(e.pos, p.pos) <= PLAYER_RADIUS + e.radius() && (!hitter || e.damage > hitter->damage)) hitter = &e;
        return false;
    });
    if (hitter) hurtPlayer(g, hitter->damage, hitter);
}

void updateSpawner(Game& g, float dt) {
    Player& p = g.p;
    if (g.floor == MAX_FLOORS) {
        // The last island holds only the final boss.
        if (g.finalBossT > 0 && (g.finalBossT -= dt) <= 0) spawnFinalBoss(g);
        if (g.victoryT > 0 && (g.victoryT -= dt) <= 0) {
            g.p.gold += 1000;
            g.mode = Mode::Victory;
            bankRun(g);
            stopMusic();
            A.win.play(1, 0);
        }
        return;
    }
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
        startEndTimes(g);
        return;
    }
    if ((g.spawnT -= dt) <= 0) {
        g.spawnT = g.spawnWait;
        for (int i = 0; i < g.spawnCount; i++)
            if (!spawnEnemy(g, -1, 0)) break;
    }
    if ((g.difficultyT -= dt) <= 0) {
        g.difficultyT = g.spawnWait > 0.5f ? 10 : 20;
        if (g.spawnWait > 0.5f) g.spawnWait -= 0.05f;
        else g.spawnCount++;
    }
}

float weaponWait(const Player& p, const Weapon& w) {
    return std::max(0.05f, w.stats().wait * p.fireRateMul / w.fireRate / (1 + 0.12f * p.items[IT_QUILL]));
}

int orbCount(const Player& p, const Weapon& w) { return w.stats().projectiles + w.projectile + p.items[IT_CROWN]; }
float orbRadius(const Weapon& w) { return 8 * w.size; }
Vector2 orbPos(const Player& p, const Weapon& w, int k) {
    float ring = 40 * w.stats().scale * w.size * p.aoe;
    return Vector2Add(p.pos, Vector2Rotate({ring, 0}, w.spin + k * 2 * PI / orbCount(p, w)));
}

void fireWeapon(Game& g, Weapon& w, float dt) {
    Player& p = g.p;
    const WeaponLevel& lv = w.stats();
    w.pulse = std::max(0.f, w.pulse - dt);
    int damage = int(roundf(lv.damage * p.dmgMul * w.damage));

    if (w.id == ORBIT) {
        // Orbs hit whatever they touch; each enemy then has a cooldown.
        w.spin += lv.speed * dt;
        float r = orbRadius(w);
        for (int k = 0; k < orbCount(p, w); k++) {
            Vector2 at = orbPos(p, w, k);
            g.grid.query(at, r + 120, [&](int i) {
                Enemy& e = g.enemies[i];
                if (e.dying || e.orbitT > 0 || Vector2Distance(e.pos, at) > r + e.radius()) return false;
                e.orbitT = weaponWait(p, w);
                onHit(g, e, damage, p.imbueFire, p.imbueFrost);
                return false;
            });
        }
        return;
    }

    if ((w.timer += dt) < weaponWait(p, w)) return;
    w.timer = 0;

    if (w.id == POISON_AURA) {
        w.pulse = 0.3f;
        float radius = 48 * lv.scale * p.aoe * w.size;
        g.grid.query(p.pos, radius + 120, [&](int i) {
            Enemy& e = g.enemies[i];
            if (e.dying || Vector2Distance(e.pos, p.pos) > radius + e.radius()) return false;
            onHit(g, e, damage, p.imbueFire, p.imbueFrost);
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
    int count = lv.projectiles + w.projectile + p.items[IT_CROWN];
    A.wandShot.play(1, -10, 0.05);
    for (int i = 0; i < count; i++) {
        bool crit = rnd() <= 0.05f + p.crit + 0.08f * p.items[IT_CLOVER];
        Vector2 d = Vector2Rotate(dir, (i - (count - 1) / 2.f) * 15 * DEG2RAD);
        g.shots.push_back({p.pos, d, lv.speed, crit ? 1.5f * w.size : w.size, 3, crit ? damage * 2 : damage,
                           w.pierce, w.ricochet, p.imbueFire, p.imbueFrost, crit, {}});
    }
}

void explode(Game& g, const Shot& s) {
    float radius = 35 * s.size;
    g.grid.query(s.pos, radius + 120, [&](int i) {
        Enemy& e = g.enemies[i];
        if (e.dying || Vector2Distance(e.pos, s.pos) > radius + e.radius()) return false;
        bool wasFull = e.hp >= e.maxHp;
        onHit(g, e, s.damage, s.fire, s.frost);
        if (wasFull && e.hp <= 0) g.p.crit += 0.001f;  // one-shot kills sharpen your crits
        return false;
    });
}

void updateShots(Game& g, float dt) {
    for (Shot& s : g.shots) {
        s.pos = Vector2Add(s.pos, Vector2Scale(s.dir, s.speed * dt));
        s.life -= dt;
        if (s.life <= 0 || Vector2Distance(s.pos, g.p.pos) > 900) continue;
        int hit = -1;
        g.grid.query(s.pos, 6 * s.size + 120, [&](int i) {
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
        s.pos = Vector2Add(s.pos, Vector2Scale(s.dir, ENEMY_SHOT_SPEED * dt));
        s.life -= dt;
        if (Vector2Distance(s.pos, g.p.pos) < PLAYER_RADIUS + 6 * s.scale) {
            hurtPlayer(g, s.damage, nullptr);
            s.life = 0;
        }
    }
    std::erase_if(g.enemyShots, [](const EnemyShot& s) { return s.life <= 0; });

    for (Blast& b : g.blasts) {
        if (b.t == 0)
            g.grid.query(b.pos, b.radius + 120, [&](int i) {
                Enemy& e = g.enemies[i];
                if (Vector2Distance(e.pos, b.pos) <= b.radius + e.radius()) hurtEnemy(g, e, b.damage);
                return false;
            });
        b.t += dt;
    }
    std::erase_if(g.blasts, [](const Blast& b) { return b.t > 0.25f; });
    for (Bolt& b : g.bolts) b.t += dt;
    std::erase_if(g.bolts, [](const Bolt& b) { return b.t > 0.15f; });
}

void onEnemyRemoved(Game& g, const Enemy& e) {
    addKill(g);
    if (int n = g.p.items[IT_SPORES]) g.blasts.push_back({e.pos, 30.f + 12 * n, std::max(1, e.lastHit), 0});
    if (!e.boss()) {
        dropSeed(g, e.pos, e.exp);
        return;
    }
    float spread = e.kind == GUARDIAN ? 80 : 100;
    for (int i = 0; i < 50; i++)
        g.seeds.push_back({Vector2Add(e.pos, {rndr(-spread, spread), rndr(-spread, spread)}), SEED_EXP,
                           e.kind == GUARDIAN ? 1 : e.exp, PICKUP_DESPAWN, 0, rndr(0.4f, 0.7f)});
    if (e.kind == FINAL_BOSS) {
        g.victoryT = 4;
        return;
    }
    if (e.portal >= 0) {
        g.portals[e.portal].state = Portal::PURIFIED;
        g.chests.push_back({Vector2Add(g.portals[e.portal].pos, {0, 28}), 0});  // the guardian's reward
    }
    g.bossDefeated = true;
    if (!g.endTimes) startEndTimes(g);
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
        e.orbitT -= dt;

        Vector2 to = Vector2Subtract(p.pos, e.pos);
        float dist = Vector2Length(to);
        if (dist > DESPAWN_RADIUS && !e.boss()) { e.gone = true; continue; }
        Vector2 dir = dist > 0 ? Vector2Scale(to, 1 / dist) : Vector2{};
        float speed = e.speed * e.slowMul;

        if (e.boss()) {
            updateBoss(g, e, dir, dist, dt);
        } else if (e.ratman) {
            // Ratmen shrug off hits and, as boss minions, shoot on the move.
            e.hurtT -= dt;
            e.vel = Vector2Scale(dir, speed);
            if (e.shooter && dist <= 400 && (e.shootT -= dt) <= 0) {
                e.shootT = rndr(2, 3.5f);
                bossShot(g, e, dir, e.damage, 0.8f, rgb(0.1f, 0.1f, 0.1f));
            }
        } else if (e.dashT > 0) {
            e.dashT -= dt;
            e.vel = Vector2Scale(e.dashDir, e.speed * 5);
        } else if (e.castT > 0) {
            // Dash wind-up (drawn grey): stand still, then lunge at where the player was.
            e.vel = {};
            if ((e.castT -= dt) <= 0) e.dashT = 0.35f, e.dashDir = dir;
        } else if (e.hurtT > 0) {
            e.hurtT -= dt;
            e.vel = Vector2Scale(e.vel, powf(0.85f, dt * 60));  // knockback skid
        } else {
            Vector2 want = Vector2Scale(dir, speed);
            if (dist <= ACTIVE_RADIUS) {
                if (e.shooter && dist <= 350) {
                    // Creep in while firing so melee builds can reach them; back off (slower than you) if rushed.
                    want = Vector2Scale(dir, dist < 140 ? -speed * 0.4f : dist > 200 ? speed * 0.35f : 0);
                    if ((e.shootT -= dt) <= 0) {
                        e.shootT = rndr(3.5f, 6);
                        bossShot(g, e, dir, e.damage, 1, rgb(1, 0.3f, 0.2f));
                    }
                } else {
                    if (e.type == DASHER && (e.shootT -= dt) <= 0 && dist < 220) e.shootT = rndr(2.5f, 4), e.castT = 0.5f;
                    want = Vector2Scale(Vector2Rotate(dir, e.flank * std::clamp((dist - 60) / 300, 0.f, 1.f)), speed);
                }
                // Separation, stronger the deeper the overlap.
                Vector2 push{};
                int n = 0;
                g.grid.query(e.pos, e.radius() + 120, [&](int j) {
                    if (j == int(i)) return false;
                    const Enemy& o = g.enemies[j];
                    Vector2 away = Vector2Subtract(e.pos, o.pos);
                    float d = Vector2Length(away), reach = e.radius() + o.radius();
                    if (d >= reach) return false;
                    Vector2 u = d > 0 ? Vector2Scale(away, 1 / d) : Vector2{rndr(-1, 1), rndr(-1, 1)};
                    push = Vector2Add(push, Vector2Scale(u, 1 - d / reach));
                    return ++n >= 6;
                });
                want = Vector2ClampValue(Vector2Add(want, Vector2Scale(push, speed)), 0, speed);
            }
            e.vel = Vector2Lerp(e.vel, want, std::min(1.f, dt * 8));  // steer, don't snap
        }

        Vector2 step = Vector2Scale(e.vel, dt);
        if (dist > ACTIVE_RADIUS || e.boss()) {
            e.pos = Vector2Add(e.pos, step);
        } else {
            if (g.map.walk(e.pos, {e.pos.x + step.x, e.pos.y})) e.pos.x += step.x;
            if (g.map.walk(e.pos, {e.pos.x, e.pos.y + step.y})) e.pos.y += step.y;
        }
        if (Vector2LengthSqr(e.vel) > 0 && (e.hurtT <= 0 || e.ratman)) {
            e.facing = fabsf(e.vel.x) > fabsf(e.vel.y) ? (e.vel.x > 0 ? RIGHT : LEFT) : (e.vel.y > 0 ? DOWN : UP);
            e.anim += dt;
        }
    }

    for (size_t i = g.enemies.size(); i-- > 0;) {
        Enemy& e = g.enemies[i];
        if (e.dying && e.deathT <= 0) onEnemyRemoved(g, e);
        else if (!e.gone) continue;
        e = g.enemies.back();
        g.enemies.pop_back();
    }
    g.enemies.insert(g.enemies.end(), g.pending.begin(), g.pending.end());
    g.pending.clear();
}

void collectSeed(Game& g, const Seed& s) {
    Player& p = g.p;
    switch (s.type) {
        case SEED_EXP: gainExp(g, s.amount); break;
        case SEED_MAGNET: A.orb.play(1.3f, -5); p.magnetT = 5, p.magnetScan = 0; break;
        case SEED_SPEED:
            A.orb.play(1.3f, -5);
            p.speedT = 5;
            break;
        case SEED_BOMB:
            A.orb.play(1.3f, -5);
            g.grid.query(s.pos, 600, [&](int i) {
                if (Vector2Distance(g.enemies[i].pos, s.pos) <= 600) hurtEnemy(g, g.enemies[i], 150);
                return false;
            });
            break;
        case SEED_GOLD: p.gold += s.amount * p.coinMul; A.orb.play(2, -5, 0.04); break;
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

// Pixelify Sans rasterised at its on-screen pixel size, so text stays crisp at any scale like Godot's.
// ponytail: one font per pixel size, never freed; fine for the handful of sizes the UI uses.
// Sizes are em sizes as in Godot; raylib sizes fonts by line height, 1.2 em for this font.
constexpr float EM = 1.2f;
float textScale = 1;  // screen pixels per unit of the current transform
const Font& font(float size) {
    static std::map<int, Font> cache;
    auto [it, fresh] = cache.try_emplace(std::max(6, int(roundf(size * textScale))));
    if (fresh) {
        it->second = LoadFontEx(asset("ui/fonts/PixelifySans-VariableFont_wght.ttf"), it->first, nullptr, 0);
        if (!IsFontValid(it->second)) it->second = GetFontDefault();
    }
    return it->second;
}
Vector2 measure(const char* s, float size) { return MeasureTextEx(font(size * EM), s, size * EM, 0); }

void text(const char* s, float x, float y, float size, Color c, bool center = false, float outline = 0, Color oc = BLACK) {
    if (center) x -= measure(s, size).x / 2;
    size *= EM;
    for (int k = 0; outline > 0 && k < 8; k++) DrawTextEx(font(size), s, {x + cosf(k * PI / 4) * outline / 2, y + sinf(k * PI / 4) * outline / 2}, size, 0, oc);
    DrawTextEx(font(size), s, {x, y}, size, 0, c);
}

Color enemyTint(const Enemy& e, float t) {
    if (e.castT > 0 || e.transformT > 0) return rgb(0.3f, 0.3f, 0.3f);
    if (e.hurtT > 0 && e.boss()) return e.kind == GUARDIAN ? WHITE : ColorBrightness(e.color, -0.35f);
    if (e.enraged) return ColorLerp(e.color, e.glow, (sinf(t * PI) + 1) / 2);
    if (e.burnTicks > 0) return rgb(1, .4f, .1f);
    if (e.slowT > 0) return rgb(.3f, .8f, 1);
    return e.color;
}

void drawEnemy(const Enemy& e, float t) {
    Color tint = enemyTint(e, t);
    int walk = int(e.anim * 5) % 4;
    if (e.ratman) {
        if (e.hurtT > 0 && !e.boss()) tint = ColorBrightness(tint, -0.35f);
        if (e.dying) drawFrame(A.ratman, 3 - std::min(3, int((0.8f - e.deathT) * 5)), 5, e.pos, e.scale, tint);
        else drawFrame(A.ratman, walk, e.facing, e.pos, e.scale, tint);
    } else if (e.kind == GUARDIAN) {
        int f = e.dying ? (e.deathT < 0.2f) : e.hurtT > 0 ? (e.hurtT < 0.075f) : walk;
        int col = e.dying ? 6 + f : e.hurtT > 0 ? (e.facing == RIGHT ? 3 - f : 4 + f) : (e.facing == RIGHT ? 7 - f : f);
        drawFrame(A.guardian, col, e.dying ? 0 : e.facing, e.pos, e.scale, tint);
    } else if (Texture2D tex = A.enemySprites[e.type]; tex.id) {
        if (e.hurtT > 0) tint = ColorBrightness(tint, -0.35f);
        if (e.dying) drawFrame(tex, 0, e.facing, e.pos, e.scale * (0.6f + e.deathT), Fade(tint, e.deathT / 0.4f));  // shrink and fade
        else drawFrame(tex, walk, e.facing, e.pos, e.scale, tint);
    } else if (e.dying) {
        drawFrame(A.slime, 12 + (e.deathT < 0.2f), 0, e.pos, e.scale, tint);
    } else {
        drawFrame(A.slime, e.hurtT > 0 ? 10 + (e.hurtT < 0.125f) : 6 + walk, e.facing, e.pos, e.scale, tint);
    }
    if (!e.dying && !e.boss() && e.hp < e.maxHp) {
        float w = 16 * e.scale, y = e.pos.y + 10 * e.scale;
        DrawRectangleV({e.pos.x - w / 2, y}, {w, 2}, Fade(BLACK, 0.6f));
        DrawRectangleV({e.pos.x - w / 2, y}, {w * e.hp / e.maxHp, 2}, RED);
    }
}

// Call inside BeginMode2D: water everywhere on screen, then the grass and soil autotiles.
void drawTerrain(const Map& map, Camera2D cam) {
    Vector2 lo = GetScreenToWorld2D({0, 0}, cam), hi = GetScreenToWorld2D({float(GetScreenWidth()), float(GetScreenHeight())}, cam);
    int x0 = int(floorf(lo.x / TILE)) - 1, y0 = int(floorf(lo.y / TILE)) - 1, x1 = int(ceilf(hi.x / TILE)), y1 = int(ceilf(hi.y / TILE));
    float frame = float(int(GetTime() * 2) % 4) * TILE;  // 4 frames at 2 fps, as in the Godot tileset
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) DrawTextureRec(A.water, {frame, 0, TILE, TILE}, {float(x * TILE), float(y * TILE)}, map.water);
    auto layer = [&](Texture2D tex, const std::vector<int8_t>& tiles, Color tint) {
        for (int y = std::max(y0, -MAP_N / 2); y <= std::min(y1, MAP_N / 2 - 1); y++)
            for (int x = std::max(x0, -MAP_N / 2); x <= std::min(x1, MAP_N / 2 - 1); x++) {
                int8_t t = tiles[(y + MAP_N / 2) * MAP_N + x + MAP_N / 2];
                if (t >= 0) DrawTextureRec(tex, {float(t % 11 * TILE), float(t / 11 * TILE), TILE, TILE}, {float(x * TILE), float(y * TILE)}, tint);
            }
    };
    layer(A.grass, map.grass, map.grassTint);
    layer(A.soil, map.soil, map.soilTint);
}

void drawWorld(const Game& g, Camera2D cam) {
    const Player& p = g.p;
    float t = float(GetTime());
    BeginMode2D(cam);
    textScale = cam.zoom;
    drawTerrain(g.map, cam);

    for (const Portal& pt : g.portals) {
        int col = pt.state == Portal::PURIFIED ? 3 + int(t * 5) % 2 : int(t * 5) % 3;
        drawFrame(A.portal, col, 0, pt.pos, 1.5f, WHITE);
    }

    for (const Chest& c : g.chests) {
        if (!c.cost) DrawCircleV(c.pos, 14 + 2 * sinf(t * 4), Fade(rgb(1, .8f, .2f), 0.18f));  // the guardian's reward glows
        drawFrame(A.chest, 0, 0, c.pos, 1, WHITE);
    }

    static const Color SEED_TINT[] = {WHITE, rgb(1, .2f, .2f), rgb(.2f, .5f, 1), rgb(.1f, .1f, .1f), rgb(1, .8f, .1f), rgb(.8f, .8f, .85f), rgb(.7f, .25f, 1)};
    static const float SEED_SCALE[] = {1.2f, 1.8f, 1.6f, 1.7f, 1.5f, 1.4f, 1.6f};
    for (const Seed& s : g.seeds) {
        float alpha = s.life < PICKUP_WARNING && !s.magnetic ? 0.6f + 0.4f * sinf(t * 20) : 0.75f + 0.25f * sinf(t * PI / s.glimmer);
        float size = 10 * SEED_SCALE[s.type];
        DrawTexturePro(A.seed, {0, 0, 48, 48}, {s.pos.x, s.pos.y, size, size}, {size / 2, size / 2}, 0, Fade(SEED_TINT[s.type], alpha));
    }

    for (const Weapon& w : p.weapons)
        if (w.id == POISON_AURA) {
            float r = 48 * w.stats().scale * p.aoe * w.size;
            DrawTexturePro(A.aura, {0, 0, 48, 48}, {p.pos.x, p.pos.y, r * 2, r * 2}, {r, r}, 0, Fade(WHITE, 0.35f + w.pulse));
        }

    // Trees, enemies and the player sorted by their feet, like Godot's y-sort.
    // A tree the player stands behind turns see-through.
    Vector2 lo = GetScreenToWorld2D({0, 0}, cam), hi = GetScreenToWorld2D({float(GetScreenWidth()), float(GetScreenHeight())}, cam);
    std::vector<std::pair<float, int>> order;  // (feet y, index): enemies >= 0, player -1, trees <= -2
    for (int i = 0; i < int(g.enemies.size()); i++) order.push_back({g.enemies[i].pos.y + 8 * g.enemies[i].scale, i});
    order.push_back({p.pos.y + 9, -1});
    for (int i = 0; i < int(g.map.trees.size()); i++) {
        Vector2 tp = g.map.trees[i];
        if (tp.x > lo.x - 32 && tp.x < hi.x + 32 && tp.y > lo.y - 8 && tp.y < hi.y + 56) order.push_back({tp.y, -2 - i});
    }
    std::sort(order.begin(), order.end());
    for (auto [y, i] : order) {
        if (i >= 0) drawEnemy(g.enemies[i], t);
        else if (i == -1) {
            Color tint = p.speedT > 0 ? rgb(.5f, .8f, 1) : WHITE;
            if (p.iframes > 0) tint = Fade(tint, int(p.iframes * 10) % 2 ? 0.3f : 1.f);
            drawFrame(A.characters[p.character], p.moving ? int(p.anim * 5) % 4 : 0, p.facing, p.pos, 1, tint);
        } else {
            Vector2 c = Vector2Add(g.map.trees[-2 - i], {0, -22});
            float size = 32 * 1.51f;  // world/tree.tscn scale
            Color tint = Fade(g.map.grassTint, Vector2Distance(p.pos, c) < 26 ? 0.4f : 1.f);
            DrawTexturePro(A.tree, {0, 0, 32, 32}, {c.x, c.y, size, size}, {size / 2, size / 2}, 0, tint);
        }
    }

    for (const Shot& s : g.shots) {
        float size = 16 * s.size;
        DrawTexturePro(A.projectile, {0, 0, 16, 16}, {s.pos.x, s.pos.y, size, size}, {size / 2, size / 2},
                       atan2f(s.dir.y, s.dir.x) * RAD2DEG, s.crit ? rgb(1, .8f, .1f) : WHITE);
    }
    for (const Weapon& w : p.weapons)
        if (w.id == ORBIT)
            for (int k = 0; k < orbCount(p, w); k++) {
                Vector2 at = orbPos(p, w, k);
                float size = 2.5f * orbRadius(w);
                DrawTexturePro(A.thorn, {0, 0, 16, 16}, {at.x, at.y, size, size}, {size / 2, size / 2}, w.spin * RAD2DEG * 3, WHITE);
            }
    for (const EnemyShot& s : g.enemyShots) {
        DrawCircleV(s.pos, 7 * s.scale, Fade(s.color, 0.25f));
        DrawCircleV(s.pos, 4 * s.scale, s.color);
        DrawCircleV(s.pos, 1.8f * s.scale, Fade(WHITE, 0.7f));
    }
    for (const Blast& b : g.blasts) DrawCircleV(b.pos, b.radius * (0.5f + b.t * 2), Fade(rgb(1, .6f, .2f), 0.5f * (1 - b.t / 0.25f)));
    for (const Bolt& b : g.bolts) DrawLineEx(b.a, b.b, 1.5f, Fade(rgb(.6f, .85f, 1), 1 - b.t / 0.15f));
    for (const DamageNumber& n : g.numbers) {
        const char* txt = TextFormat("%d", n.value);
        Vector2 at = {n.pos.x - 4, n.pos.y - 14 - n.t * 30};
        DrawTextEx(font(8), txt, {at.x + 0.5f, at.y + 0.5f}, 8, 0, Fade(BLACK, 1 - n.t / 0.6f));
        DrawTextEx(font(8), txt, at, 8, 0, Fade(WHITE, 1 - n.t / 0.6f));
    }

    auto prompt = [&](const char* label, Vector2 at) {
        Vector2 size = MeasureTextEx(font(9), label, 9, 0.5f);
        DrawRectangleRounded({at.x - size.x / 2 - 5, at.y - size.y / 2 - 1, size.x + 10, size.y + 2}, 1, 8, Fade(BLACK, 0.6f));
        DrawTextEx(font(9), label, {at.x - size.x / 2, at.y - size.y / 2}, 9, 0.5f, WHITE);
    };
    int chest = nearChest(g), near = nearPortal(g);
    if (g.mode == Mode::Play && chest >= 0) {
        const Chest& c = g.chests[chest];
        prompt(c.cost ? TextFormat("[E] Open chest - %d silver", c.cost) : "[E] Open chest", {c.pos.x, c.pos.y - 18});
    } else if (g.mode == Mode::Play && near >= 0) {
        const Portal& pt = g.portals[near];
        prompt(pt.state == Portal::PURIFIED ? "[E] Enter portal" : "[E] Summon guardian", {pt.pos.x, pt.pos.y - 40});
    }
    EndMode2D();
}

// ---------------------------------------------------------------- ui

// The menus and HUD follow the Godot scenes (ui/*.tscn): laid out on its 1280x720
// canvas and scaled to the window like its canvas_items stretch (see main).
struct Ui {
    int hot = -1, lastHot = -1, drag = -1;
    std::string hint;
    float w = 1280, h = 720;  // canvas size in UI units
    float grow[64]{};         // per-button hover scale
    bool options = false;
    int lastExp = -1;
    float expShowT = 0, turnT = 0;
    Vector2 scroll{}, dir{1, 0}, target{1, 0};
} ui;

float roundness(Rectangle r, float radius) { return std::min(1.f, 2 * radius / std::min(r.width, r.height)); }
Color gray(float v, float a) { return {uint8_t(v * 255), uint8_t(v * 255), uint8_t(v * 255), uint8_t(a * 255)}; }

std::vector<std::string> lines(const std::string& s) {
    std::vector<std::string> out(1);
    for (char c : s)
        if (c == '\n') out.emplace_back();
        else out.back().push_back(c);
    return out;
}

float textWidth(const std::string& s, float size) {
    float w = 0;
    for (const std::string& l : lines(s)) w = std::max(w, measure(l.c_str(), size).x);
    return w;
}

// Each line centred in r, the block centred vertically, like a Godot Label or Button.
void textIn(const std::string& s, Rectangle r, float size, Color c, float outline = 0, Color oc = BLACK) {
    std::vector<std::string> ls = lines(s);
    float lh = size * EM, y = r.y + (r.height - ls.size() * lh) / 2;
    for (const std::string& l : ls) text(l.c_str(), r.x + r.width / 2, y, size, c, true, outline, oc), y += lh;
}

// Hover grows a button 10% and a click squashes it, like the tweens in gui.gd.
Rectangle grown(int id, Rectangle r, bool hover) {
    float& g = ui.grow[id];
    if (g == 0) g = 1;
    g = Lerp(g, hover ? 1.1f : 1.f, std::min(1.f, GetFrameTime() * 20));
    return {r.x + r.width * (1 - g) / 2, r.y + r.height * (1 - g) / 2, r.width * g, r.height * g};
}

bool clicked(int id, bool hover, bool enabled) {
    if (hover) ui.hot = id;
    bool c = enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (c) A.click.play(1, -6), ui.grow[id] = 0.9f;
    return c;
}

// A TextureButton on ui/menu_buttons.png with a white label.
bool imageButton(int id, Rectangle r, const char* label, float size) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), r), click = clicked(id, hover, true);
    Rectangle d = grown(id, r, hover);
    DrawTexturePro(A.menuButton, {0, 0, 128, 48}, d, {}, 0, WHITE);
    textIn(label, d, size * d.width / r.width, WHITE);
    return click;
}

// Godot's default theme Button; `tint` is the node's modulate.
bool button(int id, Rectangle r, const std::string& label, bool enabled = true, Color fg = gray(.875f, 1), Color tint = WHITE) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), r), click = clicked(id, hover, enabled);
    Rectangle d = grown(id, r, hover && enabled);
    float v = hover && enabled ? (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 0 : 0.225f) : 0.1f;
    DrawRectangleRounded(d, roundness(d, 3), 4, ColorTint(gray(v, enabled ? 0.6f : 0.3f), tint));
    textIn(label, d, 16 * d.width / r.width, ColorTint(enabled ? fg : ColorAlpha(fg, 0.5f), tint));
    return click;
}

// Godot's default ProgressBar under a modulate colour.
void meter(Rectangle r, float frac, Color mod, const char* label, Color labelColor) {
    frac = std::clamp(frac, 0.f, 1.f);
    if (frac < 1) DrawRectangleRounded(r, roundness(r, 3), 4, ColorAlpha(ColorTint(gray(.1f, 1), mod), 0.3f * (1 - frac * 0.8f)));
    if (frac > 0) {
        Rectangle f = {r.x, r.y, std::max(6.f, r.width * frac), r.height};
        DrawRectangleRounded(f, roundness(f, 3), 4, ColorAlpha(ColorTint(gray(.75f, 1), mod), 0.6f));
    }
    textIn(label, r, 16, labelColor);
}

// The drifting lily-of-the-valley wallpaper of the Godot menus: 40 px/s, a new heading every 4 s.
void wallpaper() {
    float dt = GetFrameTime();
    if ((ui.turnT -= dt) <= 0) ui.turnT = 4, ui.target = Vector2Normalize({rndr(-1, 1), rndr(-1, 1)});
    ui.dir = Vector2Lerp(ui.dir, ui.target, std::min(1.f, dt * 0.5f));
    ui.scroll = Vector2Add(ui.scroll, Vector2Scale(ui.dir, 40 * dt));
    float ox = fmodf(ui.scroll.x, 288), oy = fmodf(ui.scroll.y, 288);
    for (float y = oy - 288 * (oy > 0); y < ui.h; y += 288)
        for (float x = ox - 288 * (1 + (ox > 0)); x < ui.w; x += 288) DrawTexture(A.titleBg, int(x), int(y), WHITE);
}

// The title art bobbing over its half-transparent shadow.
void logo(Vector2 c, float scale, float bob) {
    float y = c.y - bob * (0.5f - 0.5f * cosf(float(GetTime()) * PI / 1.5f)), w = 256 * scale, h = 48 * scale;
    DrawTexturePro(A.title, {0, 0, 256, 48}, {c.x - w / 2 + scale, y - h / 2 + 2 * scale, w, h}, {}, 0, Fade(BLACK, 0.5f));
    DrawTexturePro(A.title, {0, 0, 256, 48}, {c.x - w / 2, y - h / 2, w, h}, {}, 0, WHITE);
}

void applyVolume() {
    SetMasterVolume(profile.volume[0]);
    musicVolume = profile.volume[1], sfxVolume = profile.volume[2];
    if (IsMusicValid(A.music)) SetMusicVolume(A.music, db(A.musicDb) * musicVolume);
}

void toTitle(Game& g) {
    g.mode = Mode::Title;
    playMusic("ui/music.mp3", -8);
}

void openShop(Game& g) {
    g.mode = Mode::Shop;
    ui.hint = "Prepare for your journey.";
    playMusic("ui/shopping.wav", -8);
}

// ui/options.tscn: three volume sliders and BACK.
void optionsScreen() {
    float cx = ui.w / 2, cy = ui.h / 2, k = 1.2885f, x = cx - 376, w = 584 * k, y = cy - 168;
    wallpaper();
    const char* names[] = {"Master", "Music", "Sfx"};
    Vector2 m = GetMousePosition();
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ui.drag = -1;
    for (int i = 0; i < 3; i++) {
        text(names[i], x, y, 42 * k, WHITE);
        y += 42 * k * EM + 4 * k;
        Rectangle track = {x, y + 6 * k, w, 4 * k};
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, {x - 8, y, w + 16, 16 * k})) ui.drag = i;
        if (ui.drag == i) profile.volume[i] = std::clamp((m.x - x) / w, 0.f, 1.f), applyVolume();
        float v = profile.volume[i];
        DrawRectangleRounded(track, 1, 4, gray(0, 0.4f));
        DrawRectangleRounded({x, track.y, w * v, track.height}, 1, 4, gray(1, 0.75f));
        DrawCircleV({x + w * v, track.y + track.height / 2}, 8 * k, WHITE);
        y += 16 * k + 4 * k;
    }
    if (imageButton(40, {cx - 103.8f, ui.h - 181.87f, 128 * 1.6408f, 48 * 1.6408f}, "BACK", 34.5f) || IsKeyPressed(KEY_ESCAPE)) {
        ui.options = false;
        save();
    }
}

void titleScreen(Game& g) {
    float cx = ui.w / 2, cy = ui.h / 2;
    wallpaper();
    logo({cx, cy - 112}, 4.785f, 18.3f);
    if (imageButton(30, {cx - 104.7f, cy + 28, 234, 87.8f}, "START", 38.5f) || IsKeyPressed(KEY_ENTER)) openShop(g);
    if (imageButton(31, {cx - 104.7f, cy + 123.1f, 234, 87.8f}, "OPTIONS", 27.6f)) ui.options = true;
    if (imageButton(32, {cx - 104.7f, cy + 218.2f, 234, 87.8f}, "QUIT", 30.3f)) g.quit = true;
}

// The PREPARATION panel of ui/gui.tscn.
void shopScreen(Game& g) {
    float cx = ui.w / 2;
    wallpaper();
    DrawRectangle(0, 0, int(ui.w) + 1, int(ui.h) + 1, gray(.1f, .6f));
    textIn("PREPARATION", {0, 0, ui.w, 20}, 16, WHITE);
    int refund = respecRefund(profile);
    if (imageButton(20, {cx - 62, 26, 124, 46}, "RESPEC", 20) && refund > 0) {
        respec(profile);
        save();
        ui.hint = TextFormat("Upgrades reset! Refunded %d coins.", refund);
    }
    textIn(TextFormat("Total Coins: %d", profile.coins), {0, 76, ui.w, 20}, 16, WHITE);
    textIn("Upgrades", {cx - 234, 100, 263, 20}, 16, WHITE);
    textIn("Weapons", {cx + 33, 100, 200, 20}, 16, WHITE);
    std::string hover;
    for (int i = 0; i < PERM_COUNT; i++) {
        const PermUpgrade& u = PERM_UPGRADES[i];
        int cost = upgradeCost(profile, i);
        Rectangle r = {cx - 234, 124 + i * 54.f, 263, 50};
        std::string label = cost < 0 ? TextFormat("%s (MAX)", u.name) : TextFormat("%s (Lv %d) : %d Coins", u.name, profile.levels[i], cost);
        if (button(i, r, label, cost >= 0 && profile.coins >= cost) && buyUpgrade(profile, i)) {
            save();
            ui.hint = "Upgrade Purchased!";
        }
        if (CheckCollisionPointRec(GetMousePosition(), r)) {
            bool pct = i != PERM_MAX_HP && i != PERM_SPEED && i != PERM_REGEN;
            float scale = pct ? 100.f : 1.f;
            std::string now = fmtNum(permBoost(profile, PermId(i)) * scale), next = fmtNum((profile.levels[i] + 1) * u.boost * scale);
            hover = cost < 0 ? TextFormat("Maximum level reached for %s.", u.name)
                             : TextFormat("Increases %s. Current: +%s%s -> Next: +%s%s", u.name, now.c_str(), pct ? "%" : "", next.c_str(), pct ? "%" : "");
        }
    }
    for (int w = 0; w < WEAPON_COUNT; w++) {
        Rectangle r = {cx + 33, 124 + w * 54.f, 200, 50};
        bool selected = profile.weapon == w;
        if (button(10 + w, r, selected ? TextFormat("[ %s ]", WEAPON_NAMES[w]) : WEAPON_NAMES[w], true, gray(.875f, 1), selected ? rgb(.5f, 1, .5f) : WHITE)) {
            profile.weapon = WeaponId(w);
            save();
        }
        if (CheckCollisionPointRec(GetMousePosition(), r)) hover = TextFormat("Start your run equipped with the %s.", WEAPON_NAMES[w]);
    }
    textIn("Character", {cx + 33, 124 + float(WEAPON_COUNT) * 54, 200, 20}, 16, WHITE);
    for (int c = 0; c < CHARACTER_COUNT; c++) {
        Rectangle r = {cx + 33 + c * 40.f, 148 + float(WEAPON_COUNT) * 54, 36, 46};
        if (button(30 + c, r, "")) profile.character = c, save();
        if (profile.character == c) DrawRectangleLinesEx(r, 2, rgb(.5f, 1, .5f));
        DrawTexturePro(A.characters[c], {12, 12, 24, 24}, {r.x, r.y + 5, 36, 36}, {}, 0, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), r)) hover = TextFormat("Play as %s.", CHARACTERS[c].name);
    }
    float y = 124 + int(PERM_COUNT) * 54.f;
    const char* details = hover.empty() ? ui.hint.c_str() : hover.c_str();
    float tw = measure(details, 16).x;
    DrawRectangleRec({cx - tw / 2 - 4, y + 2, tw + 8, 20}, gray(.1f, .6f));
    textIn(details, {0, y + 2, ui.w, 20}, 16, WHITE);
    if (imageButton(21, {cx - 128, y + 26, 124, 46}, "START", 20) || IsKeyPressed(KEY_ENTER)) newRun(g);
    if (imageButton(22, {cx + 4, y + 26, 124, 46}, "QUIT", 16) || IsKeyPressed(KEY_ESCAPE)) toTitle(g);
}

// The PLAYER STATS / WEAPON BUFFS panel shown while paused or levelling up (hud.gd).
void statsPanel(const Player& p) {
    struct Row { std::string name, value; Color color; bool header; };
    std::vector<Row> rows;
    auto pct = [](float v) { return std::string(TextFormat("%.1f%%", v * 100)); };
    rows.push_back({"PLAYER STATS", "", YELLOW, true});
    rows.push_back({"Max HP:", TextFormat("%d", int(p.maxHp)), rgb(.56f, .93f, .56f), false});
    rows.push_back({"Speed:", TextFormat("%d", int(p.speed)), rgb(0, 1, 1), false});
    rows.push_back({"Damage:", pct(p.dmgMul), rgb(1, .39f, .28f), false});
    rows.push_back({"Area Size:", pct(p.aoe), rgb(.56f, .93f, .56f), false});
    rows.push_back({"Cooldown:", pct(p.fireRateMul), rgb(0, 1, 1), false});
    if (p.crit > 0) rows.push_back({"Crit Chance:", pct(p.crit), rgb(1, 0, 1), false});
    if (p.regen > 0) rows.push_back({"HP Regen:", TextFormat("%.1f/s", p.regen), rgb(1, .75f, .8f), false});
    if (p.evasion > 0) rows.push_back({"Evasion:", pct(p.evasion), rgb(.68f, .85f, .9f), false});
    if (p.thorns > 0) rows.push_back({"Thorns:", pct(p.thorns), rgb(1, .65f, 0), false});
    if (p.vampirism > 0) rows.push_back({"Vampirism:", pct(p.vampirism), RED, false});
    if (p.coinMul > 1) rows.push_back({"Greed Bonus:", "+" + pct(p.coinMul - 1), rgb(1, .84f, 0), false});
    rows.push_back({"WEAPON BUFFS", "", YELLOW, true});
    for (const Weapon& w : p.weapons) {
        rows.push_back({"Equipped:", TextFormat("%s Lv.%d", WEAPON_NAMES[w.id], w.level), rgb(1, .65f, 0), false});
        if (w.damage > 1) rows.push_back({"Bonus Dmg:", "+" + pct(w.damage - 1), rgb(1, .39f, .28f), false});
        if (w.size > 1) rows.push_back({"Bonus Size:", "+" + pct(w.size - 1), rgb(.56f, .93f, .56f), false});
        if (w.fireRate > 1) rows.push_back({"Bonus Speed:", "+" + pct(w.fireRate - 1), rgb(0, 1, 1), false});
        if (w.pierce > 0) rows.push_back({"Pierce:", TextFormat("+%d", w.pierce), YELLOW, false});
        if (w.ricochet > 0) rows.push_back({"Ricochet:", TextFormat("+%d", w.ricochet), rgb(.68f, .85f, .9f), false});
        if (w.projectile > 0) rows.push_back({"Projectiles:", TextFormat("+%d", w.projectile), YELLOW, false});
    }
    float c1 = measure("PLAYER STATS      ", 16).x, c2 = 0;
    for (const Row& r : rows) c1 = std::max(c1, textWidth(r.name, 16)), c2 = std::max(c2, textWidth(r.value, 16));
    Rectangle box = {ui.w - (c1 + c2 + 44), 99, c1 + c2 + 44, rows.size() * 24.f + 26};
    DrawRectangleRounded(box, roundness(box, 12), 8, gray(.1f, .85f));
    float y = box.y + 15;
    for (const Row& r : rows) {
        text(r.name.c_str(), box.x + 20, y, 16, r.color);
        text(r.value.c_str(), box.x + 24 + c1, y, 16, WHITE);
        y += 24;
    }
}

// ui/hud.tscn.
// Points from the screen edge at a world spot that is off screen (the world is drawn at ZOOM around the player).
void edgeMarker(const Game& g, Vector2 at, Color c) {
    Vector2 d = Vector2Scale(Vector2Subtract(at, g.p.pos), ZOOM), half = {ui.w / 2 - 28, ui.h / 2 - 34};
    if (fabsf(d.x) < half.x && fabsf(d.y) < half.y) return;
    Vector2 m = Vector2Add({ui.w / 2, ui.h / 2}, Vector2Scale(d, std::min(half.x / fabsf(d.x), half.y / fabsf(d.y))));
    float ang = atan2f(d.y, d.x) * RAD2DEG;
    DrawPoly(m, 3, 13, ang, BLACK);
    DrawPoly(m, 3, 10, ang, c);
}

// Name and description beside a hovered HUD icon; only while the mouse is free (paused or picking).
void tooltip(Rectangle icon, const char* name, const char* desc, Color c) {
    if (!CheckCollisionPointRec(GetMousePosition(), icon)) return;
    float tw = std::max(measure(name, 16).x, measure(desc, 16).x);
    Rectangle box = {icon.x + icon.width + 6, icon.y, tw + 16, 46};
    DrawRectangleRounded(box, roundness(box, 6), 4, gray(.1f, .85f));
    text(name, box.x + 8, box.y + 4, 16, c);
    text(desc, box.x + 8, box.y + 24, 16, WHITE);
}

void drawHud(Game& g) {
    const Player& p = g.p;
    float w = ui.w, h = ui.h;
    if (p.hp / p.maxHp <= 0.3f) DrawRectangle(0, 0, int(w) + 1, int(h) + 1, ColorAlpha(rgb(.278f, 0, 0), 0.204f));  // LowHPWarning
    for (const Portal& pt : g.portals) edgeMarker(g, pt.pos, pt.state == Portal::PURIFIED ? rgb(.6f, 1, .6f) : rgb(.8f, .5f, 1));
    for (const Enemy& e : g.enemies)
        if (e.boss() && !e.dying) edgeMarker(g, e.pos, RED);
    bool mouse = g.mode == Mode::Paused || g.mode == Mode::LevelUp;
    meter({6, 1, 199, 20}, p.hp / p.maxHp, rgb(1, .0744f, .045f), TextFormat("%d/%d", int(std::max(0.f, p.hp)), int(p.maxHp)), rgb(1, .23f, .17f));
    text(TextFormat("Kills: %d", p.kills), w * 0.243f - 24.6f, 0, 16, rgb(1, .4f, .33f));
    text(TextFormat("Gold: %d", int(p.gold)), w * 0.243f + 79.5f, 0, 16, YELLOW);
    text(TextFormat("Silver: %d", p.silver), w * 0.243f + 169, 0, 16, rgb(.72f, .72f, .72f));
    int secs = int(p.time);
    textIn(TextFormat("%02d:%02d", secs / 60, secs % 60), {5, 0, w - 5, 20}, 16, g.endTimes ? rgb(1, .2f, .2f) : WHITE);

    for (int i = 0; i < int(p.weapons.size()); i++) {
        float x = 5 + i * 68.f;
        DrawTexturePro(A.weaponSlot, {0, 0, 64, 64}, {x, 23, 64, 64}, {}, 0, Fade(WHITE, 0.765f));
        Texture2D icon = A.weaponIcons[p.weapons[i].id];
        DrawTexturePro(icon, {0, 0, float(icon.width), float(icon.height)}, {x + 6, 29, 52, 52}, {}, 0, WHITE);
        const Weapon& wp = p.weapons[i];
        text(TextFormat("Lv%d", wp.level), x + 34, 68, 12, WHITE, false, 4);
        if (mouse) tooltip({x, 23, 64, 64}, WEAPON_NAMES[wp.id], TextFormat("Level %d", wp.level), rgb(1, .65f, 0));
    }
    float iy = 91;
    for (int i = 0; i < ITEM_COUNT; i++)
        if (p.items[i]) {
            Texture2D icon = A.itemIcons[i];
            DrawTexturePro(icon, {0, 0, float(icon.width), float(icon.height)}, {5, iy, 32, 32}, {}, 0, WHITE);
            if (p.items[i] > 1) text(TextFormat("x%d", p.items[i]), 23, iy + 18, 12, WHITE, false, 4);
            if (mouse) tooltip({5, iy, 32, 32}, ITEMS[i].name, ITEMS[i].desc, ITEM_TIERS[ITEMS[i].tier].color);
            iy += 36;
        }

    for (const Enemy& e : g.enemies)
        if (e.boss() && !e.dying) {
            textIn(e.kind == GUARDIAN ? "Floor Guardian" : "The Rat King", {0, 22, w, 20}, 16, e.enraged ? e.glow : WHITE);
            meter({w / 2 - 300, 44, 600, 14}, e.hp / e.maxHp, e.enraged ? rgb(1, .35f, .15f) : rgb(.7f, .2f, .75f), "", WHITE);
            break;
        }

    if (p.exp != ui.lastExp) ui.expShowT = ui.lastExp < 0 ? 0 : 2, ui.lastExp = p.exp;
    ui.expShowT -= GetFrameTime();
    meter({0, h - 19, w, 19}, float(p.exp) / p.expNext, rgb(0, .263f, 1),
          ui.expShowT > 0 ? TextFormat("%d / %d", p.exp, p.expNext) : TextFormat("LVL %d", p.level), rgb(0, .263f, 1));

    if (g.toastT > 0) textIn(g.toast, {0, h - 140, w, 24}, 16, Fade(g.toastColor, std::min(1.f, g.toastT)), 4, Fade(BLACK, std::min(1.f, g.toastT)));

    if (g.mode == Mode::Paused || g.mode == Mode::LevelUp) statsPanel(p);
    if (imageButton(50, {w - 114.8f, 4.2f, 108.3f, 40.6f}, "PAUSE", 14.6f) && (g.mode == Mode::Play || g.mode == Mode::Paused))
        g.mode = g.mode == Mode::Play ? Mode::Paused : Mode::Play;
}

Rectangle optionRect(const Game& g, int i) {
    float x = ui.w / 2, total = -4;
    for (const Option& o : g.options) total += std::max(150.f, textWidth(o.text, 16) + 16) + 4;
    x -= total / 2;
    for (int k = 0; k < i; k++) x += std::max(150.f, textWidth(g.options[k].text, 16) + 16) + 4;
    return {x, ui.h / 2 + 60, std::max(150.f, textWidth(g.options[i].text, 16) + 16), 64};
}

void chooseOption(Game& g, int i) {
    applyOption(g, g.options[i]);
    if (--g.p.pendingLevels > 0) buildOptions(g);
    else g.mode = Mode::Play;
}

std::string clock(float seconds) { int s = int(seconds); return TextFormat("%02d:%02d", s / 60, s % 60); }

struct StatRow { const char* name; std::string value; Color color; };
// The outlined two-column summary of the victory screen; returns the y below the last row.
float statRows(float cx, float y, std::initializer_list<StatRow> rows, Color title, Color outline) {
    float c1 = 0;
    for (const StatRow& r : rows) c1 = std::max(c1, measure(r.name, 24).x);
    for (const StatRow& r : rows) {
        text(r.name, cx - 144, y, 24, title, false, 6, outline);
        text(r.value.c_str(), cx - 132 + c1, y, 24, r.color, false, 6, outline);
        y += 34;
    }
    return y;
}

void drawOverlay(Game& g) {
    float w = ui.w, h = ui.h, cx = w / 2, cy = h / 2;
    switch (g.mode) {
        case Mode::LevelUp:
            DrawRectangle(0, 0, int(w) + 1, int(h) + 1, gray(0, 0.49f));
            textIn("BLOSSOM UP!", {0, cy - 40, w, 80}, 64, WHITE);
            for (int i = 0; i < int(g.options.size()); i++) {
                Rectangle r = optionRect(g, i);
                text(TextFormat("%d", i + 1), r.x + r.width / 2, r.y + r.height + 6, 16, gray(.75f, 1), true, 4);  // key hint
                if (button(60 + i, r, g.options[i].text, true, RARITIES[g.options[i].rarity].color)) {
                    chooseOption(g, i);
                    break;
                }
            }
            break;
        case Mode::ItemGet: {
            // hud.tscn ItemGetPopup: name on top, icon in the middle, description below; a click anywhere continues.
            const ItemDef& it = ITEMS[g.gotItem];
            Rectangle r = {cx - 141, cy - 118, 286, 229};
            Texture2D icon = A.itemIcons[g.gotItem];
            DrawTexturePro(icon, {0, 0, float(icon.width), float(icon.height)}, {cx - 64 + 2, cy - 64 - 3, 128, 128}, {}, 0, WHITE);
            textIn(it.name, {0, r.y, w, 38}, 32, ITEM_TIERS[it.tier].color, 6);
            textIn(it.desc, {0, r.y + r.height - 38, w, 38}, 32, WHITE, 6);
            textIn("Click to continue", {0, r.y + r.height + 4, w, 20}, 16, gray(.75f, 1), 4);
            if (clicked(65, CheckCollisionPointRec(GetMousePosition(), r), true)) g.mode = Mode::Play;
            break;
        }
        case Mode::Paused:
            DrawRectangle(0, 0, int(w) + 1, int(h) + 1, gray(0, 0.706f));
            logo({cx - 8, cy - 160}, 2.254f, 10);
            if (imageButton(70, {cx - 306, h - 229.12f, 201.4f, 75.5f}, "RESUME", 31.9f)) g.mode = Mode::Play;
            if (imageButton(71, {cx - 98.3f, h - 229.12f, 201.4f, 75.5f}, "OPTIONS", 31.9f)) ui.options = true;
            if (imageButton(72, {cx + 109.4f, h - 229.12f, 201.4f, 75.5f}, "QUIT", 31.9f)) bankRun(g), toTitle(g);
            if (imageButton(73, {cx - 63, h * 0.883f - 23.76f, 128, 48}, "RESTART", 17)) bankRun(g), newRun(g);
            break;
        case Mode::GameOver:
            DrawRectangle(0, 0, int(w) + 1, int(h) + 1, ColorAlpha(rgb(.314f, 0, 0), 0.757f));
            textIn("GAME OVER", {0, cy - 190, w, 80}, 74, RED);
            statRows(cx, cy - 86, {{"Time Survived:", clock(g.runTime), WHITE},
                                   {"Floor Reached:", TextFormat("%d / %d", g.floor, MAX_FLOORS), WHITE},
                                   {"Enemies Slain:", TextFormat("%d", g.p.kills), WHITE},
                                   {"Gold Banked:", TextFormat("+ %d Gold", g.runGold), rgb(1, .84f, 0)}},
                     rgb(1, .7f, .7f), rgb(.2f, 0, 0));
            if (button(80, {cx - 136, cy + 76, 111, 40}, "Try Again")) newRun(g);
            else if (button(81, {cx + 24, cy + 76, 112, 40}, "Exit")) toTitle(g);
            break;
        case Mode::Victory: {
            wallpaper();
            textIn("VICTORY!!!", {0, cy - 185.57f, w, 77}, 64, WHITE);
            float y = statRows(cx, cy - 66.5f, {{"Time Survived:", clock(g.runTime), WHITE},
                                                {"Enemies Slain:", TextFormat("%d", g.p.kills), WHITE},
                                                {"Victory Bonus:", "+ 1000 Gold", rgb(1, .84f, 0)}},
                               rgb(.7f, .95f, .7f), rgb(.06f, .2f, .1f));
            if (imageButton(90, {cx - 64, y + 4, 128, 48}, "Return", 24)) toTitle(g);
            break;
        }
        default: break;
    }
}

void handleInput(Game& g) {
    if (ui.options) return;  // optionsScreen handles its own input
    switch (g.mode) {
        case Mode::Title:
            if (IsKeyPressed(KEY_ESCAPE)) g.quit = true;
            break;
        case Mode::Shop: break;  // shopScreen handles its own input
        case Mode::Play:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) g.mode = Mode::Paused;
            if (IsKeyPressed(KEY_E)) {
                int c = nearChest(g), i = nearPortal(g);
                if (c >= 0) openChest(g, c);
                else if (i >= 0 && g.portals[i].state == Portal::CORRUPTED) summonGuardian(g, i);
                else if (i >= 0) startFloor(g, g.floor + 1, g.p, g.runTime);
            }
            break;
        case Mode::Paused:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) g.mode = Mode::Play;
            break;
        case Mode::ItemGet:
            if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) g.mode = Mode::Play;
            break;
        case Mode::LevelUp:
            for (int i = 0; i < int(g.options.size()); i++)
                if (IsKeyPressed(KEY_ONE + i)) { chooseOption(g, i); break; }
            break;
        case Mode::GameOver:
            if (IsKeyPressed(KEY_R)) newRun(g);
            else if (IsKeyPressed(KEY_ESCAPE)) toTitle(g);
            break;
        case Mode::Victory:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) toTitle(g);
            break;
    }
}

void step(Game& g, float dt) {
    g.grid.build(g.enemies);
    updatePlayer(g, dt);
    updateSpawner(g, dt);
    for (Weapon& w : g.p.weapons) fireWeapon(g, w, dt);
    updateShots(g, dt);
    updateSeeds(g, dt);
    updateEnemies(g, dt);  // last: removes enemies, invalidating grid indices
    for (DamageNumber& n : g.numbers) n.t += dt;
    std::erase_if(g.numbers, [](const DamageNumber& n) { return n.t > 0.6f; });
    g.toastT -= dt;
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
    for (int i = 0; i < 1000; i++) { int t = weightedEnemy(MEADOW_SPAWNS, 0); CHECK(t == BASIC || t == RATMAN); }
    CHECK(fabsf(hyper(0.15f, 1) - 0.1304f) < 0.001f && hyper(0.15f, 0) == 0);

    // Every island is one landmass, and the portal and chests are walkable from the spawn.
    for (const MapConfig& cfg : MAPS) {
        Map m = genMap(cfg);
        int walkableCount = 0;
        for (int v = MAP_N + 1; v < MAP_N * (MAP_N - 1); v++) walkableCount += walkable(m, v);
        std::vector<bool> seen(m.field.size());
        CHECK(m.at(m.spawn) && !m.blocked(m.spawn) && int(landmass(m, vertexIndex(m.spawn), seen).size()) == walkableCount);
        CHECK(seen[vertexIndex(m.portal)] && !m.blocked(m.portal) && int(m.chests.size()) == cfg.chests && !m.trees.empty());
        for (Vector2 c : m.chests) CHECK(seen[vertexIndex(c)] && !m.blocked(c));
        for (int i = 0; i < int(m.field.size()); i++) CHECK((m.grass[i] >= 0) == (m.field[i] > m.cut));
    }

    // Guardian: enrages at half health, dies, purifies its portal and starts the end times.
    Game g;
    g.map.field.assign(MAP_N * MAP_N, 255);
    g.portals.push_back({{0, 0}});
    summonGuardian(g, 0);
    Enemy& boss = g.enemies.back();
    CHECK(g.portals[0].state == Portal::COMBAT && boss.kind == GUARDIAN && boss.hp == BOSS_HEALTH);
    hurtEnemy(g, boss, int(boss.maxHp / 2) + 1);
    CHECK(boss.enraged && boss.phase == 2);
    hurtEnemy(g, boss, int(boss.maxHp));
    CHECK(boss.dying);
    updateEnemies(g, 1);
    CHECK(g.enemies.empty() && g.portals[0].state == Portal::PURIFIED && g.bossDefeated && g.endTimes);
    CHECK(g.seeds.size() == 50);
    CHECK(g.chests.size() == 1 && g.chests[0].cost == 0);
    openChest(g, 0);
    CHECK(g.chests.empty() && std::accumulate(std::begin(g.p.items), std::end(g.p.items), 0) == 1);
    stopMusic();

    // Dashers wind up near the player, then lunge; regular enemies never outpace the player.
    Game dg;
    dg.map.field.assign(MAP_N * MAP_N, 255);
    Enemy dasher = makeEnemy(dg, {150, 0});
    dasher.type = DASHER, dasher.speed = 55, dasher.shootT = 0;
    dg.enemies.push_back(dasher);
    for (int f = 0; f < 41; f++) {
        dg.grid.build(dg.enemies);
        updateEnemies(dg, 1 / 60.f);
        if (f == 0) CHECK(dg.enemies[0].castT > 0);
    }
    CHECK(dg.enemies[0].dashT > 0 && dg.enemies[0].pos.x < 130);
    dg.runTime = 3600, dg.floor = 3;
    for (int i = 0; i < 20; i++) CHECK(spawnEnemy(dg, RUNNER, 0) && spawnEnemy(dg, RATMAN, 0));
    for (const Enemy& e : dg.enemies) CHECK(e.speed <= ENEMY_SPEED_CAP && ENEMY_SPEED_CAP < Player{}.speed);

    // Shooters creep into range instead of holding off forever, so aura and orbit builds can reach them.
    Game sg;
    sg.map.field.assign(MAP_N * MAP_N, 255);
    Enemy shooter = makeEnemy(sg, {300, 0});
    shooter.shooter = true, shooter.speed = 60, shooter.shootT = 99;
    sg.enemies.push_back(shooter);
    for (int f = 0; f < 120; f++) sg.grid.build(sg.enemies), updateEnemies(sg, 1 / 60.f);
    CHECK(sg.enemies[0].pos.x < 280);

    // A chest item pauses on the item popup.
    sg.mode = Mode::Play;
    grantItem(sg, IT_BOOTS);
    CHECK(sg.mode == Mode::ItemGet && sg.gotItem == IT_BOOTS && sg.p.items[IT_BOOTS] == 1);

    // Several levels at once queue several picks; weapon offers are only for weapons you lack.
    Game lv;
    lv.mode = Mode::Play;
    lv.p.weapons.push_back({WAND});
    gainExp(lv, 500);
    CHECK(lv.mode == Mode::LevelUp && lv.p.pendingLevels > 1 && lv.options.size() == 3);
    for (int i = 0; i < 200; i++) {
        buildOptions(lv);
        for (const Option& o : lv.options) CHECK(o.kind != OPT_WEAPON || o.index != WAND);
    }
    applyOption(lv, {OPT_WEAPON, ORBIT, ORBIT});
    CHECK(lv.p.weapons.size() == 2 && findWeapon(lv.p, ORBIT));
    lv.p.items[IT_CHARM] = 0, lv.p.evasion = 5;  // over the cap: still hittable
    int hits = 0;
    for (int i = 0; i < 200; i++) { lv.p.iframes = 0, lv.p.hp = 1e6f; hurtPlayer(lv, 1, nullptr); hits += lv.p.hp < 1e6f; }
    CHECK(hits > 40);

    // Smoke run: every weapon and three of every item for two simulated minutes on floor 2.
    Player hero;
    for (int w = 0; w < WEAPON_COUNT; w++) hero.weapons.push_back({WeaponId(w)});
    for (int& n : hero.items) n = 3;
    Game sim;
    startFloor(sim, 2, hero, 0);
    int kills = 0;
    for (int f = 0; f < 120 * 60; f++) {
        sim.p.hp = sim.p.maxHp;
        while (sim.mode == Mode::LevelUp) {
            applyOption(sim, sim.options[0]);
            if (--sim.p.pendingLevels <= 0) sim.mode = Mode::Play;
            else buildOptions(sim);
        }
        CHECK(sim.mode == Mode::Play);
        step(sim, 1 / 60.f);
        kills = sim.p.kills;
    }
    CHECK(kills > 100 && sim.map.cfg != &MAPS[0]);
    stopMusic();

    Profile pr;
    pr.coins = 1000;
    CHECK(upgradeCost(pr, PERM_MAX_HP) == 100);
    CHECK(buyUpgrade(pr, PERM_MAX_HP) && buyUpgrade(pr, PERM_MAX_HP));  // 100 + 150
    CHECK(pr.coins == 750 && pr.levels[PERM_MAX_HP] == 2 && upgradeCost(pr, PERM_MAX_HP) == 225);
    CHECK(respecRefund(pr) == 250);
    pr.weapon = POISON_AURA, pr.character = 2;
    auto path = std::filesystem::temp_directory_path() / "convallaria_selftest" / "save.txt";
    CHECK(saveProfile(pr, path));
    Profile back = loadProfile(path);
    CHECK(back.coins == 750 && back.weapon == POISON_AURA && back.character == 2 && back.levels[PERM_MAX_HP] == 2 && back.levels[PERM_SPEED] == 0);
    std::filesystem::remove_all(path.parent_path());
    CHECK(loadProfile(path).coins == 0);  // missing file means a fresh profile
    respec(pr);
    CHECK(pr.coins == 1000 && pr.levels[PERM_MAX_HP] == 0);
    puts("selftest ok");
    return 0;
}


}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "--test")) {
        InitAudioDevice();
        int r = selftest();
        CloseAudioDevice();
        return r;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "Convallaria");
    InitAudioDevice();
    SetExitKey(KEY_NULL);
    loadAssets();
    profile = loadProfile(savePath());
    applyVolume();
    playMusic("ui/music.mp3", -8);

    Game g;

    while (!WindowShouldClose() && !g.quit) {
        float dt = std::min(GetFrameTime(), 1 / 30.f);
        if (IsMusicValid(A.music)) UpdateMusicStream(A.music);
        handleInput(g);
        if (g.mode == Mode::Play) step(g, dt);

        // Godot's canvas_items stretch: the 1280x720 layout and the world both scale with the window.
        float scale = std::min(GetScreenWidth() / float(SCREEN_W), GetScreenHeight() / float(SCREEN_H));
        ui.w = GetScreenWidth() / scale, ui.h = GetScreenHeight() / scale;
        SetMouseScale(1 / scale, 1 / scale);
        Camera2D cam{{GetScreenWidth() / 2.f, GetScreenHeight() / 2.f}, g.p.pos, 0, ZOOM * scale}, canvas{{}, {}, 0, scale};
        BeginDrawing();
        ClearBackground(BLACK);
        ui.hot = -1;
        bool inRun = g.mode != Mode::Title && g.mode != Mode::Shop && g.mode != Mode::Victory;
        if (inRun && !ui.options) drawWorld(g, cam);
        BeginMode2D(canvas);
        textScale = scale;
        if (ui.options) optionsScreen();
        else if (g.mode == Mode::Title) titleScreen(g);
        else if (g.mode == Mode::Shop) shopScreen(g);
        else {
            if (inRun) drawHud(g);
            drawOverlay(g);
        }
        EndMode2D();
        if (ui.hot >= 0 && ui.hot != ui.lastHot) A.hover.play(1, -10);
        ui.lastHot = ui.hot;
        EndDrawing();
    }
    if (g.mode != Mode::Title && g.mode != Mode::Shop) bankRun(g);
    CloseAudioDevice();
    CloseWindow();
}
