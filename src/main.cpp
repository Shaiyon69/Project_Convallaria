// Convallaria: top-down survival roguelite, C++ / raylib.
#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <fstream>
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

Sound loadClip(const char* path, float from, float to) {
    Wave w = LoadWave(asset(path));
    if (w.frameCount > unsigned(to * w.sampleRate)) WaveCrop(&w, int(from * w.sampleRate), int(to * w.sampleRate));
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

// Terrain: draws the map heightfield (see Map) as smooth coast, beach, grass and animated water.
const char* TERRAIN_FS = R"(#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float cut, time, size;
uniform vec3 grass, soil, water;
out vec4 finalColor;

float hash(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { v += a * noise(p); p = p * 2.03 + 17.0; a *= 0.5; }
    return v;
}

void main() {
    vec2 w = fragTexCoord * size;  // world pixels
    float h = texture(texture0, fragTexCoord).r;
    float aa = max(fwidth(h), 1e-4);
    float land = smoothstep(cut - aa, cut + aa, h);

    // Water: shallows to deep, drifting light, the shore's shadow and rolling foam.
    float depth = clamp((cut - h) / 0.15, 0.0, 1.0);
    vec3 wc = mix(water * 1.3 + 0.06, water * 0.55, smoothstep(0.0, 1.0, depth));
    float ripple = noise(w * 0.04 + vec2(time * 0.35, time * 0.2)) * noise(w * 0.07 - vec2(time * 0.25, -time * 0.3));
    wc += smoothstep(0.22, 0.32, ripple) * 0.07 * (1.0 - depth * 0.5);
    float hs = texture(texture0, fragTexCoord - vec2(1.5, 2.5) / vec2(textureSize(texture0, 0))).r;
    wc *= 1.0 - 0.3 * smoothstep(cut - 0.02, cut + 0.02, hs);
    float shore = clamp((cut - h) / 0.06, 0.0, 1.0);
    float waves = sin(shore * 16.0 - time * 2.2 + noise(w * 0.05) * 3.0);
    float foam = 1.0 - smoothstep(0.0, 0.2, shore) + smoothstep(0.8, 1.0, waves) * (1.0 - shore) * 0.6;
    wc = mix(wc, vec3(0.95, 0.98, 1.0), clamp(foam, 0.0, 1.0) * 0.65);

    // Land: wet sand, beach, grass with soft patches and flowers, soil on the heights.
    float up = h - cut, n = fbm(w * 0.02), fine = noise(w * 0.3);
    vec3 g = grass * (0.85 + 0.3 * n);
    g = mix(g, grass * vec3(1.15, 1.12, 0.75), smoothstep(0.55, 0.75, fbm(w * 0.008 + 31.0)) * 0.6);
    g *= 0.95 + 0.1 * fine;
    vec3 s = soil * (0.85 + 0.3 * fbm(w * 0.05 + 7.0));
    vec3 lc = mix(g, s, 0.85 * smoothstep(0.56, 0.65, h + (n - 0.5) * 0.08));
    vec3 sand = mix(soil, vec3(0.96, 0.89, 0.7), 0.6) * (0.95 + 0.1 * fine);
    lc = mix(sand, lc, smoothstep(0.025, 0.05, up + (n - 0.5) * 0.02));
    vec2 cell = floor(w / 10.0);
    float r = hash(cell);
    if (r > 0.94 && up > 0.06 && h < 0.56) {
        vec2 c = (cell + 0.25 + 0.5 * vec2(hash(cell + 1.3), hash(cell + 7.1))) * 10.0;
        vec3 fc = r > 0.993 ? lc * vec3(1.5, 1.0, 1.4) : lc * 0.72;
        lc = mix(lc, fc, 1.0 - smoothstep(0.8, 1.8, length(w - c)));
    }
    lc *= mix(0.78, 1.0, smoothstep(0.0, 0.015, up));
    finalColor = vec4(mix(wc, lc, land), 1.0);
}
)";

struct Assets {
    Texture2D player, slime, ratman, guardian, portal, seed, projectile, aura, title, chest, shadow;
    Shader terrain;
    Font font;
    Sfx orb, levelup, hurt, wandShot, enemyShot, slimeHit, slimeDeath, ratmanDeath, win, hover, click;
    Music music{};
    std::string musicPath;
} A;

void loadAssets() {
    A.player = LoadTexture(asset("player/woman.png"));
    A.slime = LoadTexture(asset("enemies/blob.png"));
    A.ratman = LoadTexture(asset("enemies/ratman/ratman.png"));
    A.guardian = LoadTexture(asset("enemies/bob.png"));
    A.portal = LoadTexture(asset("world/portal.png"));
    A.seed = LoadTexture(asset("drops/exp/seed.png"));
    A.projectile = LoadTexture(asset("weapons/wand/projectile.png"));
    A.aura = LoadTexture(asset("weapons/poison/poison_radius..png"));
    A.chest = LoadTexture(asset("drops/chest/chest.png"));
    Image shadow = GenImageGradientRadial(32, 32, 0.2f, {0, 0, 0, 120}, {0, 0, 0, 0});
    A.shadow = LoadTextureFromImage(shadow);
    UnloadImage(shadow);
    SetTextureFilter(A.shadow, TEXTURE_FILTER_BILINEAR);
    A.terrain = LoadShaderFromMemory(nullptr, TERRAIN_FS);
    // Loaded large and mipmapped so it stays crisp at every size, including world-space damage numbers.
    A.font = LoadFontEx(asset("ui/fonts/Poppins-SemiBold.ttf"), 64, nullptr, 0);
    if (!IsFontValid(A.font)) A.font = GetFontDefault();
    GenTextureMipmaps(&A.font.texture);
    SetTextureFilter(A.font.texture, TEXTURE_FILTER_TRILINEAR);
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
    SetMusicVolume(A.music, db(volumeDb));
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
    int levels[PERM_COUNT]{};
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
        out << "coins " << pr.coins << "\nweapon " << int(pr.weapon) << "\n";
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
        else if (key == "upgrade") {
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

// The island is a heightfield, FIELD_RES cells per tile, and land is wherever the
// bilinear height beats the cut. The GPU draws the same field with linear filtering,
// so the smooth coastline on screen is exactly the one you collide with.
constexpr int FIELD_RES = 4, FIELD_N = MAP_N * FIELD_RES;
constexpr float CELL = float(TILE) / FIELD_RES;

struct Map {
    const MapConfig* cfg = &MAPS[0];
    std::vector<uint8_t> field;  // height, 0-255
    uint8_t cut = 0;
    Texture2D tex{};
    Vector2 spawn{}, portal{};
    std::vector<Vector2> chests;
    Color water{}, grass{}, soil{};

    float height(Vector2 p) const {
        float fx = p.x / CELL + FIELD_N / 2 - 0.5f, fy = p.y / CELL + FIELD_N / 2 - 0.5f;
        int x0 = int(floorf(fx)), y0 = int(floorf(fy));
        float tx = fx - x0, ty = fy - y0;
        auto h = [&](int x, int y) { return x < 0 || y < 0 || x >= FIELD_N || y >= FIELD_N ? 0.f : float(field[y * FIELD_N + x]); };
        return Lerp(Lerp(h(x0, y0), h(x0 + 1, y0), tx), Lerp(h(x0, y0 + 1), h(x0 + 1, y0 + 1), tx), ty);
    }
    bool at(Vector2 p) const { return height(p) > cut; }
    bool landAround(Vector2 p, int r) const {
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (!at({p.x + dx * TILE, p.y + dy * TILE})) return false;
        return true;
    }
};

const Color WATER = {52, 118, 170, 255}, GRASS = {92, 158, 70, 255}, SOIL = {139, 108, 66, 255};

int cellIndex(Vector2 p) { return (int(floorf(p.y / CELL)) + FIELD_N / 2) * FIELD_N + int(floorf(p.x / CELL)) + FIELD_N / 2; }

// Cells connected to `start` above the cut. Walking needs 4-connection (movement is
// per axis); `diagonal` also follows corners so coastline nubs count as part of the island.
// Land never touches the array edge.
std::vector<int> landmass(const std::vector<uint8_t>& field, uint8_t cut, int start, std::vector<bool>& seen, bool diagonal) {
    std::vector<int> out, stack = {start};
    seen[start] = true;
    while (!stack.empty()) {
        int c = stack.back();
        stack.pop_back();
        out.push_back(c);
        for (int n : {c - 1, c + 1, c - FIELD_N, c + FIELD_N, c - FIELD_N - 1, c - FIELD_N + 1, c + FIELD_N - 1, c + FIELD_N + 1}) {
            if (field[n] > cut && !seen[n]) seen[n] = true, stack.push_back(n);
            if (!diagonal && n == c + FIELD_N) break;
        }
    }
    return out;
}

// Same island recipe as the Godot map: fbm Perlin noise with a radial falloff.
// Only the biggest landmass is kept so the portal and chests are always reachable;
// the rest sinks under the waterline and shows as reefs.
Map genMap(const MapConfig& cfg) {
    Map m;
    m.cfg = &cfg;
    const Biome& biome = BIOMES[cfg.biome];
    float radius = float(cfg.radius * FIELD_RES);
    m.water = ColorTint(WATER, biome.water);
    m.grass = ColorTint(GRASS, biome.grass);
    m.soil = ColorTint(SOIL, biome.soil);
    m.cut = uint8_t(cfg.cut * 255);
    std::vector<uint8_t> raw(FIELD_N * FIELD_N, 0);
    Image noise = GenImagePerlinNoise(FIELD_N, FIELD_N, rndi(100000), rndi(100000), cfg.noiseScale * MAP_N);
    Color* px = LoadImageColors(noise);
    for (int y = 0; y < FIELD_N; y++)
        for (int x = 0; x < FIELD_N; x++) {
            float dx = x + 0.5f - FIELD_N / 2, dy = y + 0.5f - FIELD_N / 2, d = sqrtf(dx * dx + dy * dy) / radius;
            if (d >= 1) continue;
            float falloff = 1.f - powf(fabsf(d - cfg.ring) / (1.f - cfg.ring), 2.5f);
            raw[y * FIELD_N + x] = uint8_t(std::clamp(px[y * FIELD_N + x].r / 255.f * falloff, 0.f, 1.f) * 255);
        }
    UnloadImageColors(px);
    UnloadImage(noise);

    std::vector<bool> seen(raw.size());
    std::vector<int> biggest;
    for (int i = 0; i < int(raw.size()); i++)
        if (raw[i] > m.cut && !seen[i]) {
            std::vector<int> part = landmass(raw, m.cut, i, seen, true);
            if (part.size() > biggest.size()) biggest.swap(part);
        }
    std::vector<bool> keep(raw.size());
    for (int i : biggest) keep[i] = true;
    for (int i = 0; i < int(raw.size()); i++)
        if (raw[i] > m.cut && !keep[i]) raw[i] = uint8_t(std::max(0, m.cut - 1 - (raw[i] - m.cut) / 2));  // mirrored, so the reef edge stays smooth

    std::vector<Vector2> land;
    float best = 1e9f;
    for (int i : biggest) {
        Vector2 center = {(i % FIELD_N - FIELD_N / 2 + 0.5f) * CELL, (i / FIELD_N - FIELD_N / 2 + 0.5f) * CELL};
        land.push_back(center);
        float d2 = Vector2LengthSqr(center);
        if (d2 < best) best = d2, m.spawn = center;
    }
    if (IsWindowReady()) {  // the selftest runs without a window
        Image img = {raw.data(), FIELD_N, FIELD_N, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
        m.tex = LoadTextureFromImage(img);
        SetTextureFilter(m.tex, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(m.tex, TEXTURE_WRAP_CLAMP);
    }
    m.field = std::move(raw);

    m.portal = m.spawn;
    for (int a = 0; a < 200 && !land.empty(); a++) {
        Vector2 c = land[rndi(int(land.size()))];
        if (m.landAround(c, 3) && Vector2Distance(c, m.spawn) > 9 * TILE) { m.portal = c; break; }
    }
    for (int a = 0; a < cfg.chests * 20 && int(m.chests.size()) < cfg.chests && !land.empty(); a++) {
        Vector2 c = land[rndi(int(land.size()))];
        if (m.landAround(c, 1) && Vector2Distance(c, m.spawn) > 6 * TILE && Vector2Distance(c, m.portal) > 4 * TILE)
            m.chests.push_back(c);
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
    int facing = DOWN;
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
enum class Mode { Title, Shop, Play, LevelUp, Paused, GameOver, Victory };

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
    int runGold = 0;
};

int chestCost(int floor) { return 10 + 8 * (floor - 1); }

void startFloor(Game& g, int floor, Player player, float runTime) {  // player by value: g is reset below
    const MapConfig* prev = g.map.cfg;
    if (g.map.tex.id) UnloadTexture(g.map.tex);
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
    if (floor < MAX_FLOORS) g.portals.push_back({g.map.portal});
    else g.finalBossT = 2.5f;
    g.mode = Mode::Play;
    playMusic("ui/music.mp3", -8);
}

// A fresh character with the shop upgrades applied.
void newRun(Game& g) {
    Player p;
    p.weapons.push_back({profile.weapon});
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

void showToast(Game& g, std::string text, Color c) { g.toast = std::move(text), g.toastColor = c, g.toastT = 3; }

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
    const ItemDef& it = ITEMS[id];
    showToast(g, TextFormat("%s  -  %s", it.name, it.desc), ITEM_TIERS[it.tier].color);
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
        float m = runMinutes(g), floorHp = 1 + (g.floor - 1) * 0.5f;
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
    e.hp = float(int(int(BOSS_HEALTH * (1 + m * 0.2f)) * (1 + (f - 1) * 1.5f + m * 0.25f)));
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
        if (g.map.at(nx)) p.pos.x = nx.x;
        Vector2 ny = {p.pos.x, p.pos.y + in.y * speed * dt};
        if (g.map.at(ny)) p.pos.y = ny.y;
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
        g.difficultyT = 5;
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
        s.pos = Vector2Add(s.pos, Vector2Scale(s.dir, 250 * dt));
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
                    want = dist < 180 ? Vector2Scale(dir, -speed * 0.6f) : Vector2{};  // hold range, back off if rushed
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
            if (g.map.at(Vector2{e.pos.x + step.x, e.pos.y})) e.pos.x += step.x;
            if (g.map.at(Vector2{e.pos.x, e.pos.y + step.y})) e.pos.y += step.y;
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

void text(const char* s, float x, float y, float size, Color c, bool center = false) {
    if (center) x -= MeasureTextEx(A.font, s, size, 0).x / 2;
    float o = std::max(1.f, size / 16);
    DrawTextEx(A.font, s, {x, y + o}, size, 0, Fade(BLACK, 0.45f * c.a / 255.f));
    DrawTextEx(A.font, s, {x, y}, size, 0, c);
}

// A rounded card with a soft drop shadow and a hairline edge.
void panel(Rectangle r, Color bg = Fade(rgb(.06f, .08f, .1f), 0.82f), Color edge = Fade(WHITE, 0.08f), float radius = 12) {
    float round = std::min(1.f, 2 * radius / std::min(r.width, r.height));
    DrawRectangleRounded({r.x, r.y + 4, r.width, r.height}, round, 12, Fade(BLACK, 0.25f * bg.a / 255.f));
    DrawRectangleRounded(r, round, 12, bg);
    DrawRectangleRoundedLinesEx(r, round, 12, 1.5f, edge);
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

// Call inside BeginMode2D. The quad is padded far past the field (clamped to deep water) so no edge ever shows.
void drawTerrain(const Map& map) {
    float t = float(GetTime()), extent = MAP_N * TILE, pad = 4000, cut = map.cut / 255.f;
    auto uniform = [&](const char* name, const void* v, int type) { SetShaderValue(A.terrain, GetShaderLocation(A.terrain, name), v, type); };
    auto color = [&](const char* name, Color c) { Vector3 v = {c.r / 255.f, c.g / 255.f, c.b / 255.f}; uniform(name, &v, SHADER_UNIFORM_VEC3); };
    uniform("cut", &cut, SHADER_UNIFORM_FLOAT);
    uniform("time", &t, SHADER_UNIFORM_FLOAT);
    uniform("size", &extent, SHADER_UNIFORM_FLOAT);
    color("grass", map.grass), color("soil", map.soil), color("water", map.water);
    BeginShaderMode(A.terrain);
    DrawTexturePro(map.tex, {-pad / CELL, -pad / CELL, FIELD_N + 2 * pad / CELL, FIELD_N + 2 * pad / CELL},
                   {-BOUND * TILE - pad, -BOUND * TILE - pad, extent + 2 * pad, extent + 2 * pad}, {}, 0, WHITE);
    EndShaderMode();
}

void drawWorld(const Game& g, Camera2D cam) {
    const Player& p = g.p;
    float t = float(GetTime());
    BeginMode2D(cam);
    drawTerrain(g.map);
    auto shadow = [](Vector2 at, float w) { DrawTexturePro(A.shadow, {0, 0, 32, 32}, {at.x, at.y, w, w * 0.4f}, {w / 2, w * 0.2f}, 0, WHITE); };

    for (const Portal& pt : g.portals) {
        int col = pt.state == Portal::PURIFIED ? 3 + int(t * 5) % 2 : int(t * 5) % 3;
        drawFrame(A.portal, col, 0, pt.pos, 1.5f, WHITE);
    }

    for (const Chest& c : g.chests) {
        shadow({c.pos.x, c.pos.y + 6}, 22);
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

    for (const Enemy& e : g.enemies)
        if (!e.dying) shadow({e.pos.x, e.pos.y + 8 * e.scale}, 16 * e.scale);
    shadow({p.pos.x, p.pos.y + 9}, 18);
    for (const Enemy& e : g.enemies) drawEnemy(e, t);

    Color tint = p.speedT > 0 ? rgb(.5f, .8f, 1) : WHITE;
    if (p.iframes > 0) tint = Fade(tint, int(p.iframes * 10) % 2 ? 0.3f : 1.f);
    drawFrame(A.player, p.moving ? int(p.anim * 5) % 4 : 0, p.facing, p.pos, 1, tint);

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
                DrawTexturePro(A.projectile, {0, 0, 16, 16}, {at.x, at.y, size, size}, {size / 2, size / 2}, w.spin * RAD2DEG * 3, rgb(.6f, 1, .5f));
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
        DrawTextEx(A.font, txt, {at.x + 0.5f, at.y + 0.5f}, 8, 0, Fade(BLACK, 1 - n.t / 0.6f));
        DrawTextEx(A.font, txt, at, 8, 0, Fade(WHITE, 1 - n.t / 0.6f));
    }

    auto prompt = [&](const char* label, Vector2 at) {
        Vector2 size = MeasureTextEx(A.font, label, 9, 0.5f);
        DrawRectangleRounded({at.x - size.x / 2 - 5, at.y - size.y / 2 - 1, size.x + 10, size.y + 2}, 1, 8, Fade(BLACK, 0.6f));
        DrawTextEx(A.font, label, {at.x - size.x / 2, at.y - size.y / 2}, 9, 0.5f, WHITE);
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

// A pill-shaped meter with a glossy fill.
void bar(Rectangle r, float frac, Color c) {
    DrawRectangleRounded(r, 1, 12, Fade(BLACK, 0.55f));
    frac = std::clamp(frac, 0.f, 1.f);
    if (frac <= 0) return;
    Rectangle f = {r.x, r.y, std::max(r.height, r.width * frac), r.height};
    DrawRectangleRounded(f, 1, 12, c);
    if (f.width > 6) DrawRectangleRounded({f.x + 3, f.y + 1, f.width - 6, f.height * 0.4f}, 1, 12, Fade(WHITE, 0.2f));
}

void coin(float x, float y, float r, Color c) {
    DrawCircleV({x, y}, r, ColorBrightness(c, -0.3f));
    DrawCircleV({x, y - 1}, r - 1.5f, c);
    DrawCircleV({x - r * 0.3f, y - r * 0.35f}, r * 0.3f, Fade(WHITE, 0.5f));
}

void drawHud(const Game& g) {
    const Player& p = g.p;
    float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
    const Color card = Fade(rgb(.06f, .08f, .1f), 0.82f);
    bar({16, 10, sw - 32, 8}, float(p.exp) / p.expNext, rgb(.35f, .9f, .6f));

    panel({16, 28, 300, 64});
    DrawCircleV({50, 60}, 22, rgb(.35f, .9f, .6f));
    DrawCircleV({50, 60}, 19, rgb(.08f, .12f, .12f));
    text(TextFormat("%d", p.level), 50, 47, 24, WHITE, true);
    text("LEVEL", 84, 31, 13, Fade(WHITE, 0.55f));
    bar({84, 50, 218, 20}, p.hp / p.maxHp, p.hp < p.maxHp * 0.3f ? rgb(1, .3f, .25f) : rgb(.9f, .25f, .35f));
    text(TextFormat("%d / %d", int(std::max(0.f, p.hp)), int(p.maxHp)), 193, 50, 16, WHITE, true);
    text(g.floor == MAX_FLOORS ? "Final Floor" : TextFormat("Floor %d  -  %s", g.floor, g.map.cfg->name), 20, 100, 18, rgb(.8f, .9f, .7f));

    int secs = int(p.time);
    panel({sw / 2 - 72, 28, 144, 48}, card, g.endTimes ? Fade(rgb(1, .3f, .3f), 0.8f) : Fade(WHITE, 0.08f), 24);
    text(TextFormat("%02d:%02d", secs / 60, secs % 60), sw / 2, 34, 30, g.endTimes ? rgb(1, .35f, .35f) : WHITE, true);

    float rx = sw - 196;
    panel({rx, 28, 180, 96});
    coin(rx + 22, 47, 8, rgb(.9f, .3f, .3f));
    text(TextFormat("%d", p.kills), rx + 40, 36, 20, WHITE);
    coin(rx + 22, 75, 8, rgb(.8f, .82f, .88f));
    text(TextFormat("%d", p.silver), rx + 40, 64, 20, rgb(.85f, .87f, .92f));
    coin(rx + 22, 103, 8, rgb(1, .78f, .15f));
    text(TextFormat("%d", int(p.gold)), rx + 40, 92, 20, rgb(1, .84f, .3f));

    int owned = 0;
    for (int i = 0; i < ITEM_COUNT; i++) owned += p.items[i] > 0;
    if (owned) {
        panel({rx, 132, 180, 14.f + owned * 22});
        float iy = 139;
        for (int i = 0; i < ITEM_COUNT; i++)
            if (p.items[i]) {
                Color tc = ITEM_TIERS[ITEMS[i].tier].color;
                DrawCircleV({rx + 16, iy + 10}, 4, tc);
                text(ITEMS[i].name, rx + 28, iy, 15, WHITE);
                if (p.items[i] > 1) {
                    const char* n = TextFormat("x%d", p.items[i]);
                    text(n, rx + 168 - MeasureTextEx(A.font, n, 15, 0).x, iy, 15, tc);
                }
                iy += 22;
            }
    }

    for (int i = 0; i < int(p.weapons.size()); i++) {
        const Weapon& w = p.weapons[i];
        const char* label = TextFormat("%s  Lv %d", WEAPON_NAMES[w.id], w.level);
        Rectangle r = {16, sh - 50 - i * 42.f, MeasureTextEx(A.font, label, 16, 0).x + 28, 34};
        panel(r, card, Fade(rgb(.55f, .8f, .45f), 0.5f), 17);
        text(label, r.x + 14, r.y + 7, 16, WHITE);
    }

    if (g.toastT > 0) {
        float a = std::min(1.f, g.toastT), tw = MeasureTextEx(A.font, g.toast.c_str(), 20, 0).x + 40;
        panel({sw / 2 - tw / 2, sh - 136, tw, 42}, Fade(card, 0.85f * a), Fade(g.toastColor, a), 21);
        text(g.toast.c_str(), sw / 2, sh - 127, 20, Fade(g.toastColor, a), true);
    }
    const char* fps = TextFormat("%d FPS  %zu enemies", GetFPS(), g.enemies.size());
    text(fps, sw - 16 - MeasureTextEx(A.font, fps, 14, 0).x, sh - 26, 14, Fade(WHITE, 0.45f));

    for (const Enemy& e : g.enemies)
        if (e.boss() && !e.dying) {
            float w = std::min(600.f, sw - 80);
            text(e.kind == GUARDIAN ? "Floor Guardian" : "The Rat King", sw / 2, 84, 20, e.enraged ? e.glow : WHITE, true);
            bar({sw / 2 - w / 2, 112, w, 14}, e.hp / e.maxHp, e.enraged ? rgb(1, .35f, .15f) : rgb(.7f, .2f, .75f));
            break;
        }
}

// Greedy word wrap; explicit newlines are kept.
std::string wrap(const std::string& s, float size, float width) {
    std::string out, line;
    for (size_t i = 0; i <= s.size();) {
        size_t j = std::min(s.find_first_of(" \n", i), s.size());
        std::string word = s.substr(i, j - i), next = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureTextEx(A.font, next.c_str(), size, 0).x > width) out += line + "\n", line = word;
        else line = next;
        if (j == s.size() || s[j] == '\n') out += line + (j == s.size() ? "" : "\n"), line.clear();
        i = j + 1;
    }
    return out;
}

Rectangle optionRect(int i) {
    float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
    float w = 320, h = 170, gap = 30, x0 = sw / 2 - (3 * w + 2 * gap) / 2;
    return {x0 + i * (w + gap), sh / 2 - h / 2 + 20, w, h};
}

// ---------------------------------------------------------------- menus

// Immediate-mode buttons: drawn and clicked in the same call.
struct Ui { int hot = -1, lastHot = -1; std::string hint; } ui;

bool button(int id, Rectangle r, const char* label, bool enabled, Color accent = rgb(.55f, .8f, .45f), float size = 22) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    if (hover) ui.hot = id;
    Color base = rgb(.1f, .12f, .15f);
    Rectangle d = hover && enabled ? Rectangle{r.x, r.y - 2, r.width, r.height} : r;
    Color bg = !enabled ? rgb(.08f, .09f, .1f) : hover ? ColorLerp(base, accent, 0.28f) : base;
    panel(d, Fade(bg, 0.94f), enabled ? Fade(accent, hover ? 1.f : 0.55f) : Fade(WHITE, 0.06f), 10);
    Vector2 m = MeasureTextEx(A.font, label, size, 0);
    text(label, d.x + (d.width - m.x) / 2, d.y + (d.height - m.y) / 2, size, enabled ? WHITE : Fade(WHITE, 0.35f));
    bool clicked = enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (clicked) A.click.play(1, -6);
    return clicked;
}

// A slow drift over the current island.
void menuBackground(const Game& g) {
    float sw = float(GetScreenWidth()), sh = float(GetScreenHeight()), t = float(GetTime()) * 0.05f;
    float r = g.map.cfg->radius * TILE * 0.5f;
    BeginMode2D({{sw / 2, sh / 2}, {cosf(t) * r, sinf(t * 0.8f) * r}, 0, 1.5f});
    drawTerrain(g.map);
    EndMode2D();
    DrawRectangleGradientV(0, 0, int(sw), int(sh), Fade(BLACK, 0.3f), Fade(BLACK, 0.75f));
}

void openShop(Game& g) {
    g.mode = Mode::Shop;
    ui.hint = "Prepare for your journey.";
    playMusic("ui/shopping.wav", -8);
}

void titleScreen(Game& g) {
    float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
    menuBackground(g);
    Rectangle src = {0, 0, float(A.title.width), float(A.title.height)};
    float w = A.title.width * 4.f, h = A.title.height * 4.f, y = sh / 2 - 200 + sinf(float(GetTime()) * 2) * 8;
    DrawTexturePro(A.title, src, {sw / 2 - w / 2 + 4, y + 8, w, h}, {}, 0, Fade(BLACK, 0.5f));
    DrawTexturePro(A.title, src, {sw / 2 - w / 2, y, w, h}, {}, 0, WHITE);
    if (button(30, {sw / 2 - 150, sh / 2 + 20, 300, 56}, "Start", true, rgb(.55f, .8f, .45f), 26) || IsKeyPressed(KEY_ENTER)) openShop(g);
    if (button(31, {sw / 2 - 150, sh / 2 + 96, 300, 56}, "Quit", true, GRAY, 26)) g.quit = true;
    text(TextFormat("Coins: %d", profile.coins), sw / 2, sh - 60, 22, rgb(1, .84f, 0), true);
}

void shopScreen(Game& g) {
    float sw = float(GetScreenWidth());
    menuBackground(g);
    float x0 = std::max(20.f, sw / 2 - 560), x1 = x0 + 540, top = 40;
    text("Shop", x0, top, 48, rgb(.95f, .95f, .85f));
    const char* coins = TextFormat("Total Coins: %d", profile.coins);
    text(coins, x1 + 540 - MeasureTextEx(A.font, coins, 28, 1).x, top + 12, 28, rgb(1, .84f, 0));
    std::string hover;

    panel({x0 - 16, top + 68, 512, 52 + int(PERM_COUNT) * 52.f});
    panel({x1 - 16, top + 68, 572, 108});
    panel({x1 - 16, top + 184, 572, 316});
    text("Permanent Upgrades", x0, top + 80, 24, rgb(1, .9f, .5f));
    for (int i = 0; i < PERM_COUNT; i++) {
        const PermUpgrade& u = PERM_UPGRADES[i];
        int cost = upgradeCost(profile, i);
        Rectangle r = {x0, top + 116 + i * 52.f, 480, 46};
        const char* label = cost < 0 ? TextFormat("%s  (MAX)", u.name) : TextFormat("%s  Lv %d/%d  -  %d", u.name, profile.levels[i], u.maxLevel, cost);
        if (button(i, r, label, cost >= 0 && profile.coins >= cost, rgb(1, .84f, 0), 22) && buyUpgrade(profile, i)) {
            save();
            ui.hint = "Upgrade purchased!";
        }
        if (CheckCollisionPointRec(GetMousePosition(), r)) {
            bool pct = i != PERM_MAX_HP && i != PERM_SPEED && i != PERM_REGEN;
            float scale = pct ? 100.f : 1.f;
            std::string now = fmtNum(permBoost(profile, PermId(i)) * scale), next = fmtNum((profile.levels[i] + 1) * u.boost * scale);
            hover = cost < 0 ? TextFormat("Maximum level reached for %s.", u.name)
                             : TextFormat("Increases %s. Current: +%s%s -> Next: +%s%s", u.name, now.c_str(), pct ? "%" : "", next.c_str(), pct ? "%" : "");
        }
    }

    text("Starting Weapon", x1, top + 80, 24, rgb(1, .9f, .5f));
    for (int w = 0; w < WEAPON_COUNT; w++) {
        Rectangle r = {x1 + w * 182.f, top + 116, 176, 50};
        bool selected = profile.weapon == w;
        if (button(10 + w, r, selected ? TextFormat("[ %s ]", WEAPON_NAMES[w]) : WEAPON_NAMES[w], true, selected ? rgb(.5f, 1, .5f) : GRAY, 22)) {
            profile.weapon = WeaponId(w);
            save();
        }
        if (CheckCollisionPointRec(GetMousePosition(), r)) hover = TextFormat("Start your run equipped with the %s.", WEAPON_NAMES[w]);
    }

    text("Base Stats", x1, top + 196, 24, rgb(1, .9f, .5f));
    float y = top + 232;
    auto stat = [&](const char* name, const char* value, Color c) {
        text(name, x1, y, 22, c);
        text(value, x1 + 220, y, 22, WHITE);
        y += 32;
    };
    auto pct = [&](PermId id) { return int(roundf(permBoost(profile, id) * 100)); };
    stat("Max HP", TextFormat("%d", int(250 + permBoost(profile, PERM_MAX_HP))), rgb(.6f, 1, .6f));
    stat("Speed", TextFormat("%d", int(165 + permBoost(profile, PERM_SPEED))), rgb(.4f, .9f, 1));
    stat("Damage", TextFormat("+%d%%", pct(PERM_DAMAGE)), rgb(1, .45f, .35f));
    stat("HP Regen", TextFormat("%s/s", fmtNum(permBoost(profile, PERM_REGEN)).c_str()), rgb(1, .7f, .8f));
    stat("Dodge", TextFormat("%d%%", pct(PERM_EVASION)), rgb(.6f, .8f, 1));
    stat("Thorns", TextFormat("%d%%", pct(PERM_ARMOR)), rgb(1, .65f, .3f));
    stat("Coin Bonus", TextFormat("+%d%%", pct(PERM_GREED)), rgb(1, .84f, 0));
    stat("EXP Bonus", TextFormat("+%d%%", pct(PERM_EXP_GAIN)), rgb(.5f, 1, .7f));

    float by = top + 116 + int(PERM_COUNT) * 52.f + 24;
    int refund = respecRefund(profile);
    if (button(20, {x0, by, 230, 54}, "Respec", refund > 0, rgb(1, .5f, .4f))) {
        respec(profile);
        save();
        ui.hint = TextFormat("Upgrades reset! Refunded %d coins.", refund);
    }
    if (button(21, {x0 + 250, by, 230, 54}, "Back", true, GRAY) || IsKeyPressed(KEY_ESCAPE)) {
        g.mode = Mode::Title;
        playMusic("ui/music.mp3", -8);
    }
    if (button(22, {x1, by, 540, 54}, "Start Run", true, rgb(.5f, 1, .5f), 28) || IsKeyPressed(KEY_ENTER)) newRun(g);
    text(hover.empty() ? ui.hint.c_str() : hover.c_str(), x0, by + 72, 20, Fade(WHITE, 0.8f));
}

void drawOverlay(const Game& g) {
    float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
    if (g.mode == Mode::Play || g.mode == Mode::Title || g.mode == Mode::Shop) return;
    DrawRectangleGradientV(0, 0, int(sw), int(sh), Fade(BLACK, 0.45f), Fade(BLACK, 0.75f));
    int secs = int(g.runTime);
    switch (g.mode) {
        case Mode::LevelUp: {
            text(g.p.pendingLevels > 1 ? TextFormat("LEVEL UP!  x%d", g.p.pendingLevels) : "LEVEL UP!", sw / 2, sh / 2 - 150, 48, rgb(1, .8f, .1f), true);
            for (int i = 0; i < int(g.options.size()); i++) {
                Rectangle r = optionRect(i);
                const Option& o = g.options[i];
                Color rc = RARITIES[o.rarity].color;
                bool hover = CheckCollisionPointRec(GetMousePosition(), r);
                if (hover) r.y -= 6;
                panel(r, hover ? rgb(.13f, .15f, .19f) : rgb(.09f, .1f, .13f), Fade(rc, hover ? 1.f : 0.6f), 16);
                DrawRectangleRounded({r.x + 16, r.y + 16, 30, 30}, 0.4f, 8, Fade(rc, 0.2f));
                text(TextFormat("%d", i + 1), r.x + 31, r.y + 18, 20, rc, true);
                const char* rarity = TextFormat("%c%s", RARITIES[o.rarity].id[0] - 32, RARITIES[o.rarity].id + 1);
                text(rarity, r.x + r.width - 16 - MeasureTextEx(A.font, rarity, 16, 0).x, r.y + 22, 16, rc);
                text(wrap(o.text, 22, r.width - 36).c_str(), r.x + 18, r.y + 64, 22, WHITE);
            }
            break;
        }
        case Mode::Paused:
            panel({sw / 2 - 270, sh / 2 - 90, 540, 160});
            text("Paused", sw / 2, sh / 2 - 60, 56, WHITE, true);
            text("Esc to resume  -  Q to end the run", sw / 2, sh / 2 + 10, 24, Fade(WHITE, 0.8f), true);
            break;
        case Mode::GameOver:
            panel({sw / 2 - 400, sh / 2 - 130, 800, 270});
            text("You Died", sw / 2, sh / 2 - 100, 64, rgb(1, .3f, .3f), true);
            text(TextFormat("Floor %d  -  %02d:%02d  -  Level %d  -  %d kills", g.floor, secs / 60, secs % 60, g.p.level, g.p.kills), sw / 2, sh / 2, 26, WHITE, true);
            text(TextFormat("+%d gold banked", g.runGold), sw / 2, sh / 2 + 44, 24, rgb(1, .84f, 0), true);
            text("R to retry  -  Enter for shop  -  Esc for title", sw / 2, sh / 2 + 90, 22, Fade(WHITE, 0.8f), true);
            break;
        case Mode::Victory:
            panel({sw / 2 - 330, sh / 2 - 150, 660, 340});
            text("Victory!", sw / 2, sh / 2 - 120, 72, rgb(.7f, .95f, .7f), true);
            text(TextFormat("Time Survived  %02d:%02d", secs / 60, secs % 60), sw / 2, sh / 2 - 20, 26, WHITE, true);
            text(TextFormat("Enemies Slain  %d", g.p.kills), sw / 2, sh / 2 + 14, 26, WHITE, true);
            text("Victory Bonus  +1000 Gold", sw / 2, sh / 2 + 48, 26, rgb(1, .84f, 0), true);
            text(TextFormat("Gold Banked  %d", g.runGold), sw / 2, sh / 2 + 82, 26, rgb(1, .84f, 0), true);
            text("Enter to return to the shop", sw / 2, sh / 2 + 140, 22, Fade(WHITE, 0.8f), true);
            break;
        default: break;
    }
}

void handleInput(Game& g) {
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
            if (IsKeyPressed(KEY_Q)) bankRun(g), openShop(g);
            break;
        case Mode::LevelUp:
            for (int i = 0; i < int(g.options.size()); i++)
                if (IsKeyPressed(KEY_ONE + i) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), optionRect(i)))) {
                    applyOption(g, g.options[i]);
                    if (--g.p.pendingLevels > 0) buildOptions(g);
                    else g.mode = Mode::Play;
                    break;
                }
            break;
        case Mode::GameOver:
            if (IsKeyPressed(KEY_R)) newRun(g);
            if (IsKeyPressed(KEY_ENTER)) openShop(g);
            if (IsKeyPressed(KEY_ESCAPE)) g.mode = Mode::Title, playMusic("ui/music.mp3", -8);
            break;
        case Mode::Victory:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) openShop(g);
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
        int land = int(std::count_if(m.field.begin(), m.field.end(), [&](uint8_t h) { return h > m.cut; }));
        std::vector<bool> seen(m.field.size());
        CHECK(m.at(m.spawn) && int(landmass(m.field, m.cut, cellIndex(m.spawn), seen, true).size()) == land);
        std::vector<bool> walk(m.field.size());
        landmass(m.field, m.cut, cellIndex(m.spawn), walk, false);
        CHECK(walk[cellIndex(m.portal)] && int(m.chests.size()) == cfg.chests);
        for (Vector2 c : m.chests) CHECK(walk[cellIndex(c)]);
    }

    // Guardian: enrages at half health, dies, purifies its portal and starts the end times.
    Game g;
    g.map.field.assign(FIELD_N * FIELD_N, 255);
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
    dg.map.field.assign(FIELD_N * FIELD_N, 255);
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
    pr.weapon = POISON_AURA;
    auto path = std::filesystem::temp_directory_path() / "convallaria_selftest" / "save.txt";
    CHECK(saveProfile(pr, path));
    Profile back = loadProfile(path);
    CHECK(back.coins == 750 && back.weapon == POISON_AURA && back.levels[PERM_MAX_HP] == 2 && back.levels[PERM_SPEED] == 0);
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
    playMusic("ui/music.mp3", -8);

    Game g;
    g.map = genMap(MAPS[0]);
    g.p.pos = g.map.spawn;

    while (!WindowShouldClose() && !g.quit) {
        float dt = std::min(GetFrameTime(), 1 / 30.f);
        if (IsMusicValid(A.music)) UpdateMusicStream(A.music);
        handleInput(g);
        if (g.mode == Mode::Play) step(g, dt);

        Camera2D cam{{GetScreenWidth() / 2.f, GetScreenHeight() / 2.f}, g.p.pos, 0, ZOOM};
        BeginDrawing();
        ClearBackground(g.map.water);
        ui.hot = -1;
        if (g.mode == Mode::Title) titleScreen(g);
        else if (g.mode == Mode::Shop) shopScreen(g);
        else {
            drawWorld(g, cam);
            drawHud(g);
            drawOverlay(g);
        }
        if (ui.hot >= 0 && ui.hot != ui.lastHot) A.hover.play(1, -10);
        ui.lastHot = ui.hot;
        EndDrawing();
    }
    if (g.mode != Mode::Title && g.mode != Mode::Shop) bankRun(g);
    CloseAudioDevice();
    CloseWindow();
}
