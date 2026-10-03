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
#include <queue>
#include <random>
#include <string>
#include <vector>

#include "data.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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

// A shipped build keeps assets/ beside the exe; a dev build falls back to the checkout.
const char* asset(const char* path) {
    static std::string dir = DirectoryExists(TextFormat("%sassets", GetApplicationDirectory())) ? TextFormat("%sassets", GetApplicationDirectory()) : ASSET_DIR;
    return TextFormat("%s/%s", dir.c_str(), path);
}
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
    Texture2D characters[CHARACTER_COUNT], enemySprites[ENEMY_TYPE_COUNT], bossSprites[BOSS_COUNT], ratKing, thorn, slime, ratman, guardian, portal, pickups, projectile, aura, title, chest, padRing, padKnob;
    Texture2D water, grass, soil, tree, titleBg, menuButton, weaponSlot, hud, weaponIcons[WEAPON_COUNT], itemIcons[ITEM_COUNT];
    Texture2D slash, pod, bramble;
    Sfx orb, levelup, hurt, wandShot, enemyShot, slimeHit, slimeDeath, ratmanDeath, win, hover, click;
    Sfx swish, whirl, thump, boom, zap, rustle, block, revive, freeze, unlock, croak, caw, dig, splash, roar, thud;
    Sfx enemyHit[ENEMY_TYPE_COUNT], enemyDie[ENEMY_TYPE_COUNT];  // sfx/<sprite>_hit / _die, for kinds with a sheet
    Music music{};
    std::string musicPath;
    float musicDb = 0;
} A;

void loadAssets() {
    for (int i = 0; i < CHARACTER_COUNT; i++) A.characters[i] = LoadTexture(asset(TextFormat("player/%s.png", CHARACTERS[i].sprite)));
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++)
        if (ENEMIES[i].sprite) {
            A.enemySprites[i] = LoadTexture(asset(TextFormat("enemies/%s.png", ENEMIES[i].sprite)));
            A.enemyHit[i].load(LoadSound(asset(TextFormat("sfx/%s_hit.wav", ENEMIES[i].sprite))));
            A.enemyDie[i].load(LoadSound(asset(TextFormat("sfx/%s_die.wav", ENEMIES[i].sprite))));
        }
    A.thorn = LoadTexture(asset("weapons/thorn/orb.png"));
    A.slime = LoadTexture(asset("enemies/blob.png"));
    A.ratman = LoadTexture(asset("enemies/ratman/ratman.png"));
    A.guardian = LoadTexture(asset("enemies/bob.png"));
    for (int i = 0; i < BOSS_COUNT; i++)
        if (BOSSES[i].sheet) A.bossSprites[i] = LoadTexture(asset(TextFormat("enemies/%s.png", BOSSES[i].sheet)));
    A.ratKing = LoadTexture(asset("enemies/rat_king.png"));
    A.portal = LoadTexture(asset("world/portal.png"));
    A.pickups = LoadTexture(asset("drops/pickups.png"));
    A.projectile = LoadTexture(asset("weapons/wand/projectile.png"));
    A.aura = LoadTexture(asset("weapons/poison/poison_radius..png"));
    A.chest = LoadTexture(asset("drops/chest/chest.png"));
    A.padRing = LoadTexture(asset("player/JoystickSplitted.png")), A.padKnob = LoadTexture(asset("player/SmallHandleFilled.png"));
    A.water = LoadTexture(asset("world/water.png"));
    A.grass = LoadTexture(asset("world/grass.png"));
    A.soil = LoadTexture(asset("world/soil.png"));
    A.tree = LoadTexture(asset("world/tree.png"));
    A.titleBg = LoadTexture(asset("ui/title_background.png"));
    A.menuButton = LoadTexture(asset("ui/menu_buttons.png"));
    A.weaponSlot = LoadTexture(asset("weapons/weaponslot.png"));
    A.hud = LoadTexture(asset("ui/hud.png"));
    const char* weaponIcon[WEAPON_COUNT] = {"wand/wand", "poison/poison", "thorn/thorn", "sword/sword", "axe/axe", "mortar/mortar", "lily/lily", "bramble/bramble"};
    for (int i = 0; i < WEAPON_COUNT; i++) A.weaponIcons[i] = LoadTexture(asset(TextFormat("weapons/%s.png", weaponIcon[i])));
    A.slash = LoadTexture(asset("weapons/sword/slash.png"));
    A.pod = LoadTexture(asset("weapons/mortar/pod.png"));
    A.bramble = LoadTexture(asset("weapons/bramble/patch.png"));
    Sfx* sfx[] = {&A.swish, &A.whirl, &A.thump, &A.boom, &A.zap, &A.rustle, &A.block, &A.revive, &A.freeze, &A.unlock, &A.croak, &A.caw, &A.dig, &A.splash, &A.roar, &A.thud};
    const char* sfxName[] = {"swish", "whirl", "thump", "boom", "zap", "rustle", "block", "revive", "freeze", "unlock", "croak", "caw", "dig", "splash", "roar", "thud"};
    for (int i = 0; i < int(std::size(sfx)); i++) sfx[i]->load(LoadSound(asset(TextFormat("sfx/%s.wav", sfxName[i]))));
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
    int character = 0;
    int levels[PERM_COUNT]{};
    int stats[STAT_COUNT]{};
    float volume[3] = {1, 1, 1};  // master, music, sfx
} profile;

const UnlockDef* unlockFor(UnlockKind kind, int index) {
    for (const UnlockDef& u : UNLOCKS)
        if (u.kind == kind && u.index == index) return &u;
    return nullptr;
}
bool unlocked(const Profile& pr, UnlockKind kind, int index) {
    const UnlockDef* u = unlockFor(kind, index);
    return !u || pr.stats[u->stat] >= u->need;
}
std::string unlockName(const UnlockDef& u) {
    return u.kind == UL_CHARACTER ? CHARACTERS[u.index].name : ITEMS[u.index].name;
}

// Adds one run's stats to the lifetime ones (best floor is a maximum); returns what that unlocked.
std::vector<std::string> addStats(Profile& pr, const int* run) {
    std::vector<bool> before;
    for (const UnlockDef& u : UNLOCKS) before.push_back(pr.stats[u.stat] >= u.need);
    for (int i = 0; i < STAT_COUNT; i++) pr.stats[i] = i == ST_FLOOR ? std::max(pr.stats[i], run[i]) : pr.stats[i] + run[i];
    std::vector<std::string> fresh;
    for (int i = 0; i < int(std::size(UNLOCKS)); i++)
        if (!before[i] && pr.stats[UNLOCKS[i].stat] >= UNLOCKS[i].need) fresh.push_back(unlockName(UNLOCKS[i]));
    return fresh;
}

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
        out << "coins " << pr.coins << "\ncharacter " << pr.character << "\n";
        out << "volume " << pr.volume[0] << " " << pr.volume[1] << " " << pr.volume[2] << "\n";
        for (int i = 0; i < PERM_COUNT; i++) out << "upgrade " << PERM_UPGRADES[i].id << " " << pr.levels[i] << "\n";
        for (int i = 0; i < STAT_COUNT; i++) out << "stat " << STAT_IDS[i] << " " << pr.stats[i] << "\n";
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
        else if (key == "character") { int c = 0; in >> c; pr.character = c >= 0 && c < CHARACTER_COUNT ? c : 0; }
        else if (key == "volume") {
            for (float& v : pr.volume) in >> v, v = std::clamp(v, 0.f, 1.f);
        } else if (key == "upgrade") {
            std::string id;
            int level = 0;
            in >> id >> level;
            for (int i = 0; i < PERM_COUNT; i++)
                if (id == PERM_UPGRADES[i].id) pr.levels[i] = std::clamp(level, 0, PERM_UPGRADES[i].maxLevel);
        } else if (key == "stat") {
            std::string id;
            int value = 0;
            in >> id >> value;
            for (int i = 0; i < STAT_COUNT; i++)
                if (id == STAT_IDS[i]) pr.stats[i] = std::max(0, value);
        }
        if (!in) break;
    }
    pr.coins = std::max(0, pr.coins);
    if (!unlocked(pr, UL_CHARACTER, pr.character)) pr.character = 0;
    return pr;
}

void save() {
    if (!saveProfile(profile, savePath())) TraceLog(LOG_WARNING, "Could not write save file %s", savePath().string().c_str());
#ifdef __EMSCRIPTEN__
    EM_ASM(FS.syncfs(false, function() {}););  // flush to IndexedDB
#endif
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
    std::vector<bool> open;  // tile corners enemies path through: walkable and clear of trunks
    Color water = WHITE, grassTint = WHITE, soilTint = WHITE;

    static int tileOf(float v) { return int(floorf(v / TILE)) + MAP_N / 2; }
    bool land(int x, int y) const { return x >= 0 && y >= 0 && x < MAP_N && y < MAP_N && field[y * MAP_N + x] > cut; }
    bool at(Vector2 p) const {
        int vx = int(roundf(p.x / TILE)) + MAP_N / 2, vy = int(roundf(p.y / TILE)) + MAP_N / 2;
        return land(vx - 1, vy - 1) && land(vx, vy - 1) && land(vx - 1, vy) && land(vx, vy);
    }
    bool blocked(Vector2 p, float pad = 0) const {  // by a tree trunk, grown by pad
        if (treeAt.empty()) return false;
        int r = pad > 0 ? 2 : 1;
        for (int y = tileOf(p.y) - r; y <= tileOf(p.y) + r; y++)
            for (int x = tileOf(p.x) - r; x <= tileOf(p.x) + r; x++) {
                int t = x >= 0 && y >= 0 && x < MAP_N && y < MAP_N ? treeAt[y * MAP_N + x] : -1;
                if (t >= 0 && fabsf(p.x - trees[t].x) < TREE_W + pad && fabsf(p.y - trees[t].y) < TREE_H + pad) return true;
            }
        return false;
    }
    // Something already inside a trunk or off the shore (spawned or pushed there) may walk out.
    bool walk(Vector2 from, Vector2 to) const { return (at(to) || !at(from)) && (!blocked(to) || blocked(from)); }
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
    m.open.assign(m.field.size(), false);
    for (int v : biggest) m.open[v] = !m.blocked(vertexPos(v), 5);
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
    float castT = 0, dashT = 0, transformT = 0, specialT = 0, specialWait = 0, orbitT = 0, freezeT = 0, trailT = 0;
    int variant = V_NONE, bossId = 0, charges = 0;  // bossId: BOSSES index; charges: boar charges left in a volley
    bool under = false;  // burrowed: unseen, untouchable, left out of the Grid
    float flank = 0;  // radians off the direct line while closing in, so packs surround
    float radius() const { return 6.f * scale; }
    bool boss() const { return kind != NORMAL; }
    float missing() const { return 1.f - hp / maxHp; }
};

enum ShotKind { SHOT_BOLT, SHOT_AXE };
struct Shot {
    Vector2 pos, dir;
    float speed, size, life;
    int damage, pierce, ricochet;
    bool fire, frost, crit;
    std::vector<uint32_t> hit;
    int kind = SHOT_BOLT;
    float t = 0;
};
struct EnemyShot { Vector2 pos, dir; int damage; float life, scale; Color color; bool web = false; };  // web: slows you
enum SeedType { SEED_EXP, SEED_MAGNET, SEED_SPEED, SEED_BOMB, SEED_GOLD, SEED_SILVER, SEED_HEAL };
struct Seed { Vector2 pos; int type, amount; float life, speed = 0, glimmer; bool magnetic = false; };
struct DamageNumber { Vector2 pos; int value; float t; };
struct Chest { Vector2 pos; bool free = false, open = false; };  // free: the guardian's reward
// Queued, lands next frame or after `wait` (drawn as a warning); hostile ones hit the player, fx ones are only seen.
struct Blast { Vector2 pos; float radius; int damage; float t; bool hostile = false; float wait = 0; bool fx = false; };
struct Slash { float angle, radius, t; bool flip; };  // around the player
struct Pod { Vector2 from, to; float t, time, radius; int damage; };  // a mortar seed in flight
struct Patch { Vector2 pos; float radius, life, tick; int damage; bool hostile; };  // brambles, or snail slime that slows you
struct Particle { Vector2 pos, vel; Color color; float t, life, size; };
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
    int level = 1, exp = 0, expNext = 15, kills = 0, silver = 0, pendingLevels = 0, rerolls = 1, chestsBought = 0;
    float dmgMul = 1, fireRateMul = 1, aoe = 1, expMul = 1, regen = 0, regenAcc = 0;
    float thorns = 0, evasion = 0, crit = 0, vampirism = 0, magnetScale = 1, coinMul = 1;
    float gold = 0;  // collected this run, banked into the profile when the run ends
    bool imbueFire = false, imbueFrost = false, moving = false;
    float magnetT = 0, magnetScan = 0, speedT = 0, iframes = 0, anim = 0;
    float armor = 0, critMul = 2, executioner = 0, staticChance = 0;
    float burnT = 0, burnDamage = 0, slowT = 0, shieldT = 0, sprayT = 4;
    int burnTicks = 0, pierce = 0, revives = 0;
    int stats[STAT_COUNT]{};  // this run, banked into the profile
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
    void build(const std::vector<Enemy>& es) {  // burrowed enemies are left out: nothing can find or touch them
        start.assign(W * W + 1, 0);
        cellOf.resize(es.size());
        for (size_t i = 0; i < es.size(); i++) {
            cellOf[i] = es[i].under ? -1 : cell(es[i].pos.y) * W + cell(es[i].pos.x);
            if (cellOf[i] >= 0) start[cellOf[i] + 1]++;
        }
        for (int c = 0; c < W * W; c++) start[c + 1] += start[c];
        items.resize(start[W * W]);
        fill.assign(start.begin(), start.end() - 1);
        for (size_t i = 0; i < es.size(); i++)
            if (cellOf[i] >= 0) items[fill[cellOf[i]]++] = int(i);
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

enum OptionKind { OPT_UPGRADE, OPT_BUFF };
struct Option { int kind, index, weapon; float value; int rarity; std::string title, desc; };
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
    std::vector<Particle> particles;
    std::vector<Slash> slashes;
    std::vector<Pod> pods;
    std::vector<Patch> patches;
    std::vector<std::string> unlocks;  // earned when this run was banked
    std::string toast;
    Color toastColor = WHITE;
    float toastT = 0;
    Grid grid;
    std::vector<uint16_t> flow;  // path cost from each tile corner to the player, 10 per straight step
    int flowAt = -1;             // the corner the flow was built from
    Mode mode = Mode::Title;
    uint32_t nextId = 1;
    int floor = 1;
    float runTime = 0;
    float spawnT = 1, spawnWait = 1, difficultyT = 5, deathT = 0, deathWait = DEATH_SLIME_BASE_INTERVAL;
    int spawnCount = 1, lastSecond = -1;
    bool endTimes = false, bossSummoned = false, bossDefeated = false, banked = false, quit = false;
    float finalBossT = -1, victoryT = -1;
    int runGold = 0, gotItem = -1;  // gotItem: shown by the item popup
    int optionsRoll = 0;  // bumped by every buildOptions, so the cards can replay their entrance
};

int chestCost(const Game& g) { return int(roundf((CHEST_BASE + CHEST_PER_FLOOR * (g.floor - 1)) * (1 + CHEST_GROWTH * g.p.chestsBought))); }

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
    for (Vector2 c : g.map.chests) g.chests.push_back({c});
    if (floor > 1) g.p.rerolls += 1 + g.p.items[IT_LURE];
    // Later floors open with the spawner already part-ramped instead of a quiet first minute.
    g.spawnWait = std::max(0.5f, 1 - 0.25f * (floor - 1)), g.spawnCount = floor;
    if (floor < MAX_FLOORS) g.portals.push_back({g.map.portal});
    else g.finalBossT = 2.5f;
    g.mode = Mode::Play;
    showToast(g, TextFormat("Floor %d  -  %s", floor, cfg->name), WHITE);
    playMusic("ui/music.mp3", -8);
}

void applyPerk(Player& p) {  // CHARACTERS perks
    switch (p.character) {
        case 0: p.expMul += 0.1f, p.rerolls++; break;
        case 1: p.speed *= 1.15f, p.magnetScale += 0.3f, p.maxHp *= 0.9f; break;
        case 2: p.maxHp *= 1.2f, p.regen += 1.5f, p.speed *= 0.92f; break;
        case 3: p.dmgMul += 0.15f, p.aoe += 0.1f, p.fireRateMul *= 1.1f; break;
        case 4: p.crit += 0.08f, p.critMul += 0.5f, p.maxHp *= 0.85f; break;
        case 5: p.aoe += 0.2f, p.armor += 0.1f, p.speed *= 0.9f; break;
        case 6: p.thorns += 0.25f, p.evasion += 0.1f, p.dmgMul -= 0.1f; break;
        case 7: p.coinMul += 0.25f, p.fireRateMul *= 0.9f, p.maxHp *= 0.9f; break;
    }
    p.hp = p.maxHp;
}

// A fresh character with the shop upgrades applied.
void newRun(Game& g) {
    Player p;
    p.character = profile.character;
    p.weapons.push_back({CHARACTERS[p.character].weapon});
    p.maxHp += permBoost(profile, PERM_MAX_HP);
    p.hp = p.maxHp;
    p.dmgMul += permBoost(profile, PERM_DAMAGE);
    p.speed += permBoost(profile, PERM_SPEED);
    p.regen += permBoost(profile, PERM_REGEN);
    p.thorns += permBoost(profile, PERM_ARMOR);
    p.evasion += permBoost(profile, PERM_EVASION);
    p.coinMul += permBoost(profile, PERM_GREED);
    p.expMul += permBoost(profile, PERM_EXP_GAIN);
    applyPerk(p);
    startFloor(g, 1, p, 0);
}

// Moves the gold collected this run into the saved profile, once per run.
void bankRun(Game& g) {
    if (g.banked) return;
    g.banked = true;
    g.runGold = int(g.p.gold);
    profile.coins += g.runGold;
    g.p.gold = 0;
    int run[STAT_COUNT];
    std::copy(std::begin(g.p.stats), std::end(g.p.stats), run);
    run[ST_KILLS] = g.p.kills, run[ST_FLOOR] = g.floor, run[ST_WINS] = g.mode == Mode::Victory, run[ST_GOLD] = g.runGold;
    g.unlocks = addStats(profile, run);
    if (!g.unlocks.empty()) A.unlock.play(1, -4);
    save();
}

float runMinutes(const Game& g) { return g.runTime / 60.f; }

void playAt(Sfx& s, const Game& g, Vector2 at, float pitch, float volumeDb, double throttle) {
    float fade = 1.f - Vector2Distance(at, g.p.pos) / 800.f;
    if (fade > 0) s.play(pitch, volumeDb + 20.f * log10f(fade), throttle);
}

// Purely visual bits flung from a point; capped so a horde dying at once stays cheap.
void burst(Game& g, Vector2 at, Color c, int n, float speed, float size) {
    for (int i = 0; i < n && g.particles.size() < 800; i++)
        g.particles.push_back({at, Vector2Rotate({rndr(0.3f, 1) * speed, 0}, rndr(0, 2 * PI)), c, 0, rndr(0.3f, 0.6f), size * rndr(0.7f, 1.3f)});
}

// ---------------------------------------------------------------- combat

void enrage(Game& g, Enemy& e) {
    e.phase = 2, e.enraged = true, e.transformT = 1.2f, e.castT = 0, e.dashT = 0;
    if (e.kind == GUARDIAN) {
        const FloorBoss& fb = FLOOR_BOSSES[std::min(g.floor, 3) - 1];
        const BossDef& bd = BOSSES[e.bossId];
        if (bd.sprite < 0) e.color = fb.enragedColor, e.glow = fb.glow, e.scale *= 1.3f;
        else e.color = bd.enraged, e.glow = bd.glow, e.scale *= 1.15f;
        e.speed *= fb.speedMult;
        e.damage = int(e.damage * fb.damageMult);
        e.specialWait = fb.specialWait;
    } else {
        e.color = rgb(1, 0.55f, 0.55f), e.glow = rgb(1, 0.25f, 0.15f);
        e.speed *= 1.3f;
        e.damage = int(e.damage * 1.3f);
        e.scale *= 1.25f;
    }
}

void bossShot(Game& g, const Enemy& e, Vector2 dir, int damage, float scale, Color color);
Enemy enemyOf(Game& g, int type, Vector2 pos);

// The burst every enemy dies with, plus each kind's parting shot.
void enemyDeath(Game& g, const Enemy& e) {
    const EnemyDef& d = ENEMIES[e.type];
    Color gib = e.boss() || d.deathSlime ? e.color : e.ratman ? ENEMIES[RATMAN].gib : d.gib;
    bool rubble = d.death == DIE_CRUMBLE && !e.ratman;
    burst(g, e.pos, gib, int((rubble ? 14 : 8) * std::min(e.scale, 3.f)), 90 * std::min(e.scale, 2.f), rubble ? 3 : 2);
    if (e.boss()) return;
    if (e.variant == V_BLAZING) {  // bursts into flame a moment later
        g.blasts.push_back({e.pos, 40 * e.scale, std::max(1, e.damage / 3), 0, true, 0.45f});
        burst(g, e.pos, rgb(1, .5f, .1f), 12, 70, 2);
    }
    if (e.ratman) return;
    if (d.death == DIE_SPORES)  // shrooms pop into a short-lived ring of spores
        for (int k = 0; k < 4; k++) {
            bossShot(g, e, Vector2Rotate({1, 0}, PI / 4 + k * PI / 2), std::max(1, e.damage / 3), 0.7f, rgb(1, .8f, .3f));
            g.enemyShots.back().life = 0.8f;
        }
    if (d.death == DIE_SPLIT)  // spiders burst into spiderlings
        for (int k = 0; k < 3; k++) g.pending.push_back(enemyOf(g, SPIDERLING, Vector2Add(e.pos, Vector2Rotate({8, 0}, k * 2 * PI / 3))));
}

// Each kind's own voice (tools/sounds.py), deeper for giants and bosses; slimes keep the Godot squelch.
Sfx& voice(const Enemy& e, bool death) {
    Sfx& s = death ? A.enemyDie[e.type] : A.enemyHit[e.type];
    return s.base.frameCount && !(e.kind == GUARDIAN && BOSSES[e.bossId].sprite < 0) ? s : death ? A.slimeDeath : A.slimeHit;
}
float voicePitch(const Enemy& e) {
    return A.enemyHit[e.type].base.frameCount ? rndr(0.92f, 1.08f) * sqrtf(ENEMIES[e.type].scale / e.scale) : e.pitch;
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
        enemyDeath(g, e);
        if (e.ratman) playAt(A.ratmanDeath, g, e.pos, e.pitch * rndr(0.9f, 1.1f), e.boss() ? -10 : -15, 0.05);
        else playAt(voice(e, true), g, e.pos, voicePitch(e), e.boss() ? 0 : -10, 0.05);
        if (e.boss()) stopMusic();
    } else if (e.hurtT <= 0 && e.castT <= 0 && e.transformT <= 0 && e.dashT <= 0) {
        e.hurtT = e.ratman || e.boss() ? 0.15f : 0.25f;
        if (!e.ratman && !e.boss()) e.vel = Vector2Scale(Vector2Normalize(Vector2Subtract(e.pos, g.p.pos)), 140 / e.scale);  // knockback
        if (!e.ratman) playAt(voice(e, false), g, e.pos, voicePitch(e), -5, 0.05);
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

// A Golden Goldfish first, then Second Wind.
bool revive(Game& g) {
    Player& p = g.p;
    if (p.items[IT_GOLDFISH] > 0) p.items[IT_GOLDFISH]--;
    else if (p.revives > 0) p.revives--;
    else return false;
    p.hp = p.maxHp / 2, p.iframes = 2, p.burnTicks = 0;
    burst(g, p.pos, rgb(1, .85f, .3f), 24, 120, 2);
    A.revive.play(1, -4);
    showToast(g, "Revived!", rgb(1, .85f, .3f));
    return true;
}

void playerDown(Game& g) {
    if (g.p.hp <= 0 && !revive(g)) g.mode = Mode::GameOver, bankRun(g);
}

void hurtPlayer(Game& g, int damage, Enemy* source) {
    Player& p = g.p;
    if (p.iframes > 0) return;
    if (rnd() < std::min(p.evasion, 0.6f) || rnd() < hyper(0.15f, p.items[IT_CHARM])) return;  // dodge caps at 60%
    if (p.items[IT_PUMPKIN] && p.shieldT <= 0) {
        p.shieldT = 12 / (1 + 0.25f * (p.items[IT_PUMPKIN] - 1)), p.iframes = 0.5f;
        burst(g, p.pos, rgb(1, .6f, .15f), 10, 80, 2);
        A.block.play(1, -6);
        return;
    }
    damage = std::max(1, int(damage * (1 - std::min(p.armor, 0.6f)) * (1 - hyper(0.1f, p.items[IT_STRAW]))));
    p.hp -= damage;
    A.hurt.play(rndr(1.4f, 1.8f), -5);
    if (source) {
        if (source->variant == V_BLAZING) p.burnTicks = 3, p.burnT = 0.5f, p.burnDamage = std::max(1.f, damage * 0.15f);
        if (source->variant == V_FROST) p.slowT = 2;
        if (p.thorns > 0) hurtEnemy(g, *source, int(damage * p.thorns));
    }
    if (p.hp > 0) p.iframes = 0.5f;
    else playerDown(g);
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
        p.maxHp += LEVEL_HP;
        p.dmgMul += LEVEL_DAMAGE;
        p.hp = std::min(p.maxHp, p.hp + p.maxHp * 0.08f * p.items[IT_APPLE]);
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

int rollItem() {  // locked items never drop
    int roll = rndi(100), tier = 0;
    for (int acc = 0; tier < 2 && roll >= (acc += ITEM_TIERS[tier].weight); tier++) {}
    std::vector<int> pool;
    for (int i = 0; i < ITEM_COUNT; i++)
        if (ITEMS[i].tier == tier && unlocked(profile, UL_ITEM, i)) pool.push_back(i);
    return pool[rndi(int(pool.size()))];
}

void grantItem(Game& g, int id) {
    Player& p = g.p;
    p.items[id]++;
    switch (id) {  // flat stat items apply once; the rest read their stack count where they act
        case IT_BARK: p.maxHp += 30, p.hp += 30; break;
        case IT_WHEAT: p.expMul += 0.1f; break;
        case IT_BEANIE: p.magnetScale += 0.2f; break;
        case IT_CAN: p.aoe += 0.08f; break;
        case IT_HOE: p.critMul += 0.5f; break;
        case IT_LURE: p.rerolls++; break;
        case IT_KOI: p.coinMul += 0.5f; break;
    }
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
    if (!near.empty()) playAt(A.zap, g, from.pos, rndr(1.1f, 1.4f), -14, 0.08);
}

// A weapon hit: damage plus every on-hit effect. Effects call hurtEnemy
// directly so they can't proc each other.
void onHit(Game& g, Enemy& e, int damage, bool fire, bool frost) {
    const Player& p = g.p;
    if (p.executioner > 0 && e.hp < e.maxHp / 2) damage = int(damage * (1 + p.executioner));
    hurtEnemy(g, e, damage);
    if (fire || rnd() < 0.15f * p.items[IT_EMBER]) applyBurn(e, damage * 0.2f * (1 + 0.5f * p.items[IT_KINDLING]));
    if (frost) applySlow(e, 0.5f);
    if (!e.dying && !e.boss() && rnd() < hyper(0.08f, p.items[IT_SAPPHIRE])) e.freezeT = 1.2f, playAt(A.freeze, g, e.pos, rndr(0.9f, 1.1f), -8, 0.08);
    if (p.items[IT_LEECH]) heal(g.p, float(p.items[IT_LEECH]));
    if (!e.dying && !e.boss() && e.hp < e.maxHp * hyper(0.15f, p.items[IT_SICKLE])) hurtEnemy(g, e, int(ceilf(e.hp)));
    if (p.items[IT_BELL] && rnd() < 0.2f) zap(g, e, std::max(1, int(damage * 0.6f)), 1 + 2 * p.items[IT_BELL]);
    if (p.staticChance > 0 && rnd() < p.staticChance) zap(g, e, std::max(1, int(damage * 0.5f)), 3);
}

int critRoll(const Player& p, int damage, bool* crit = nullptr) {
    bool c = rnd() <= 0.05f + p.crit + 0.08f * p.items[IT_CLOVER];
    if (crit) *crit = c;
    return c ? int(damage * p.critMul) : damage;
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

float eliteChance(const Game& g) { return std::min(ELITE_MAX, ELITE_BASE + ELITE_PER_FLOOR * (g.floor - 1) + ELITE_PER_MIN * runMinutes(g)); }

void makeElite(Enemy& e, int v) {
    const VariantDef& vd = VARIANTS[v];
    e.variant = v;
    e.hp = e.maxHp = float(int(e.hp * vd.hp));
    e.damage = int(e.damage * vd.damage);
    e.speed = std::min(e.speed * vd.speed, ENEMY_SPEED_CAP);
    e.scale *= vd.scale;
    e.exp = int(e.exp * vd.exp);
}

// A regular enemy of `type` scaled to the floor and run time; may roll an elite variant.
Enemy enemyOf(Game& g, int type, Vector2 pos) {
    const EnemyDef& d = ENEMIES[type];
    float floorMult = 1 + (g.floor - 1) * 0.3f;
    Enemy e = makeEnemy(g, pos);
    if (d.deathSlime) {
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
        e.shooter = d.attack == ATK_SHOOT;
        e.ratman = d.ratman;
        if (e.ratman) {
            float v = rnd();
            if (v > 0.75f) e.scale *= 1.25f, e.hp = float(int(e.hp * 1.5f)), e.speed *= 0.75f, e.color = rgb(0.7f, 0.6f, 0.6f);
            else if (v < 0.25f) e.scale *= 0.8f, e.hp = float(int(e.hp * 0.6f)), e.speed *= 1.4f, e.color = rgb(1.2f, 1.1f, 1.1f);
        }
        e.speed = std::min(e.speed, ENEMY_SPEED_CAP);
        e.under = d.attack == ATK_BURROW;
    }
    e.type = type;
    e.maxHp = e.hp;
    if (!d.deathSlime && type != SPIDERLING && rnd() < eliteChance(g)) makeElite(e, 1 + rndi(VARIANT_COUNT - 1));
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
    g.enemies.push_back(enemyOf(g, type, pos));
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

void summonGuardian(Game& g, int portal, int variant = -1) {  // variant: BOSSES index, random if negative
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
    e.bossId = variant >= 0 ? variant : rndi(BOSS_COUNT);
    const BossDef& bd = BOSSES[e.bossId];
    float m = runMinutes(g);
    int f = g.floor;
    e.kind = GUARDIAN;
    e.portal = portal;
    e.hp = float(int(int(BOSS_HEALTH * (1 + m * 0.2f)) * (1 + (f - 1) * 2.5f + m * 0.25f) * bd.hp));
    e.maxHp = e.hp;
    e.damage = int(BOSS_DAMAGE * (1 + (f - 1) * ENEMY_DMG_PER_FLOOR) * (1 + m * 0.05f));
    e.speed = (BOSS_SPEED + (f - 1) * 20) * bd.speed;
    e.scale = bd.scale;
    e.color = bd.sprite < 0 ? fb.color : WHITE;
    if (bd.sprite >= 0) e.type = bd.sprite;
    e.pitch = 0.3f;
    e.specialWait = e.specialT = 6;
    g.enemies.push_back(e);
    playMusic(fb.music, -10);
    A.roar.play(1, -4);
    showToast(g, bd.name, rgb(1, .5f, .4f));
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

void minion(Game& g, const Enemy& boss, int type, float spread) {
    g.pending.push_back(enemyOf(g, type, Vector2Add(boss.pos, {rndr(-spread, spread), rndr(-spread, spread)})));
}

// Each guardian's special: bullets, minions, and its own signature.
void guardianSpecial(Game& g, Enemy& e, Vector2 dir) {
    bool rage = e.enraged;
    switch (BOSSES[e.bossId].pattern) {
        case BP_GUARDIAN: {
            int n = rage ? 8 : 4;
            for (int k = 0; k < n; k++)
                bossShot(g, e, Vector2Rotate({1, 0}, k * 2 * PI / n), int(e.damage * 0.4f), rage ? 1.2f : 1, e.color);
            for (int k = 0; k < (rage ? 2 : 1); k++) {
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
            break;
        }
        case BP_HIVE: {  // a ring of stingers and a cloud of bees
            int n = rage ? 20 : 12;
            float off = rndr(0, 2 * PI);
            for (int k = 0; k < n; k++) bossShot(g, e, Vector2Rotate({1, 0}, off + k * 2 * PI / n), int(e.damage * 0.35f), 0.9f, rgb(1, .8f, .2f));
            for (int k = 0; k < (rage ? 8 : 5); k++) minion(g, e, SWARM, 60);
            break;
        }
        case BP_COLOSSUS:  // a slam around itself and boulders falling around you, all telegraphed
            g.blasts.push_back({e.pos, 100.f * (rage ? 1.3f : 1), int(e.damage * 0.6f), 0, true, 0.6f});
            for (int k = 0; k < (rage ? 8 : 5); k++)
                g.blasts.push_back({Vector2Add(g.p.pos, Vector2Rotate({rndr(0, 110), 0}, rndr(0, 2 * PI))), 34, int(e.damage * 0.5f), 0, true, 0.9f + k * 0.12f});
            break;
        case BP_BROOD:  // spiderlings and a fan of webs that slow you
            for (int k = 0; k < (rage ? 7 : 4); k++) minion(g, e, SPIDERLING, 80);
            for (int k = -(1 + rage); k <= 1 + rage; k++) {
                bossShot(g, e, Vector2Rotate(dir, k * 0.25f), int(e.damage * 0.3f), 1.1f, rgb(.92f, .92f, .96f));
                g.enemyShots.back().web = true;
            }
            break;
        case BP_BOAR:  // a volley of charges, each wound up where it stopped
            e.charges = rage ? 3 : 2, e.dashT = 0.55f, e.dashDir = dir;
            break;
    }
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
            if ((e.castT -= dt) <= 0) {
                if (e.charges > 0) e.charges--, e.dashT = 0.55f, e.dashDir = dir;
                else guardianSpecial(g, e, dir);
            }
        } else if (e.dashT > 0) {
            e.dashT -= dt;
            e.vel = Vector2Scale(e.dashDir, e.speed * (BOSSES[e.bossId].pattern == BP_BOAR ? 5.5f : 4));
            if (e.dashT <= 0 && e.charges > 0) e.castT = 0.3f;
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

const char* BUFF_TEXT[] = {"+%s%% Damage", "+%s%% Attack Speed", "+%s%% Size", "+%s Bounce", "+%s Projectile"};
const char* SIZE_TEXT[WEAPON_COUNT] = {"+%s%% Splash Size", "+%s%% Aura Radius", "+%s%% Orbit Size", "+%s%% Slash Reach",
                                       "+%s%% Axe Size", "+%s%% Blast Radius", "+%s%% Jump Range", "+%s%% Patch Size"};
const char* COUNT_TEXT[WEAPON_COUNT] = {"+%s Projectile", "", "+%s Orb", "+%s Slash", "+%s Axe", "+%s Seed Pod", "+%s Lightning Jump", "+%s Patch"};
const float BUFF_VALUE[] = {0.12f, 0.10f, 0.12f, 1, 1};
const int BUFF_WEIGHT[] = {6, 6, 4, 2, 2};

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

// Three distinct picks drawn by weight: stat upgrades and buffs for the weapon you hold.
void buildOptions(Game& g) {
    Player& p = g.p;
    std::vector<std::pair<Option, int>> pool;  // (option, weight); value, rarity and text are filled in once picked
    for (int i = 0; i < int(std::size(UPGRADES)); i++)
        if (!(UPGRADES[i].unique && hasUnique(p, UPGRADES[i].id))) pool.push_back({{OPT_UPGRADE, i, 0}, UPGRADES[i].weight});
    for (const Weapon& w : p.weapons) {
        std::vector<int> buffs = {B_DAMAGE, B_FIRE_RATE, B_SIZE};
        if (w.id == WAND) buffs.push_back(B_RICOCHET);
        if (w.id != POISON_AURA) buffs.push_back(B_PROJECTILE);
        for (int b : buffs) pool.push_back({{OPT_BUFF, b, w.id}, BUFF_WEIGHT[b]});
    }

    g.options.clear();
    g.optionsRoll++;
    while (g.options.size() < 3 && !pool.empty()) {
        int total = 0, k = 0;
        for (auto& [o, wt] : pool) total += wt;
        for (int roll = rndi(total); roll >= pool[k].second; k++) roll -= pool[k].second;
        Option o = pool[k].first;
        pool.erase(pool.begin() + k);
        o.rarity = rollRarity();
        if (o.kind == OPT_UPGRADE) {
            const Upgrade& u = UPGRADES[o.index];
            if (u.unique || o.index == MULTI_ATTACK || o.index == PIERCE) o.rarity = 0;
            o.value = u.base * RARITIES[o.rarity].mult;
            o.title = u.name, o.desc = fillText(u.text, o.value);
        } else {
            const Weapon& w = *findWeapon(p, o.weapon);
            bool whole = o.index >= B_RICOCHET;  // bounces and projectiles: +1, +2 only when legendary
            o.value = whole ? 1 + (o.rarity == 3) : BUFF_VALUE[o.index] * RARITIES[o.rarity].mult;
            const char* text = o.index == B_SIZE ? SIZE_TEXT[w.id] : o.index == B_PROJECTILE ? COUNT_TEXT[w.id] : BUFF_TEXT[o.index];
            o.title = std::string(WEAPON_NAMES[w.id]) + " Lv." + std::to_string(w.level + 1);
            o.desc = fillText(text, whole ? o.value : o.value * 100);
        }
        g.options.push_back(o);
    }
}

// Card label and colour: rarity, or the fixed look of uniques.
const char* optionLabel(const Option& o) {
    if (o.kind == OPT_UPGRADE && UPGRADES[o.index].unique) return "UNIQUE";
    return RARITIES[o.rarity].name;
}
Color optionColor(const Option& o) {
    if (o.kind == OPT_UPGRADE && UPGRADES[o.index].unique) return UNIQUE_COLOR;
    return RARITIES[o.rarity].color;
}

void applyOption(Game& g, const Option& o) {
    Player& p = g.p;
    float v = o.value, pct = v / 100.f;
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
    auto fasterFire = [&](float d) { p.fireRateMul /= 1 + d; };  // cooldown multiplier: stacks with diminishing returns
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
    else if (id == "multi_attack") for (Weapon& w : p.weapons) w.projectile += 1;
    else if (id == "glass_cannon") { p.dmgMul += pct; p.maxHp -= p.maxHp * 0.2f; p.hp = std::min(p.hp, p.maxHp); }
    else if (id == "heavy_armor") { float inc = p.maxHp * 0.1f; p.thorns += pct; p.speed -= p.speed * 0.15f; p.maxHp += inc; p.hp += inc; }
    else if (id == "berserker") { fasterFire(pct); p.evasion = std::max(0.f, p.evasion - 0.1f); }
    else if (id == "vampiric_edge") p.vampirism += pct;
    else if (id == "magnet_training") p.magnetScale += pct;
    else if (id == "precision") { p.crit += pct; p.dmgMul += pct; }
    else if (id == "momentum") { p.speed += p.speed * pct; fasterFire(pct); }
    else if (id == "soul_harvest") { p.expMul += pct; p.regen += 1; }
    else if (id == "pierce") p.pierce += 1;
    else if (id == "armor") p.armor += pct;  // capped at 60% when hit
    else if (id == "greed") p.coinMul += pct;
    else if (id == "second_wind") p.revives++;
    else if (id == "overgrowth") { p.aoe += pct; p.fireRateMul *= 1.1f; }
    else if (id == "static_charge") p.staticChance += pct;
    else if (id == "executioner") p.executioner += pct;
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
        if (!g.chests[i].open && Vector2Distance(g.chests[i].pos, g.p.pos) < PORTAL_RANGE) return i;
    return -1;
}

void openChest(Game& g, int i) {
    Chest& c = g.chests[i];
    int cost = c.free ? 0 : chestCost(g);
    if (g.p.silver < cost) {
        showToast(g, TextFormat("Need %d silver", cost), GRAY);
        return;
    }
    g.p.silver -= cost;
    if (!c.free) g.p.chestsBought++;
    g.p.stats[ST_CHESTS]++;
    c.open = true;
    burst(g, c.pos, c.free ? rgb(1, .85f, .3f) : rgb(.85f, .85f, .9f), 12, 70, 2);
    grantItem(g, rollItem());
}

// The E key: open a chest, or summon a guardian at / step through a portal.
void interact(Game& g) {
    int c = nearChest(g), i = nearPortal(g);
    if (c >= 0) openChest(g, c);
    else if (i >= 0 && g.portals[i].state == Portal::CORRUPTED) summonGuardian(g, i);
    else if (i >= 0) startFloor(g, g.floor + 1, g.p, g.runTime);
}

Vector2 touchMove{};  // the on-screen joystick, set by touchControls

void updatePlayer(Game& g, float dt) {
    Player& p = g.p;
    p.time += dt;
    g.runTime += dt;
    Vector2 in = {float(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - float(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)),
                  float(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) - float(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))};
    in = Vector2Add(in, touchMove);
    if (Vector2Length(in) > 1) in = Vector2Normalize(in);
    p.moving = in.x != 0 || in.y != 0;
    if (p.moving) {
        p.facing = fabsf(in.x) > fabsf(in.y) ? (in.x > 0 ? RIGHT : LEFT) : (in.y > 0 ? DOWN : UP);
        p.anim += dt;
        float speed = p.speed * (1 + 0.1f * p.items[IT_BOOTS]) * (p.speedT > 0 ? 1.5f : 1) * (p.slowT > 0 ? 0.6f : 1);
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
    p.slowT = std::max(0.f, p.slowT - dt);
    p.shieldT -= dt;
    if (p.burnTicks > 0 && (p.burnT -= dt) <= 0) {
        p.burnTicks--, p.burnT = 0.5f, p.hp -= p.burnDamage;
        burst(g, p.pos, rgb(1, .5f, .1f), 3, 40, 2);
        playerDown(g);
    }
    if (p.items[IT_SPRINKLER] && (p.sprayT -= dt) <= 0) {  // knocks back, slows and hurts everything close
        p.sprayT = 4;
        float r = 90 * p.aoe;
        int damage = int(30 * p.items[IT_SPRINKLER] * p.dmgMul);
        g.grid.query(p.pos, r + 120, [&](int i) {
            Enemy& e = g.enemies[i];
            if (e.dying || Vector2Distance(e.pos, p.pos) > r + e.radius()) return false;
            hurtEnemy(g, e, damage);
            applySlow(e, 0.5f);
            return false;
        });
        for (int k = 0; k < 28 && g.particles.size() < 800; k++)
            g.particles.push_back({p.pos, Vector2Rotate({r * 2.2f, 0}, k * 2 * PI / 28), rgb(.5f, .8f, 1), 0, 0.45f, 2.5f});
        A.splash.play(rndr(0.95f, 1.05f), -8);
    }

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
constexpr Vector2 FACING_DIR[] = {{0, 1}, {-1, 0}, {1, 0}, {0, -1}};

Enemy* nearestEnemy(Game& g, Vector2 from, float range) {
    Enemy* best = nullptr;
    g.grid.query(from, range, [&](int i) {
        Enemy& e = g.enemies[i];
        float d = Vector2Distance(e.pos, from);
        if (!e.dying && d < range) range = d, best = &e;
        return false;
    });
    return best;
}

Enemy* randomEnemy(Game& g, Vector2 from, float range) {
    std::vector<int> near;
    g.grid.query(from, range, [&](int i) {
        if (!g.enemies[i].dying && Vector2Distance(g.enemies[i].pos, from) < range) near.push_back(i);
        return false;
    });
    return near.empty() ? nullptr : &g.enemies[near[rndi(int(near.size()))]];
}
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
                burst(g, at, rgb(.6f, .95f, .4f), 3, 50, 2);
                onHit(g, e, damage, p.imbueFire, p.imbueFrost);
                return false;
            });
        }
        return;
    }

    if ((w.timer += dt) < weaponWait(p, w)) return;
    w.timer = 0;

    int count = orbCount(p, w);  // projectiles, slashes, axes, pods, jumps or patches
    float area = lv.scale * w.size * p.aoe;
    bool fire = p.imbueFire, frost = p.imbueFrost;
    Enemy* near = w.id == WAND || w.id == POISON_AURA ? nullptr : nearestEnemy(g, p.pos, w.id == SWORD ? 160 : 400);

    if (w.id == SWORD) {  // slashes fan out around you, the first toward the nearest enemy
        Vector2 dir = near ? Vector2Normalize(Vector2Subtract(near->pos, p.pos)) : FACING_DIR[p.facing];
        float reach = 54 * area;
        for (int k = 0; k < count; k++) {
            Vector2 d = Vector2Rotate(dir, k * 2 * PI / count);
            g.slashes.push_back({atan2f(d.y, d.x), reach, 0, k % 2 == 1});
            g.grid.query(p.pos, reach + 20, [&](int i) {
                Enemy& e = g.enemies[i];
                Vector2 to = Vector2Subtract(e.pos, p.pos);
                float dist = Vector2Length(to);
                if (e.dying || dist > reach + e.radius() || (dist > 8 && Vector2DotProduct(to, d) < dist * 0.34f)) return false;  // a ~140 degree arc
                onHit(g, e, critRoll(p, damage), fire, frost);
                return false;
            });
        }
        A.swish.play(rndr(0.9f, 1.1f), -8);
        return;
    }
    if (w.id == AXE) {
        Vector2 dir = near ? Vector2Normalize(Vector2Subtract(near->pos, p.pos)) : FACING_DIR[p.facing];
        for (int k = 0; k < count; k++) {
            Shot s{p.pos, Vector2Rotate(dir, (k - (count - 1) / 2.f) * 25 * DEG2RAD), lv.speed, area, 5, damage, 0, 0, fire, frost, false, {}};
            s.kind = SHOT_AXE;
            g.shots.push_back(s);
        }
        A.whirl.play(rndr(0.9f, 1.1f), -9);
        return;
    }
    if (w.id == MORTAR) {
        for (int k = 0; k < count; k++) {
            Enemy* t = randomEnemy(g, p.pos, 320);
            Vector2 to = t ? t->pos : Vector2Add(p.pos, Vector2Rotate({rndr(40, 160), 0}, rndr(0, 2 * PI)));
            g.pods.push_back({p.pos, to, 0, 0.7f + 0.08f * k, 40 * area, damage});
        }
        A.thump.play(rndr(0.9f, 1.1f), -6);
        return;
    }
    if (w.id == LILY) {  // strikes the nearest enemy, then jumps to the closest one not yet struck
        Enemy* e = near;
        Vector2 from = e ? Vector2{e->pos.x, e->pos.y - 90} : Vector2{};
        std::vector<uint32_t> struck;
        for (int k = 0; k <= count && e; k++) {
            g.bolts.push_back({from, e->pos, 0});
            struck.push_back(e->id);
            from = e->pos;
            Enemy* next = nullptr;
            float best = 130 * w.size * p.aoe;
            g.grid.query(from, best, [&](int i) {
                Enemy& o = g.enemies[i];
                float d = Vector2Distance(o.pos, from);
                if (!o.dying && d < best && std::find(struck.begin(), struck.end(), o.id) == struck.end()) best = d, next = &o;
                return false;
            });
            onHit(g, *e, critRoll(p, damage), fire, frost);
            e = next;
        }
        if (near) A.zap.play(rndr(0.8f, 1), -7);
        return;
    }
    if (w.id == BRAMBLE) {
        for (int k = 0; k < count; k++) {
            Enemy* t = randomEnemy(g, p.pos, 250);
            g.patches.push_back({t ? t->pos : p.pos, 34 * area, 3.5f, 0, damage, false});
        }
        A.rustle.play(rndr(0.9f, 1.1f), -8);
        return;
    }

    if (w.id == POISON_AURA) {
        w.pulse = 0.3f;
        float radius = 48 * lv.scale * p.aoe * w.size;
        for (int i = 0; i < 10 && g.particles.size() < 800; i++)  // bubbles rising out of the pool
            g.particles.push_back({Vector2Add(p.pos, Vector2Rotate({radius * sqrtf(rnd()), 0}, rndr(0, 2 * PI))), {rndr(-6, 6), rndr(-40, -20)},
                                   rgb(.94f, .54f, .85f), 0, rndr(0.4f, 0.7f), rndr(1.5f, 3)});
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
        if (!e.dying && !e.under && d < best) best = d, target = &e;
    }
    if (!target) return;
    Vector2 dir = Vector2Normalize(Vector2Subtract(target->pos, p.pos));
    A.wandShot.play(1, -10, 0.05);
    burst(g, Vector2Add(p.pos, Vector2Scale(dir, 10)), rgb(1, .9f, .6f), 5, 70, 2);  // muzzle flash
    for (int i = 0; i < count; i++) {
        bool crit;
        int hit = critRoll(p, damage, &crit);
        Vector2 d = Vector2Rotate(dir, (i - (count - 1) / 2.f) * 15 * DEG2RAD);
        g.shots.push_back({p.pos, d, lv.speed, crit ? 1.5f * w.size : w.size, 3, hit,
                           w.pierce + p.pierce + p.items[IT_CORN], w.ricochet, fire, frost, crit, {}});
    }
}

// An axe flies out, slows to a stop, then comes home; it cuts each enemy once per way.
bool updateAxe(Game& g, Shot& s, float dt) {
    const float out = 0.7f;
    s.t += dt;
    if (s.t < out) s.pos = Vector2Add(s.pos, Vector2Scale(s.dir, s.speed * 1.6f * (1 - s.t / out) * dt));
    else {
        if (s.t - dt < out) s.hit.clear();
        s.pos = Vector2MoveTowards(s.pos, g.p.pos, s.speed * 1.5f * std::min(1.f, (s.t - out) * 2.5f) * dt);
        if (Vector2Distance(s.pos, g.p.pos) < 10) return false;
    }
    float r = 10 * s.size;
    g.grid.query(s.pos, r + 120, [&](int i) {
        Enemy& e = g.enemies[i];
        if (e.dying || Vector2Distance(e.pos, s.pos) > r + e.radius() || std::find(s.hit.begin(), s.hit.end(), e.id) != s.hit.end()) return false;
        s.hit.push_back(e.id);
        onHit(g, e, critRoll(g.p, s.damage), s.fire, s.frost);
        return false;
    });
    return true;
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
        if (s.kind == SHOT_AXE) {
            if (!updateAxe(g, s, dt)) s.life = 0;
            continue;
        }
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
        burst(g, s.pos, s.crit ? rgb(1, .8f, .1f) : rgb(1, .95f, .75f), 5, 80, 2);
        explode(g, s);
        if (s.pierce > 0) { s.pierce--; continue; }
        if (s.ricochet > 0) {
            s.ricochet--;
            const Enemy* next = nullptr;
            float best = 400 * s.size;
            for (const Enemy& e : g.enemies) {
                float d = Vector2Distance(e.pos, s.pos);
                if (!e.dying && !e.under && d < best && std::find(s.hit.begin(), s.hit.end(), e.id) == s.hit.end()) best = d, next = &e;
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
            if (s.web) g.p.slowT = 2;
            hurtPlayer(g, s.damage, nullptr);
            s.life = 0;
        }
    }
    std::erase_if(g.enemyShots, [](const EnemyShot& s) { return s.life <= 0; });

    for (Pod& q : g.pods) {
        if ((q.t += dt) < q.time) continue;
        g.grid.query(q.to, q.radius + 120, [&](int i) {
            Enemy& e = g.enemies[i];
            if (!e.dying && Vector2Distance(e.pos, q.to) <= q.radius + e.radius()) onHit(g, e, critRoll(g.p, q.damage), g.p.imbueFire, g.p.imbueFrost);
            return false;
        });
        g.blasts.push_back({q.to, q.radius, 0, 0, false, 0, true});
        burst(g, q.to, rgb(.75f, .9f, .3f), 10, 90, 2);
        playAt(A.boom, g, q.to, rndr(0.9f, 1.1f), -9, 0.05);
    }
    std::erase_if(g.pods, [](const Pod& q) { return q.t >= q.time; });

    for (Patch& q : g.patches) {
        q.life -= dt;
        if (q.hostile) {  // snail slime
            if (Vector2Distance(q.pos, g.p.pos) < q.radius + PLAYER_RADIUS / 2) g.p.slowT = std::max(g.p.slowT, 0.2f);
            continue;
        }
        if ((q.tick -= dt) > 0) continue;
        q.tick = 0.4f;
        g.grid.query(q.pos, q.radius + 120, [&](int i) {
            Enemy& e = g.enemies[i];
            if (e.dying || Vector2Distance(e.pos, q.pos) > q.radius + e.radius()) return false;
            hurtEnemy(g, e, q.damage);
            applySlow(e, 0.6f);
            return false;
        });
    }
    std::erase_if(g.patches, [](const Patch& q) { return q.life <= 0; });
    for (Slash& s : g.slashes) s.t += dt;
    std::erase_if(g.slashes, [](const Slash& s) { return s.t > 0.2f; });

    for (Blast& b : g.blasts) {
        if (b.wait > 0) {
            if ((b.wait -= dt) <= 0) playAt(A.thud, g, b.pos, rndr(0.8f, 1), -8, 0.05);
            continue;
        }
        if (b.fx) {
        } else if (b.t == 0 && b.hostile) {
            if (Vector2Distance(g.p.pos, b.pos) <= b.radius + PLAYER_RADIUS) hurtPlayer(g, b.damage, nullptr);
        } else if (b.t == 0)
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
    for (Particle& q : g.particles) q.pos = Vector2Add(q.pos, Vector2Scale(q.vel, dt)), q.vel = Vector2Scale(q.vel, powf(0.02f, dt)), q.t += dt;
    std::erase_if(g.particles, [](const Particle& q) { return q.t > q.life; });
}

Seed seedAt(Vector2 pos, int type, int amount) { return {pos, type, amount, PICKUP_DESPAWN, 0, rndr(0.4f, 0.7f)}; }

void onEnemyRemoved(Game& g, const Enemy& e) {
    Player& p = g.p;
    addKill(g);
    if (int n = p.items[IT_SPORES]) g.blasts.push_back({e.pos, 30.f + 12 * n, std::max(1, e.lastHit), 0});
    if (!e.boss()) {
        dropSeed(g, e.pos, e.exp);
        auto near = [&] { return Vector2Add(e.pos, {rndr(-12, 12), rndr(-12, 12)}); };
        if (rnd() < 0.02f * p.items[IT_TOMATO]) g.seeds.push_back(seedAt(near(), SEED_HEAL, HEAL_AMOUNT));
        if (rnd() < hyper(0.03f, p.items[IT_CARP])) g.seeds.push_back(seedAt(near(), SEED_SILVER, 1));
        if (e.variant) p.stats[ST_ELITES]++;
        if (e.variant == V_GILDED) {
            for (int k = 0; k < 3; k++) g.seeds.push_back(seedAt(near(), SEED_GOLD, 1));
            for (int k = 0; k < 2; k++) g.seeds.push_back(seedAt(near(), SEED_SILVER, 1));
        }
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
    p.stats[ST_GUARDIANS]++;
    if (e.portal >= 0) {
        g.portals[e.portal].state = Portal::PURIFIED;
        g.chests.push_back({Vector2Add(g.portals[e.portal].pos, {0, 28}), true});  // the guardian's reward
    }
    g.bossDefeated = true;
    if (!g.endTimes) startEndTimes(g);
}

// Enemy pathing: a Dijkstra flow field over the open tile corners, rebuilt whenever the
// player reaches a new corner and cut off past the active radius.
constexpr uint16_t FLOW_FAR = 0xffff;
void updateFlow(Game& g) {
    const std::vector<bool>& open = g.map.open;
    int src = vertexIndex(g.p.pos);
    if (open.empty() || src == g.flowAt || src < 0 || src >= int(open.size())) return;
    g.flowAt = src;
    g.flow.assign(open.size(), FLOW_FAR);
    const int limit = int(ACTIVE_RADIUS * 1.3f / TILE) * 10;
    std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<>> q;
    g.flow[src] = 0, q.push({0, src});
    while (!q.empty()) {
        auto [d, v] = q.top();
        q.pop();
        if (d > g.flow[v] || d >= limit) continue;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int n = v + dx + dy * MAP_N, nd = d + (dx && dy ? 14 : 10);
                if ((!dx && !dy) || !open[n] || nd >= g.flow[n]) continue;
                if (dx && dy && (!open[v + dx] || !open[v + dy * MAP_N])) continue;  // no cutting corners past a trunk or shore
                g.flow[n] = uint16_t(nd), q.push({nd, n});
            }
    }
}

// Straight at the player while the way is clear; otherwise downhill on the flow field,
// looking two corners ahead so the path bends smoothly round trees and inlets.
bool clearLine(const Game& g, Vector2 a, Vector2 b) {
    // The same test movement uses, starting a step out: a coarse check lets them nose into trunks and shores.
    float len = Vector2Distance(a, b);
    for (float d = 2; d < len; d += TILE / 2) {
        Vector2 s = Vector2Lerp(a, b, d / len);
        if (!g.map.at(s) || g.map.blocked(s, 3)) return false;
    }
    return true;
}
Vector2 pathDir(const Game& g, Vector2 at, Vector2 dir) {
    if (g.flow.empty() || clearLine(g, at, g.p.pos)) return dir;
    int vx = int(roundf(at.x / TILE)) + MAP_N / 2, vy = int(roundf(at.y / TILE)) + MAP_N / 2;
    if (vx < 2 || vy < 2 || vx >= MAP_N - 2 || vy >= MAP_N - 2) return dir;
    float best = 1e9f;
    Vector2 to = dir;
    for (int y = vy - 2; y <= vy + 2; y++)
        for (int x = vx - 2; x <= vx + 2; x++) {
            uint16_t f = g.flow[y * MAP_N + x];
            Vector2 c = vertexPos(y * MAP_N + x);
            float cost = f + Vector2Distance(at, c) * 5 / TILE;  // under the 10 per corner the flow drops, so farther downhill wins
            if (f == FLOW_FAR || cost >= best || Vector2Distance(at, c) <= 2) continue;
            bool reach = true;  // never aim through a trunk at a corner on its far side
            for (int k = 1; k <= 4 && reach; k++) {
                Vector2 s = Vector2Lerp(at, c, k / 4.f);
                reach = g.map.at(s) && !g.map.blocked(s, 2);
            }
            if (reach) best = cost, to = Vector2Normalize(Vector2Subtract(c, at));
        }
    return to;
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
        if (e.freezeT > 0) {
            e.freezeT -= dt, e.vel = {};
            continue;
        }

        Vector2 to = Vector2Subtract(p.pos, e.pos);
        float dist = Vector2Length(to);
        if (dist > DESPAWN_RADIUS && !e.boss()) { e.gone = true; continue; }
        Vector2 dir = dist > 0 ? Vector2Scale(to, 1 / dist) : Vector2{};
        float speed = e.speed * e.slowMul;
        const AttackDef& atk = ATTACKS[e.kind == NORMAL ? ENEMIES[e.type].attack : ATK_NONE];

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
            e.vel = Vector2Scale(e.dashDir, ENEMIES[e.type].speed * atk.mult);  // base speed: floor and time scaling would make lunges undodgeable
            if (e.dashT <= 0 && ENEMIES[e.type].attack == ATK_LEAP) {  // lands with a thump
                g.blasts.push_back({e.pos, LEAP_RADIUS * e.scale, int(e.damage * 0.75f), 0, true});
                playAt(A.thud, g, e.pos, rndr(1.1f, 1.3f), -12, 0.05);
            }
        } else if (e.castT > 0) {
            // Wind-up (drawn grey): stand still, then lunge at where the player was, slam, or burst out of the ground.
            e.vel = {};
            if ((e.castT -= dt) <= 0) {
                Attack a = ENEMIES[e.type].attack;
                if (a == ATK_SLAM) g.blasts.push_back({e.pos, SLAM_RADIUS, e.damage, 0, true});
                else if (a == ATK_BURROW) {
                    g.blasts.push_back({e.pos, BURROW_RADIUS, int(e.damage * 0.7f), 0, true});
                    burst(g, e.pos, rgb(.5f, .38f, .25f), 12, 90, 3);
                    e.specialT = rndr(3, 5);  // above ground for a while, then back under
                } else {
                    e.dashT = atk.time, e.dashDir = dir;
                    if (a == ATK_LEAP) playAt(A.croak, g, e.pos, rndr(0.9f, 1.2f), -10, 0.1);
                    if (a == ATK_SWOOP) playAt(A.caw, g, e.pos, rndr(0.9f, 1.2f), -10, 0.1);
                }
            }
        } else if (e.hurtT > 0) {
            e.hurtT -= dt;
            e.vel = Vector2Scale(e.vel, powf(0.85f, dt * 60));  // knockback skid
        } else {
            Vector2 want = Vector2Scale(dir, speed);
            Attack a = ENEMIES[e.type].attack;
            Vector2 way = dist <= ACTIVE_RADIUS && !ENEMIES[e.type].flies ? pathDir(g, e.pos, dir) : dir;
            bool detour = way.x != dir.x || way.y != dir.y;
            if (e.under) {  // tunnels at you, surfacing beside you after a wind-up
                want = Vector2Scale(way, speed * 1.4f);
                if (rnd() < dt * 8) burst(g, e.pos, rgb(.5f, .38f, .25f), 1, 30, 2);
                if (dist < atk.range) e.under = false, e.castT = atk.windup, playAt(A.dig, g, e.pos, rndr(0.9f, 1.1f), -8, 0.1);
            } else if (a == ATK_BURROW && (e.specialT -= dt) <= 0) {
                e.under = true;
                burst(g, e.pos, rgb(.5f, .38f, .25f), 8, 60, 2);
            }
            if (a == ATK_TRAIL && (e.trailT -= dt) <= 0 && g.patches.size() < 300) {
                e.trailT = rndr(atk.cdMin, atk.cdMax);
                g.patches.push_back({e.pos, 9 * e.scale, 4, 0, 0, true});
            }
            if (e.under) {
            } else if (dist <= ACTIVE_RADIUS) {
                if (e.shooter && dist <= 350) {
                    // Creep in while firing so melee builds can reach them; back off (slower than you) if rushed.
                    want = Vector2Scale(dir, dist < 140 ? -speed * 0.4f : dist > 200 ? speed * 0.35f : 0);
                    if ((e.shootT -= dt) <= 0) {
                        e.shootT = rndr(ATTACKS[ATK_SHOOT].cdMin, ATTACKS[ATK_SHOOT].cdMax);
                        bossShot(g, e, dir, std::max(1, int(e.damage * 0.6f)), 1, rgb(1, 0.3f, 0.2f));
                    }
                } else {
                    if (atk.windup > 0 && a != ATK_BURROW && (e.shootT -= dt) <= 0 && dist < atk.range) e.shootT = rndr(atk.cdMin, atk.cdMax), e.castT = atk.windup;
                    want = detour ? Vector2Scale(way, speed) : Vector2Scale(Vector2Rotate(dir, e.flank * std::clamp((dist - 60) / 300, 0.f, 1.f)), speed);
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
        // Far ones keep to land too, or they wade out and stall once they come near.
        // The whole step first: one axis alone can catch a shore corner the path cuts.
        if (e.boss() || ENEMIES[e.type].flies || g.map.walk(e.pos, Vector2Add(e.pos, step))) {
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
        case SEED_SILVER: p.silver += s.amount + p.items[IT_KOI]; A.orb.play(2, -5, 0.04); break;
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

constexpr float BOSS_ART = 0.8f;  // boss sheets fill more of the 48 px cell than the regular kinds
bool ownArt(const Enemy& e) { return e.kind == FINAL_BOSS || (e.kind == GUARDIAN && BOSSES[e.bossId].sheet); }

// One 48 px cell; a negative sy flips it upside down.
void drawFrame(Texture2D tex, int col, int row, Vector2 pos, float scale, Color tint, float sy = 1, float rot = 0) {
    float size = 48 * scale;
    DrawTexturePro(tex, {col * 48.f, row * 48.f, 48, 48 * (sy < 0 ? -1.f : 1.f)}, {pos.x, pos.y, size, size * fabsf(sy)}, {size / 2, size * fabsf(sy) / 2}, rot, tint);
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
    if (e.enraged) return ownArt(e) ? ColorLerp(WHITE, e.glow, 0.3f * (sinf(t * PI) + 1) / 2) : ColorLerp(e.color, e.glow, (sinf(t * PI) + 1) / 2);  // drawn art keeps its colours and glows
    if (e.freezeT > 0) return rgb(.6f, .85f, 1);
    if (e.burnTicks > 0) return rgb(1, .4f, .1f);
    if (e.slowT > 0) return rgb(.3f, .8f, 1);
    return e.variant ? ColorLerp(e.color, VARIANTS[e.variant].glow, 0.3f) : e.color;
}

// Every enemy dies the same way at heart (a white flash, a burst of bits, gone in a
// moment) but in its own manner: slimes and ratmen play their sheets, the rest move.
void drawEnemy(const Enemy& e, float t) {
    if (e.under) {  // a mound of churned earth
        float w = 1 + 0.15f * sinf(t * 20 + e.id);
        DrawEllipse(int(e.pos.x), int(e.pos.y + 4), 8 * w, 4, rgb(.36f, .26f, .18f));
        DrawEllipse(int(e.pos.x), int(e.pos.y + 3), 5 * w, 2.5f, rgb(.55f, .42f, .28f));
        return;
    }
    Color tint = enemyTint(e, t);
    int walk = int(e.anim * 5) % 4;
    float u = e.dying ? 1 - e.deathT / (e.ratman ? 0.8f : 0.4f) : 0;  // death progress 0..1
    bool flash = e.dying && u < 0.2f;
    if (e.dying) tint = Fade(tint, std::min(1.f, 2 - 2 * u));  // fade out over the second half
    Texture2D tex{};
    int col = walk, row = e.facing;
    Vector2 at = e.pos;
    float scale = e.scale, sy = 1, rot = 0;
    if (e.kind == NORMAL && e.dashT > 0 && ENEMIES[e.type].attack == ATK_LEAP)  // airborne
        at.y -= sinf((1 - e.dashT / ATTACKS[ATK_LEAP].time) * PI) * 14;
    if (e.kind == FINAL_BOSS) {
        tex = A.ratKing, scale *= BOSS_ART;
        if (e.dying) col = 0, rot = (e.facing == LEFT ? -90 : 90) * std::min(1.f, u * 2);  // keels over
    } else if (e.ratman) {
        if (e.hurtT > 0 && !e.boss()) tint = ColorBrightness(tint, -0.35f);
        tex = A.ratman;
        if (e.dying) col = 3 - std::min(3, int((0.8f - e.deathT) * 5)), row = 5;
    } else if (e.kind == GUARDIAN && BOSSES[e.bossId].sprite < 0) {
        int f = e.dying ? (e.deathT < 0.2f) : e.hurtT > 0 ? (e.hurtT < 0.075f) : walk;
        tex = A.guardian;
        col = e.dying ? 6 + f : e.hurtT > 0 ? (e.facing == RIGHT ? 3 - f : 4 + f) : (e.facing == RIGHT ? 7 - f : f);
        row = e.dying ? 0 : e.facing;
    } else if (A.enemySprites[e.type].id) {
        tex = A.enemySprites[e.type];
        if (e.kind == GUARDIAN) tex = A.bossSprites[e.bossId], scale *= BOSS_ART;
        if (e.hurtT > 0 && !e.dying) tint = ColorBrightness(tint, -0.35f);
        if (e.dying) {
            col = 0;
            switch (ENEMIES[e.type].death) {
                case DIE_TOPPLE: rot = (e.facing == LEFT ? -90 : 90) * std::min(1.f, u * 2), at.y += 4 * u * scale; break;  // keels over
                case DIE_FLIP: sy = u < 0.15f ? 1 : -1, at.y -= sinf(u * PI) * 10; break;  // hops onto its back
                case DIE_FALL: at.y += 16 * u * u, rot = 540 * u * u, scale *= 1 - 0.4f * u; break;  // spirals to the ground
                case DIE_CRUMBLE: sy = 1 - 0.7f * u, at.y += 24 * scale * 0.7f * u / 2; break;  // collapses into rubble
                default: scale *= 1 + 0.4f * u, sy = 1 - 0.3f * u; break;  // pops
            }
        }
    } else {
        tex = A.slime;
        col = e.dying ? 12 + (e.deathT < 0.2f) : e.hurtT > 0 ? 10 + (e.hurtT < 0.125f) : 6 + walk;
        row = e.dying ? 0 : e.facing;
    }
    if ((e.variant || (e.enraged && ownArt(e))) && !e.dying) {  // elites and enraged bosses glow round the edge
        BeginBlendMode(BLEND_ADDITIVE);
        drawFrame(tex, col, row, at, scale * 1.12f, Fade(e.variant ? VARIANTS[e.variant].glow : e.glow, 0.45f + 0.2f * sinf(t * 5 + e.id)), sy, rot);
        EndBlendMode();
    }
    drawFrame(tex, col, row, at, scale, tint, sy, rot);
    if (flash) {
        BeginBlendMode(BLEND_ADDITIVE);
        drawFrame(tex, col, row, at, scale, Fade(WHITE, 1 - u / 0.2f), sy, rot);
        EndBlendMode();
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
        int col = (pt.state == Portal::PURIFIED ? 3 : 0) + int(t * 6) % 3;
        drawFrame(A.portal, col, 0, pt.pos, 1.5f, pt.state == Portal::COMBAT ? rgb(1, .6f, .6f) : WHITE);
        bool pure = pt.state == Portal::PURIFIED;
        for (int k = 0; k < 8; k++) {  // loose petals (or dark sparks) lifting off the bloom and drifting away
            float u = fmodf(t * 0.35f + k / 8.f, 1), a = k * 2.4f + floorf(t * 0.35f + k / 8.f);
            Vector2 at = {pt.pos.x + cosf(a) * (14 + 26 * u) + sinf(t * 2 + k) * 4, pt.pos.y + 10 + sinf(a) * 8 - 38 * u};
            Color c = pure ? (k % 2 ? rgb(1, .9f, .95f) : rgb(.96f, .63f, .78f)) : (k % 2 ? rgb(.55f, .2f, .7f) : rgb(.88f, .44f, 1));
            DrawRectangleV(at, {2, pure ? 1.f : 2.f}, Fade(c, sinf(u * PI)));
        }
    }

    for (const Chest& c : g.chests) {
        if (c.free && !c.open) DrawCircleV(c.pos, 14 + 2 * sinf(t * 4), Fade(rgb(1, .8f, .2f), 0.18f));  // the guardian's reward glows
        drawFrame(A.chest, c.open ? 2 : c.free, 0, c.pos, 1, c.open ? Fade(WHITE, 0.7f) : WHITE);
    }

    // drops/pickups.png rows: exp seeds by size, then magnet, speed, bomb, gold, silver, heal (SeedType order).
    static const Color GLOW[] = {WHITE, rgb(1, .3f, .3f), rgb(.5f, .8f, 1), rgb(1, .5f, .2f), WHITE, WHITE, rgb(1, .4f, .5f)};
    for (const Seed& s : g.seeds) {
        int row = s.type == SEED_EXP ? (s.amount < 15 ? 0 : s.amount < 40 ? 1 : 2) : 2 + s.type;
        float alpha = s.life < PICKUP_WARNING && !s.magnetic ? 0.5f + 0.5f * (sinf(t * 20) > 0) : 1;
        float bob = s.magnetic ? 0 : 1.5f + 1.5f * sinf(t * 3 + s.glimmer * 20);
        int frame = int(t * (s.type == SEED_GOLD || s.type == SEED_SILVER ? 8 : 5) + s.glimmer * 9) % 4;
        DrawEllipse(int(s.pos.x), int(s.pos.y + 6), 4 - bob * 0.4f, 1.5f, Fade({46, 75, 77, 255}, 0.4f * alpha));
        if (s.type == SEED_MAGNET || s.type == SEED_SPEED || s.type == SEED_BOMB || s.type == SEED_HEAL)  // power-ups pulse
            DrawCircleV({s.pos.x, s.pos.y - bob}, 9 + sinf(t * 5), Fade(GLOW[s.type], 0.18f * alpha));
        DrawTextureRec(A.pickups, {frame * 16.f, row * 16.f, 16, 16}, {roundf(s.pos.x - 8), roundf(s.pos.y - 8 - bob)}, Fade(WHITE, alpha));
    }

    for (const Patch& q : g.patches) {
        float fade = std::min(1.f, q.life * 2);
        if (q.hostile) {
            DrawEllipse(int(q.pos.x), int(q.pos.y), q.radius, q.radius * 0.6f, Fade(rgb(.75f, .9f, .6f), 0.3f * fade));
            continue;
        }
        float d = q.radius * 2 * std::min(1.f, (3.5f - q.life) * 6);  // sprouts up fast
        DrawTexturePro(A.bramble, {float(int(t * 6 + q.pos.x) % 4 * 32), 0, 32, 32}, {q.pos.x, q.pos.y, d, d}, {d / 2, d / 2}, 0, Fade(WHITE, 0.9f * fade));
    }
    for (const Pod& q : g.pods)  // where each seed will land
        DrawCircleLinesV(q.to, q.radius * (q.t / q.time), Fade(rgb(.75f, .9f, .3f), 0.5f));

    for (const Weapon& w : p.weapons)
        if (w.id == POISON_AURA) {
            float r = 48 * w.stats().scale * p.aoe * w.size, d = r * (2 + 0.06f * sinf(t * 3));  // slow breathing
            DrawTexturePro(A.aura, {float(int(t * 6) % 4 * 48), 0, 48, 48}, {p.pos.x, p.pos.y, d, d}, {d / 2, d / 2}, 0, Fade(WHITE, 0.7f + w.pulse));
            if (w.pulse > 0) {  // each damage tick sweeps a ring out to the edge
                float k = 1 - w.pulse / 0.3f;
                DrawRing(p.pos, r * k - 2, r * k, 0, 360, 48, Fade(rgb(.94f, .54f, .85f), 0.7f * (1 - k)));
            }
        }

    for (const Enemy& e : g.enemies)  // golem slam tells: the ring fills in as the wind-up runs out
        if (e.castT > 0 && !e.dying && e.kind == NORMAL && ENEMIES[e.type].attack == ATK_SLAM) {
            float k = 1 - e.castT / ATTACKS[ATK_SLAM].windup;
            DrawCircleV(e.pos, SLAM_RADIUS * k, Fade(RED, 0.18f));
            DrawCircleLinesV(e.pos, SLAM_RADIUS, Fade(RED, 0.6f));
        }

    // Ground shadows, apart from the sprites so a death tumble or a leap leaves them on the ground.
    auto shadow = [](Vector2 at, float w, float a) { DrawEllipse(int(at.x), int(at.y), w / 2, w / 8, Fade({46, 75, 77, 255}, 0.44f * a)); };
    for (const Enemy& e : g.enemies) {
        if (e.under) continue;
        float u = e.dying ? 1 - e.deathT / (e.ratman ? 0.8f : 0.4f) : 0, w = 13 * e.scale;
        float feet = ownArt(e) ? 8 * BOSS_ART : e.ratman ? 5.5f : e.kind == GUARDIAN && BOSSES[e.bossId].sprite < 0 ? 6 : A.enemySprites[e.type].id ? 8 : 6;
        if (e.kind == NORMAL && e.dashT > 0 && ENEMIES[e.type].attack == ATK_LEAP) w *= 1 - 0.35f * sinf((1 - e.dashT / ATTACKS[ATK_LEAP].time) * PI);
        shadow({e.pos.x, e.pos.y + feet * e.scale}, w, 1 - u);
    }
    shadow({p.pos.x, p.pos.y + 5.5f}, 12, p.iframes > 0 && int(p.iframes * 10) % 2 ? 0.3f : 1);
    Vector2 lo = GetScreenToWorld2D({0, 0}, cam), hi = GetScreenToWorld2D({float(GetScreenWidth()), float(GetScreenHeight())}, cam);
    for (Vector2 tp : g.map.trees)
        if (tp.x > lo.x - 32 && tp.x < hi.x + 32 && tp.y > lo.y && tp.y < hi.y + 8) shadow({tp.x, tp.y + 1}, 30, 1);

    // Trees, enemies and the player sorted by their feet, like Godot's y-sort.
    // A tree the player stands behind turns see-through.
    std::vector<std::pair<float, int>> order;  // (feet y, index): enemies >= 0, player -1, trees <= -2
    for (int i = 0; i < int(g.enemies.size()); i++) order.push_back({g.enemies[i].pos.y + 8 * g.enemies[i].scale, i});
    order.push_back({p.pos.y + 9, -1});
    for (int i = 0; i < int(g.map.trees.size()); i++) {
        Vector2 tp = g.map.trees[i];
        if (tp.x > lo.x - 32 && tp.x < hi.x + 32 && tp.y > lo.y - 8 && tp.y < hi.y + 80) order.push_back({tp.y, -2 - i});
    }
    std::sort(order.begin(), order.end());
    for (auto [y, i] : order) {
        if (i >= 0) drawEnemy(g.enemies[i], t);
        else if (i == -1) {
            Color tint = p.burnTicks > 0 ? rgb(1, .6f, .4f) : p.slowT > 0 ? rgb(.7f, .85f, 1) : p.speedT > 0 ? rgb(.5f, .8f, 1) : WHITE;
            if (p.items[IT_PUMPKIN] && p.shieldT <= 0) DrawCircleLinesV({p.pos.x, p.pos.y - 2}, 13, Fade(rgb(1, .6f, .15f), 0.5f + 0.2f * sinf(t * 4)));
            if (p.iframes > 0) tint = Fade(tint, int(p.iframes * 10) % 2 ? 0.3f : 1.f);
            drawFrame(A.characters[p.character], p.moving ? int(p.anim * 5) % 4 : 0, p.facing, p.pos, 1, tint);
        } else {
            // world/tree.png (tools/trees.lua): a row per biome, four 48x72 variants each, some mirrored; base on cell row 68.
            int k = -2 - i;
            uint32_t h = uint32_t(k) * 2654435761u;
            Vector2 base = g.map.trees[k];
            Color tint = Fade(WHITE, Vector2Distance(p.pos, {base.x, base.y - 33}) < 26 ? 0.4f : 1.f);
            DrawTexturePro(A.tree, {float(h >> 16 & 3) * 48, float(g.map.cfg->biome) * 72, h >> 20 & 1 ? -48.f : 48.f, 72},
                           {roundf(base.x), roundf(base.y), 48, 72}, {24, 68}, 0, tint);
        }
    }

    Rectangle bolt = {float(int(t * 12) % 4 * 16), 0, 16, 16}, orb = {float(int(t * 8) % 4 * 16), 0, 16, 16};
    for (const Shot& s : g.shots) {
        if (s.kind == SHOT_AXE) {
            Texture2D axe = A.weaponIcons[AXE];
            float size = 20 * s.size;
            DrawTexturePro(axe, {0, 0, 16, 16}, {s.pos.x, s.pos.y, size, size}, {size / 2, size / 2}, s.t * 1100, WHITE);
            continue;
        }
        float size = 16 * s.size * (1 + 0.12f * sinf(t * 30 + s.life * 7));  // shimmer
        for (int k = 3; k >= 1; k--) {  // fading trail
            Vector2 at = Vector2Subtract(s.pos, Vector2Scale(s.dir, k * 4 * s.size));
            float ks = size * (1 - k * 0.2f);
            DrawTexturePro(A.projectile, bolt, {at.x, at.y, ks, ks}, {ks / 2, ks / 2}, 0, Fade(s.crit ? rgb(1, .8f, .1f) : WHITE, 0.4f / k));
        }
        DrawTexturePro(A.projectile, bolt, {s.pos.x, s.pos.y, size, size}, {size / 2, size / 2},
                       atan2f(s.dir.y, s.dir.x) * RAD2DEG, s.crit ? rgb(1, .8f, .1f) : WHITE);
    }
    for (const Weapon& w : p.weapons)
        if (w.id == ORBIT)
            for (int k = 0; k < orbCount(p, w); k++) {
                for (int j = 3; j >= 1; j--) {  // afterimages along the orbit
                    Weapon ghost = w;
                    ghost.spin -= j * 0.12f;
                    Vector2 at = orbPos(p, ghost, k);
                    float size = 2.5f * orbRadius(w) * (1 - j * 0.12f);
                    DrawTexturePro(A.thorn, orb, {at.x, at.y, size, size}, {size / 2, size / 2}, ghost.spin * RAD2DEG * 3, Fade(WHITE, 0.3f / j));
                }
                Vector2 at = orbPos(p, w, k);
                float size = 2.5f * orbRadius(w);
                DrawTexturePro(A.thorn, orb, {at.x, at.y, size, size}, {size / 2, size / 2}, w.spin * RAD2DEG * 3, WHITE);
            }
    for (const Slash& s : g.slashes) {  // slash.png bulges toward +x
        float k = s.t / 0.2f, size = s.radius * 1.5f;
        Vector2 at = Vector2Add(p.pos, Vector2Rotate({s.radius * 0.45f, 0}, s.angle));
        DrawTexturePro(A.slash, {0, 0, 48, s.flip ? -48.f : 48.f}, {at.x, at.y, size * (0.8f + 0.3f * k), size}, {size * (0.8f + 0.3f * k) / 2, size / 2},
                       s.angle * RAD2DEG, Fade(rgb(1, .85f, .95f), 1 - k * k));
    }
    for (const Pod& q : g.pods) {
        float u = q.t / q.time;
        Vector2 ground = Vector2Lerp(q.from, q.to, u), at = {ground.x, ground.y - sinf(u * PI) * 50};
        DrawEllipse(int(ground.x), int(ground.y + 3), 4, 2, Fade(BLACK, 0.3f));
        DrawTexturePro(A.pod, {0, 0, 16, 16}, {at.x, at.y, 14, 14}, {7, 7}, u * 540, WHITE);
    }
    for (const EnemyShot& s : g.enemyShots) {
        DrawCircleV(s.pos, 7 * s.scale, Fade(s.color, 0.25f));
        DrawCircleV(s.pos, 4 * s.scale, s.color);
        DrawCircleV(s.pos, 1.8f * s.scale, Fade(WHITE, 0.7f));
    }
    for (const Blast& b : g.blasts)
        if (b.wait > 0) {  // incoming: a red warning that blinks faster as it lands
            DrawCircleV(b.pos, b.radius, Fade(RED, 0.12f + 0.08f * sinf(t * 10 / std::max(0.2f, b.wait))));
            DrawCircleLinesV(b.pos, b.radius, Fade(RED, 0.7f));
        } else if (b.fx) DrawCircleV(b.pos, b.radius * (0.6f + b.t * 1.6f), Fade(rgb(.75f, .9f, .3f), 0.45f * (1 - b.t / 0.25f)));
        else if (b.hostile) DrawRing(b.pos, b.radius * (0.4f + b.t * 2.4f) - 4, b.radius * (0.4f + b.t * 2.4f), 0, 360, 32, Fade(rgb(.85f, .8f, .7f), 1 - b.t / 0.25f));
        else DrawCircleV(b.pos, b.radius * (0.5f + b.t * 2), Fade(rgb(1, .6f, .2f), 0.5f * (1 - b.t / 0.25f)));
    for (const Particle& q : g.particles)
        DrawRectangleV({q.pos.x - q.size / 2, q.pos.y - q.size / 2}, {q.size, q.size}, Fade(q.color, 1 - q.t / q.life));
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
        prompt(c.free ? "[E] Open chest" : TextFormat("[E] Open chest - %d silver", chestCost(g)), {c.pos.x, c.pos.y - 18});
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
    float grow[128]{};        // per-button hover scale, indexed by button id (ids go up to 90)
    bool options = false;
    int lastExp = -1, seenRoll = -1;
    float expShowT = 0, turnT = 0, cardT = 0;
    Vector2 scroll{}, dir{1, 0}, target{1, 0};
} ui;

// Touch screens (the web build on phones): a joystick wherever a finger lands on the left
// half, and a USE button for the E key. Taps on menus already arrive as left clicks.
// ponytail: raylib's web touch-end drops the last touch slot, not always the lifted finger,
// so with two fingers down the stick can lag until both lift; track ids ourselves if it bites.
struct TouchPad { bool on = false; int id = -1; Vector2 base{}, at{}; std::vector<int> held; } pad;
constexpr float PAD_R = 70;
Rectangle useRect() { return {ui.w - 210, ui.h - 220, 160, 160}; }

void touchControls(Game& g) {
    float k = ui.w / GetScreenWidth();
    std::vector<int> now;
    bool stick = false;
    for (int i = 0; i < GetTouchPointCount(); i++) {
        int id = GetTouchPointId(i);
        Vector2 at = Vector2Scale(GetTouchPosition(i), k);
        bool fresh = std::find(pad.held.begin(), pad.held.end(), id) == pad.held.end();
        now.push_back(id), pad.on = true;
        if (id == pad.id) pad.at = at, stick = true;
        else if (fresh && g.mode == Mode::Play) {
            if (CheckCollisionPointRec(at, useRect())) interact(g);
            else if (pad.id < 0 && at.x < ui.w / 2 && at.y > 90) pad.id = id, pad.base = pad.at = at, stick = true;
        }
    }
    if (!stick) pad.id = -1;
    pad.held = now;
    Vector2 d = Vector2Scale(Vector2Subtract(pad.at, pad.base), 1 / PAD_R);
    touchMove = pad.id >= 0 && g.mode == Mode::Play && Vector2Length(d) > 0.2f ? Vector2ClampValue(d, 0, 1) : Vector2{};
}

void drawTouchPad(const Game& g) {
    if (!pad.on || g.mode != Mode::Play) return;
    if (pad.id >= 0) {
        Vector2 knob = Vector2Add(pad.base, Vector2Scale(touchMove, PAD_R));
        DrawTexturePro(A.padRing, {0, 0, 356, 356}, {pad.base.x - PAD_R, pad.base.y - PAD_R, 2 * PAD_R, 2 * PAD_R}, {}, 0, Fade(WHITE, 0.7f));
        DrawTexturePro(A.padKnob, {0, 0, 100, 100}, {knob.x - 28, knob.y - 28, 56, 56}, {}, 0, Fade(WHITE, 0.8f));
    }
    if (nearChest(g) < 0 && nearPortal(g) < 0) return;
    Rectangle r = useRect();
    Vector2 c = {r.x + r.width / 2, r.y + r.height / 2};
    DrawCircleV(c, r.width / 2 - 8, Fade(BLACK, 0.35f));
    DrawTexturePro(A.padRing, {0, 0, 356, 356}, r, {}, 0, WHITE);
    text("USE", c.x, c.y - 18, 28, WHITE, true, 4);
}

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
    playMusic("ui/shopping.ogg", -8);
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
void panel(Rectangle d, float k, Color tint);
std::string wrap(const std::string& s, float size, float width);

// A character's walk sheet facing down, cropped to the body; `walk` animates it.
void portrait(int c, Rectangle d, bool walk, Color tint) {
    int col = walk ? int(GetTime() * 6) % 4 : 0;
    DrawTexturePro(A.characters[c], {col * 48.f + 12, 12, 24, 24}, d, {}, 0, tint);
}

// Upgrades on the left; on the right a grid of character cards over the picked one's details.
void shopScreen(Game& g) {
    float cx = ui.w / 2, lx = cx - 560, rx = cx + 24;
    wallpaper();
    DrawRectangle(0, 0, int(ui.w) + 1, int(ui.h) + 1, gray(.1f, .6f));
    textIn("PREPARATION", {0, 18, ui.w, 48}, 40, WHITE, 5, rgb(.2f, .08f, .2f));
    textIn(TextFormat("Total Coins: %d", profile.coins), {0, 66, ui.w, 24}, 20, rgb(1, .84f, 0), 4);
    text("UPGRADES", lx, 106, 20, gray(.85f, 1), false, 4);
    text("CHARACTER", rx, 106, 20, gray(.85f, 1), false, 4);
    std::string hover;
    for (int i = 0; i < PERM_COUNT; i++) {
        const PermUpgrade& u = PERM_UPGRADES[i];
        int cost = upgradeCost(profile, i);
        Rectangle r = {lx, 138 + i * 52.f, 500, 46};
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
    auto lockHint = [](UnlockKind kind, int i) {
        const UnlockDef& u = *unlockFor(kind, i);
        return std::string(TextFormat("Locked: %s (%d / %d)", u.how, std::min(profile.stats[u.stat], u.need), u.need));
    };
    constexpr float CW = 124, CH = 110, GAP = 12;
    for (int c = 0; c < CHARACTER_COUNT; c++) {
        Rectangle r = {rx + (c % 4) * (CW + GAP), 138 + (c / 4) * (CH + GAP), CW, CH};
        bool open = unlocked(profile, UL_CHARACTER, c), picked = profile.character == c;
        bool over = CheckCollisionPointRec(GetMousePosition(), r);
        if (clicked(30 + c, over, open)) profile.character = c, save();
        Rectangle d = grown(30 + c, r, over && open);
        float k = d.width / r.width;
        panel(d, 2.5f * k, picked ? WHITE : over && open ? gray(.85f, 1) : gray(.6f, 1));
        if (picked) DrawRectangleLinesEx({d.x - 3, d.y - 3, d.width + 6, d.height + 6}, 2, rgb(.5f, 1, .5f));
        portrait(c, {d.x + d.width / 2 - 36 * k, d.y + 6 * k, 72 * k, 72 * k}, picked || (over && open), open ? WHITE : gray(0, 0.7f));
        textIn(open ? CHARACTERS[c].name : "???", {d.x, d.y + d.height - 30 * k, d.width, 20 * k}, 16 * k, open ? picked ? rgb(.5f, 1, .5f) : WHITE : gray(.6f, 1), 3 * k);
        if (over) hover = open ? TextFormat("Play as %s: %s.", CHARACTERS[c].name, CHARACTERS[c].perk) : lockHint(UL_CHARACTER, c);
    }
    // The picked character: a big portrait, perk and starting weapon.
    const CharacterDef& ch = CHARACTERS[profile.character];
    WeaponId wid = ch.weapon;
    Rectangle info = {rx, 138 + 2 * (CH + GAP) + 4, 4 * CW + 3 * GAP, 172};
    panel(info, 2.5f, WHITE);
    portrait(profile.character, {info.x + 14, info.y + 14, 144, 144}, true, WHITE);
    float tx = info.x + 172, tw = info.width - 186;
    text(ch.name, tx, info.y + 14, 28, rgb(.5f, 1, .5f), false, 4);
    text(wrap(ch.perk, 16, tw).c_str(), tx, info.y + 52, 16, WHITE, false, 3);
    Rectangle slot = {tx, info.y + 100, 56, 56};
    DrawTexturePro(A.weaponSlot, {0, 0, 64, 64}, slot, {}, 0, WHITE);
    DrawTexturePro(A.weaponIcons[wid], {0, 0, float(A.weaponIcons[wid].width), float(A.weaponIcons[wid].height)}, {slot.x + 10, slot.y + 10, 36, 36}, {}, 0, WHITE);
    text(WEAPON_NAMES[wid], slot.x + 66, slot.y + 4, 18, WEAPON_COLOR, false, 3);
    text(wrap(WEAPON_DESC[wid], 14, tw - 66).c_str(), slot.x + 66, slot.y + 28, 14, gray(.85f, 1), false, 3);

    float y = 572;
    const char* details = hover.empty() ? ui.hint.c_str() : hover.c_str();
    float dw = measure(details, 16).x;
    if (*details) DrawRectangleRec({cx - dw / 2 - 8, y, dw + 16, 24}, gray(.1f, .7f));
    textIn(details, {0, y + 2, ui.w, 20}, 16, WHITE);
    int refund = respecRefund(profile);
    if (imageButton(20, {cx - 238, y + 40, 150, 56}, "RESPEC", 20) && refund > 0) {
        respec(profile);
        save();
        ui.hint = TextFormat("Upgrades reset! Refunded %d coins.", refund);
    }
    if (imageButton(21, {cx - 75, y + 34, 150, 62}, "START", 26) || IsKeyPressed(KEY_ENTER)) newRun(g);
    if (imageButton(22, {cx + 88, y + 40, 150, 56}, "QUIT", 20) || IsKeyPressed(KEY_ESCAPE)) toTitle(g);
}

// Nine-slice of `src` (border b px) drawn at k UI units per pixel. The edges and centre are
// tiled, not stretched, so every pixel stays the same size; a whole k snaps d to its grid.
void slice(Texture2D tex, Rectangle src, float b, Rectangle d, float k, Color tint = WHITE) {
    if (k == floorf(k)) d = {roundf(d.x / k) * k, roundf(d.y / k) * k, roundf(d.width / k) * k, roundf(d.height / k) * k};
    float xs[] = {0, b, src.width - b, src.width}, ys[] = {0, b, src.height - b, src.height};
    float dx[] = {d.x, d.x + b * k, d.x + d.width - b * k, d.x + d.width}, dy[] = {d.y, d.y + b * k, d.y + d.height - b * k, d.y + d.height};
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++) {
            // Stretch like Godot's NinePatchRect when the span outgrows the source; tiling shows the art's edge shading as seams.
            float sw = xs[x + 1] - xs[x], sh = ys[y + 1] - ys[y], w = dx[x + 1] - dx[x], h = dy[y + 1] - dy[y];
            DrawTexturePro(tex, {src.x + xs[x], src.y + ys[y], std::min(sw, w / k), std::min(sh, h / k)}, {dx[x], dy[y], w, h}, {}, 0, tint);
        }
}

// ui/menu_buttons.png as a nine-slice, so its 5 px frame keeps its weight at any card size.
void panel(Rectangle d, float k, Color tint) { slice(A.menuButton, {0, 0, 128, 48}, 5, d, k, tint); }

// ui/hud.png pieces, see tools/hud.lua. HUD art is 2 UI units per pixel.
constexpr Rectangle HUD_BAR = {0, 0, 12, 10}, HUD_FILL = {12, 0, 6, 8}, HUD_SLOT = {18, 0, 16, 16}, HUD_CELL = {34, 0, 8, 8},
                    HUD_RING = {42, 0, 8, 8}, HUD_ARROW = {50, 0, 9, 9}, HUD_TIP = {59, 0, 12, 12}, HUD_SKULL = {71, 0, 10, 10};

// The PLAYER STATS / WEAPON BUFFS columns of the pause screen (hud.gd), centred and kept above `bottom`.
void statsPanel(const Player& p, float bottom) {
    struct Row { std::string name, value; Color color; bool header; };
    std::vector<Row> cols[2];
    std::vector<Row>& rows = cols[0];
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
    if (p.armor > 0) rows.push_back({"Armor:", pct(std::min(p.armor, 0.6f)), rgb(.8f, .65f, .45f), false});
    if (p.critMul > 2) rows.push_back({"Crit Damage:", pct(p.critMul), rgb(1, 0, 1), false});
    if (p.pierce > 0) rows.push_back({"Pierce:", TextFormat("+%d", p.pierce), YELLOW, false});
    if (p.revives > 0) rows.push_back({"Revives:", TextFormat("%d", p.revives), rgb(1, .85f, .3f), false});
    std::vector<Row>& buffs = cols[1];
    buffs.push_back({"WEAPON BUFFS", "", YELLOW, true});
    for (const Weapon& w : p.weapons) {
        buffs.push_back({"Equipped:", TextFormat("%s Lv.%d", WEAPON_NAMES[w.id], w.level), rgb(1, .65f, 0), false});
        if (w.damage > 1) rows.push_back({"Bonus Dmg:", "+" + pct(w.damage - 1), rgb(1, .39f, .28f), false});
        if (w.size > 1) rows.push_back({"Bonus Size:", "+" + pct(w.size - 1), rgb(.56f, .93f, .56f), false});
        if (w.fireRate > 1) rows.push_back({"Bonus Speed:", "+" + pct(w.fireRate - 1), rgb(0, 1, 1), false});
        if (w.pierce > 0) rows.push_back({"Pierce:", TextFormat("+%d", w.pierce), YELLOW, false});
        if (w.ricochet > 0) rows.push_back({"Ricochet:", TextFormat("+%d", w.ricochet), rgb(.68f, .85f, .9f), false});
        if (w.projectile > 0) rows.push_back({"Projectiles:", TextFormat("+%d", w.projectile), YELLOW, false});
    }
    float c1[2] = {}, c2[2] = {};
    for (int i = 0; i < 2; i++)
        for (const Row& r : cols[i]) c1[i] = std::max(c1[i], textWidth(r.name, 16)), c2[i] = std::max(c2[i], textWidth(r.value, 16));
    float colW[2] = {c1[0] + c2[0] + 44, c1[1] + c2[1] + 44}, bw = colW[0] + colW[1] - 4, bh = std::max(rows.size(), buffs.size()) * 24.f + 26;
    Rectangle box = {roundf(ui.w / 2 - bw / 2), roundf(std::min(ui.h / 2 - bh / 2, bottom - bh)), bw, bh};
    panel(box, 2, Fade(WHITE, 0.95f));
    for (int i = 0; i < 2; i++) {
        float x = box.x + 20 + i * colW[0], y = box.y + 15;
        for (const Row& r : cols[i]) {
            text(r.name.c_str(), x, y, 16, r.color);
            text(r.value.c_str(), x + 4 + c1[i], y, 16, WHITE);
            y += 24;
        }
    }
}

// ui/hud.tscn.
// Points from the screen edge at a world spot that is off screen (the world is drawn at ZOOM around the player).
void edgeMarker(const Game& g, Vector2 at, Color c) {
    Vector2 d = Vector2Scale(Vector2Subtract(at, g.p.pos), ZOOM), half = {ui.w / 2 - 28, ui.h / 2 - 34};
    if (fabsf(d.x) < half.x && fabsf(d.y) < half.y) return;
    Vector2 m = Vector2Add({ui.w / 2, ui.h / 2}, Vector2Scale(d, std::min(half.x / fabsf(d.x), half.y / fabsf(d.y))));
    DrawTexturePro(A.hud, HUD_ARROW, {m.x, m.y, 18, 18}, {9, 9}, atan2f(d.y, d.x) * RAD2DEG, c);
}

// Name and description beside a hovered HUD icon; only while the mouse is free (paused or picking).
void tooltip(Rectangle icon, const char* name, const char* desc, Color c) {
    if (!CheckCollisionPointRec(GetMousePosition(), icon)) return;
    float tw = std::max(measure(name, 16).x, measure(desc, 16).x);
    Rectangle box = {icon.x + icon.width + 6, icon.y, tw + 24, 52};
    slice(A.hud, HUD_TIP, 3, box, 2);
    text(name, box.x + 12, box.y + 7, 16, c);
    text(desc, box.x + 12, box.y + 27, 16, WHITE);
}

// One 16 px frame of drops/pickups.png (row: see drawWorld) drawn at `size`.
void pickupIcon(int row, Vector2 at, float size, float alpha = 1) {
    DrawTexturePro(A.pickups, {0, row * 16.f, 16, 16}, {at.x, at.y, size, size}, {}, 0, Fade(WHITE, alpha));
}

// A pixel bar: wood-rimmed trough, fill in 6 px segments tinted `fill`, outlined label.
void bar(Rectangle r, float frac, Color fill, const char* label, float size) {
    r = {roundf(r.x / 2) * 2, roundf(r.y / 2) * 2, roundf(r.width / 2) * 2, roundf(r.height / 2) * 2};
    slice(A.hud, HUD_BAR, 3, r, 2);
    Rectangle in = {r.x + 6, r.y + 6, r.width - 12, r.height - 12};
    float fw = roundf(in.width * std::clamp(frac, 0.f, 1.f) / 2) * 2;
    for (float x = 0; x < fw; x += 12) {
        float w = std::min(12.f, fw - x);
        DrawTexturePro(A.hud, {HUD_FILL.x, 0, w / 2, 8}, {in.x + x, in.y, w, in.height}, {}, 0, fill);
    }
    textIn(label, r, size, WHITE, 4);
}

// ui/hud.tscn, framed: player card top left (portrait, level, health, purse), weapons and an
// item grid under it, the clock and floor top centre, the exp bar along the bottom.
void drawHud(Game& g) {
    const Player& p = g.p;
    float w = ui.w, h = ui.h, t = float(GetTime());
    if (p.hp / p.maxHp <= 0.3f) DrawRectangle(0, 0, int(w) + 1, int(h) + 1, ColorAlpha(rgb(.278f, 0, 0), 0.204f + 0.06f * sinf(t * 6)));  // LowHPWarning
    for (const Portal& pt : g.portals) edgeMarker(g, pt.pos, pt.state == Portal::PURIFIED ? rgb(.6f, 1, .6f) : rgb(.8f, .5f, 1));
    for (const Enemy& e : g.enemies)
        if (e.boss() && !e.dying) edgeMarker(g, e.pos, RED);
    bool mouse = g.mode == Mode::Paused || g.mode == Mode::LevelUp;

    panel({6, 6, 312, 80}, 2, Fade(WHITE, 0.92f));
    slice(A.hud, HUD_SLOT, 5, {12, 12, 68, 68}, 2);
    DrawTexturePro(A.characters[p.character], {12, 12, 24, 24}, {22, 18, 48, 48}, {}, 0, WHITE);
    textIn(TextFormat("LV %d", p.level), {14, 62, 64, 16}, 14, YELLOW, 4);
    bar({90, 14, 220, 22}, p.hp / p.maxHp, p.hp / p.maxHp <= 0.3f ? rgb(1, .2f + .15f * sinf(t * 8), .15f) : rgb(.9f, .2f, .2f),
        TextFormat("%d / %d", int(std::max(0.f, p.hp)), int(p.maxHp)), 16);
    pickupIcon(8, {78, 8}, 32);
    auto counter = [&](float x, int row, const char* label, Color c) {
        pickupIcon(row, {x, 42}, 32);
        text(label, x + 30, 49, 16, c, false, 4);
    };
    counter(84, 6, TextFormat("%d", int(p.gold)), rgb(1, .86f, .3f));
    counter(162, 7, TextFormat("%d", p.silver), rgb(.85f, .88f, .95f));
    DrawTexturePro(A.hud, HUD_SKULL, {246, 48, 20, 20}, {}, 0, rgb(1, .8f, .75f));  // kills
    text(TextFormat("%d", p.kills), 272, 49, 16, rgb(1, .5f, .45f), false, 4);

    for (int i = 0; i < int(p.weapons.size()); i++) {
        Rectangle slot = {6 + i * 56.f, 92, 52, 52};
        slice(A.hud, HUD_SLOT, 5, slot, 2);
        Texture2D icon = A.weaponIcons[p.weapons[i].id];
        DrawTexturePro(icon, {0, 0, float(icon.width), float(icon.height)}, {slot.x + 10, slot.y + 10, 32, 32}, {}, 0, WHITE);
        const Weapon& wp = p.weapons[i];
        text(TextFormat("%d", wp.level), slot.x + 40, slot.y + 34, 14, wp.level >= 3 ? rgb(1, .85f, .3f) : WHITE, false, 4);
        if (mouse) tooltip(slot, WEAPON_NAMES[wp.id], TextFormat("Level %d", wp.level), WEAPON_COLOR);
    }
    int n = 0;
    for (int i = 0; i < ITEM_COUNT; i++)
        if (p.items[i]) {
            Rectangle cell = {6 + n % 8 * 40.f, 150 + n / 8 * 40.f, 40, 40};
            slice(A.hud, HUD_CELL, 2, cell, 2);
            slice(A.hud, HUD_RING, 2, cell, 2, Fade(ITEM_TIERS[ITEMS[i].tier].color, 0.8f));
            Texture2D icon = A.itemIcons[i];
            DrawTexturePro(icon, {0, 0, float(icon.width), float(icon.height)}, {cell.x + 4, cell.y + 4, 32, 32}, {}, 0, WHITE);
            if (p.items[i] > 1) text(TextFormat("%d", p.items[i]), cell.x + 28, cell.y + 24, 12, WHITE, false, 4);
            if (mouse) tooltip(cell, ITEMS[i].name, ITEMS[i].desc, ITEM_TIERS[ITEMS[i].tier].color);
            n++;
        }

    int secs = int(p.time);
    panel({w / 2 - 64, 6, 128, 38}, 2, Fade(WHITE, 0.92f));
    textIn(TextFormat("%02d:%02d", secs / 60, secs % 60), {w / 2 - 64, 6, 128, 38}, 24, g.endTimes ? rgb(1, .3f, .3f) : WHITE, 4);
    textIn(g.endTimes ? "THE END TIMES" : TextFormat("Floor %d  -  %s", g.floor, g.map.cfg->name), {0, 46, w, 16}, 13,
           g.endTimes ? rgb(1, .35f, .35f) : gray(.92f, 1), 4);

    for (const Enemy& e : g.enemies)
        if (e.boss() && !e.dying) {
            textIn(e.kind == GUARDIAN ? BOSSES[e.bossId].name : "The Rat King", {0, 66, w, 20}, 18, e.enraged ? e.glow : WHITE, 4);
            bar({w / 2 - 300, 88, 600, 20}, e.hp / e.maxHp, e.enraged ? rgb(1, .35f, .15f) : rgb(.7f, .2f, .75f), "", 12);
            break;
        }

    if (p.exp != ui.lastExp) ui.expShowT = ui.lastExp < 0 ? 0 : 2, ui.lastExp = p.exp;
    ui.expShowT -= GetFrameTime();
    bar({0, h - 18, w, 18}, float(p.exp) / p.expNext, rgb(.2f, .5f, 1),
        ui.expShowT > 0 ? TextFormat("%d / %d", p.exp, p.expNext) : TextFormat("LVL %d", p.level), 14);

    if (g.toastT > 0) textIn(g.toast, {0, h - 140, w, 24}, 16, Fade(g.toastColor, std::min(1.f, g.toastT)), 4, Fade(BLACK, std::min(1.f, g.toastT)));

    if (imageButton(50, {w - 114.8f, 4.2f, 108.3f, 40.6f}, "PAUSE", 14.6f) && (g.mode == Mode::Play || g.mode == Mode::Paused))
        g.mode = g.mode == Mode::Play ? Mode::Paused : Mode::Play;
    drawTouchPad(g);
}

constexpr float CARD_W = 264, CARD_H = 320, CARD_GAP = 36, CARD_TOP = 150;  // cards span ui.h / 2 - CARD_TOP down
Rectangle optionRect(const Game& g, int i) {
    float total = g.options.size() * (CARD_W + CARD_GAP) - CARD_GAP;
    return {ui.w / 2 - total / 2 + i * (CARD_W + CARD_GAP), ui.h / 2 - CARD_TOP, CARD_W, CARD_H};
}

// Greedy word wrap to `width`.
std::string wrap(const std::string& s, float size, float width) {
    std::string out, line, word;
    auto flush = [&] {
        if (word.empty()) return;
        std::string next = line.empty() ? word : line + " " + word;
        if (!line.empty() && measure(next.c_str(), size).x > width) out += line + "\n", next = word;
        line = next, word.clear();
    };
    for (char c : s)
        if (c == ' ') flush();
        else word.push_back(c);
    flush();
    return out + line;
}

// A level-up pick on the menu button art: rarity banner, framed icon, name, effect, key.
// `in` (0..1) is its entrance: it rises and fades in.
bool optionCard(int id, Rectangle r, const Option& o, int key, float in) {
    bool hover = in >= 1 && CheckCollisionPointRec(GetMousePosition(), r), click = clicked(id, hover, true);
    Rectangle d = grown(id, r, hover);
    float e = 1 - (1 - in) * (1 - in) * (1 - in);
    d.y += (1 - e) * 48;
    float k = d.width / r.width, t = float(GetTime()), a = e;
    Color c = optionColor(o);
    bool shiny = (o.kind == OPT_UPGRADE && UPGRADES[o.index].unique) || o.rarity >= 2;
    if (shiny)  // rare and better glow
        DrawRectangleRounded({d.x - 6, d.y - 6, d.width + 12, d.height + 12}, 0.08f, 6, Fade(c, (0.25f + 0.15f * sinf(t * 4)) * a));
    panel(d, 2.5f * k, Fade(hover ? WHITE : gray(.85f, 1), a));
    Rectangle banner = {d.x + 12 * k, d.y + 12 * k, d.width - 24 * k, 28 * k};
    DrawRectangleRec(banner, Fade(c, 0.22f * a));
    DrawRectangleLinesEx(banner, 1, Fade(c, 0.7f * a));
    textIn(optionLabel(o), banner, 16 * k, Fade(c, a), 3 * k, Fade(BLACK, a));
    float s = 92 * k;
    Rectangle slot = {d.x + d.width / 2 - s / 2, d.y + 52 * k, s, s};
    DrawTexturePro(A.weaponSlot, {0, 0, 64, 64}, slot, {}, 0, Fade(WHITE, a));
    Texture2D icon = o.kind == OPT_UPGRADE ? A.itemIcons[UPGRADE_ICONS[o.index]] : A.weaponIcons[o.weapon];
    float bobY = hover ? -2 * k * fabsf(sinf(t * 6)) : 0;
    DrawTexturePro(icon, {0, 0, float(icon.width), float(icon.height)}, {slot.x + 10 * k, slot.y + 10 * k + bobY, s - 20 * k, s - 20 * k}, {}, 0, Fade(WHITE, a));
    float y = slot.y + s + 8 * k;
    std::string title = wrap(o.title, 24, CARD_W - 28);
    textIn(title, {d.x, y, d.width, 28 * k * lines(title).size()}, 24 * k, Fade(o.kind == OPT_UPGRADE && !shiny ? WHITE : c, a), 4 * k, Fade(BLACK, a));
    y += 28 * k * lines(title).size() + 6 * k;
    DrawRectangleRec({d.x + 28 * k, y, d.width - 56 * k, 2 * k}, Fade(c, 0.35f * a));
    y += 10 * k;
    textIn(wrap(o.desc, 18, CARD_W - 36), {d.x, y, d.width, 64 * k}, 18 * k, Fade(gray(.95f, 1), a), 3 * k, Fade(BLACK, a));
    Rectangle keycap = {d.x + d.width / 2 - 14 * k, d.y + d.height - 36 * k, 28 * k, 24 * k};
    DrawRectangleRounded(keycap, 0.3f, 4, Fade(gray(hover ? .3f : .15f, .9f), a));
    DrawRectangleRoundedLinesEx(keycap, 0.3f, 4, 1, Fade(gray(.6f, 1), a));
    textIn(TextFormat("%d", key), keycap, 15 * k, Fade(WHITE, a));
    return click;
}

void reroll(Game& g) {
    if (g.p.rerolls <= 0) return;
    g.p.rerolls--;
    buildOptions(g);
}

void chooseOption(Game& g, int i) {
    applyOption(g, g.options[i]);
    if (--g.p.pendingLevels > 0) buildOptions(g);
    else g.mode = Mode::Play;
}

std::string clock(float seconds) { int s = int(seconds); return TextFormat("%02d:%02d", s / 60, s % 60); }

struct StatRow { const char* name; std::string value; Color color; };
// What this run unlocked, under the summary.
void unlockList(const Game& g, float y) {
    for (const std::string& s : g.unlocks) textIn("Unlocked: " + s, {0, y, ui.w, 22}, 18, rgb(1, .85f, .3f), 4), y += 24;
}

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
        case Mode::LevelUp: {
            if (ui.seenRoll != g.optionsRoll) ui.seenRoll = g.optionsRoll, ui.cardT = 0;
            ui.cardT += GetFrameTime();
            DrawRectangle(0, 0, int(w) + 1, int(h) + 1, gray(0, 0.55f));
            float bob = 4 * sinf(float(GetTime()) * 2.5f);
            textIn("BLOSSOM UP!", {0, cy - CARD_TOP - 150 + bob, w, 80}, 64, WHITE, 6, rgb(.2f, .08f, .2f));
            textIn(TextFormat("Level %d", g.p.level - g.p.pendingLevels + 1), {0, cy - CARD_TOP - 72, w, 28}, 24, YELLOW, 4);
            if (g.p.pendingLevels > 1) textIn(TextFormat("%d more to pick", g.p.pendingLevels - 1), {0, cy - CARD_TOP - 44, w, 20}, 16, gray(.85f, 1), 4);
            for (int i = 0; i < int(g.options.size()); i++)
                if (optionCard(60 + i, optionRect(g, i), g.options[i], i + 1, std::clamp((ui.cardT - i * 0.08f) / 0.3f, 0.f, 1.f))) {
                    chooseOption(g, i);
                    break;
                }
            float below = cy - CARD_TOP + CARD_H + 20;
            if (g.mode == Mode::LevelUp && g.p.rerolls > 0 &&
                imageButton(64, {cx - 90, below, 180, 54}, TextFormat("REROLL (%d)", g.p.rerolls), 20))
                reroll(g);
            textIn(g.p.rerolls > 0 ? "[1-3] Pick     [R] Reroll" : "[1-3] Pick", {0, below + 60, w, 20}, 16, gray(.75f, 1), 4);
            break;
        }
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
            statsPanel(g.p, h - 237);
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
            unlockList(g, cy + 124);
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
            unlockList(g, y + 58);
            if (imageButton(90, {cx - 64, y + 4, 128, 48}, "Return", 24)) toTitle(g);
            break;
        }
        default: break;
    }
}

void handleInput(Game& g) {
    touchControls(g);
    if (ui.options) return;  // optionsScreen handles its own input
    switch (g.mode) {
        case Mode::Title:
            if (IsKeyPressed(KEY_ESCAPE)) g.quit = true;
            break;
        case Mode::Shop: break;  // shopScreen handles its own input
        case Mode::Play:
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) g.mode = Mode::Paused;
            if (IsKeyPressed(KEY_E)) interact(g);
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
            if (IsKeyPressed(KEY_R)) reroll(g);
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
    updateFlow(g);
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

    // Enemies path round a tree between them and the player instead of pushing into its trunk.
    Game pg;
    startFloor(pg, 1, Player{}, 0);
    bool pathed = false;
    for (Vector2 t : pg.map.trees) {
        Vector2 above = {t.x, t.y - 40}, below = {t.x, t.y + 40};
        if (!pg.map.landAround(t, 4) || !pg.map.open[vertexIndex(above)] || !pg.map.open[vertexIndex(below)]) continue;
        pg.p.pos = below;
        Enemy e = makeEnemy(pg, above);
        e.speed = 55, e.flank = 0, e.hp = e.maxHp = 1e6f;
        pg.enemies = {e};
        for (int f = 0; f < 300; f++) pg.grid.build(pg.enemies), updateFlow(pg), updateEnemies(pg, 1 / 60.f);
        CHECK(Vector2Distance(pg.enemies[0].pos, pg.p.pos) < 24);
        pathed = true;
        break;
    }
    CHECK(pathed);
    // A crowd scattered over several floors, a few dropped in the water, all reach a player standing still.
    for (int floor : {1, 2, 2, 2, 2, MAX_FLOORS}) {
        Game cg;
        startFloor(cg, floor, Player{}, 0);
        cg.enemies.clear();
        for (int a = 0; a < 4000 && cg.enemies.size() < 16; a++) {
            Vector2 at = Vector2Add(cg.p.pos, {rndr(-500, 500), rndr(-500, 500)});
            if (cg.enemies.size() < 12 ? !cg.map.at(at) || cg.map.blocked(at) : cg.map.at(at)) continue;
            Enemy e = makeEnemy(cg, at);
            e.speed = 90, e.hp = e.maxHp = 1e6f;
            cg.enemies.push_back(e);
        }
        std::vector<bool> reached(cg.enemies.size());
        for (int f = 0; f < 60 * 25; f++) {
            cg.grid.build(cg.enemies), updateFlow(cg), updateEnemies(cg, 1 / 60.f);
            for (size_t i = 0; i < reached.size(); i++) reached[i] = reached[i] || Vector2Distance(cg.enemies[i].pos, cg.p.pos) < 80;
        }
        int n = int(std::count(reached.begin(), reached.end(), true));
        CHECK(n == int(reached.size()));
    }
    stopMusic();

    // Every guardian variant enrages at half health and fights with its special.
    for (int v = 0; v < BOSS_COUNT; v++) {
        Game bg;
        bg.map.field.assign(MAP_N * MAP_N, 255);
        bg.portals.push_back({{0, 0}});
        summonGuardian(bg, 0, v);
        hurtEnemy(bg, bg.enemies.back(), int(bg.enemies.back().maxHp / 2) + 1);
        CHECK(bg.enemies.back().enraged && bg.enemies.back().bossId == v);
        bg.enemies.back().specialT = 0, bg.p.pos = {200, 0};
        for (int f = 0; f < 180; f++) bg.grid.build(bg.enemies), updateEnemies(bg, 1 / 60.f), updateShots(bg, 1 / 60.f);
        CHECK(!bg.enemyShots.empty() || !bg.blasts.empty() || bg.enemies.size() > 1 || bg.enemies[0].charges > 0 || bg.enemies[0].dashT > 0 || bg.p.hp < bg.p.maxHp);
    }
    stopMusic();

    // Guardian: enrages at half health, dies, purifies its portal and starts the end times.
    Game g;
    g.map.field.assign(MAP_N * MAP_N, 255);
    g.portals.push_back({{0, 0}});
    summonGuardian(g, 0, 0);
    Enemy& boss = g.enemies.back();
    CHECK(g.portals[0].state == Portal::COMBAT && boss.kind == GUARDIAN && boss.hp == BOSS_HEALTH);
    hurtEnemy(g, boss, int(boss.maxHp / 2) + 1);
    CHECK(boss.enraged && boss.phase == 2);
    hurtEnemy(g, boss, int(boss.maxHp));
    CHECK(boss.dying);
    updateEnemies(g, 1);
    CHECK(g.enemies.empty() && g.portals[0].state == Portal::PURIFIED && g.bossDefeated && g.endTimes);
    CHECK(g.seeds.size() == 50);
    CHECK(g.chests.size() == 1 && g.chests[0].free);
    openChest(g, 0);
    CHECK(g.chests[0].open && nearChest(g) < 0 && std::accumulate(std::begin(g.p.items), std::end(g.p.items), 0) == 1);
    // Paid chests cost more after every purchase; the free one doesn't count.
    CHECK(g.p.chestsBought == 0 && chestCost(g) == CHEST_BASE);
    g.chests.push_back({{0, 0}});
    g.p.silver = 100;
    openChest(g, 1);
    CHECK(g.p.silver == 100 - CHEST_BASE && g.p.chestsBought == 1 && chestCost(g) > CHEST_BASE);
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

    // A golem winds up beside you, then its slam lands as a shockwave.
    Game gg;
    gg.map.field.assign(MAP_N * MAP_N, 255);
    Enemy golem = makeEnemy(gg, {40, 0});
    golem.type = TANK, golem.speed = 26, golem.damage = 30, golem.shootT = 0, golem.hp = golem.maxHp = 1000;
    gg.enemies.push_back(golem);
    for (int f = 0; f < 70; f++) gg.grid.build(gg.enemies), updateEnemies(gg, 1 / 60.f), updateShots(gg, 1 / 60.f);
    CHECK(gg.p.hp == Player{}.hp - 30);
    // Every death bursts into bits; shrooms also leave spores.
    Enemy shroom = makeEnemy(gg, {200, 0});
    shroom.type = SHOOTER, shroom.damage = 8;
    gg.enemies.push_back(shroom);
    size_t shots = gg.enemyShots.size(), bits = gg.particles.size();
    hurtEnemy(gg, gg.enemies.back(), 999);
    CHECK(gg.enemyShots.size() == shots + 4 && gg.particles.size() > bits);
    dg.runTime = 3600, dg.floor = 3;
    for (int i = 0; i < 20; i++) CHECK(spawnEnemy(dg, RUNNER, 0) && spawnEnemy(dg, RATMAN, 0));
    for (const Enemy& e : dg.enemies) CHECK(e.speed <= ENEMY_SPEED_CAP && ENEMY_SPEED_CAP < Player{}.speed);

    // Spiders split into spiderlings; elites are tougher; burrowed moles can't be found until they surface.
    Enemy spider = enemyOf(dg, SPIDER, {0, 0});
    spider.variant = V_NONE;
    dg.enemies.push_back(spider);
    dg.pending.clear();
    hurtEnemy(dg, dg.enemies.back(), 99999);
    CHECK(dg.pending.size() == 3 && dg.pending[0].type == SPIDERLING && dg.pending[0].variant == V_NONE);
    Enemy plain = enemyOf(dg, BASIC, {0, 0}), giant = plain;
    makeElite(giant, V_GIANT);
    CHECK(giant.hp == float(int(plain.hp * 3)) && giant.scale > plain.scale && giant.speed <= ENEMY_SPEED_CAP);
    Game mg;
    mg.map.field.assign(MAP_N * MAP_N, 255);
    Enemy mole = enemyOf(mg, MOLE, {300, 0});
    CHECK(mole.under);
    mg.enemies.push_back(mole);
    mg.grid.build(mg.enemies);
    CHECK(!nearestEnemy(mg, {300, 0}, 50));
    for (int f = 0; f < 240 && mg.enemies[0].under; f++) mg.grid.build(mg.enemies), updateEnemies(mg, 1 / 60.f);
    CHECK(!mg.enemies[0].under && mg.enemies[0].castT > 0);
    for (int f = 0; f < 60; f++) mg.grid.build(mg.enemies), updateEnemies(mg, 1 / 60.f), updateShots(mg, 1 / 60.f);
    CHECK(mg.p.hp < mg.p.maxHp);  // the burst as it surfaced beside you

    // Revives: a goldfish is used up first; without either, a lethal hit ends the run.
    Game rg;
    rg.mode = Mode::Play;
    rg.p.items[IT_GOLDFISH] = 1, rg.p.revives = 1, rg.p.hp = 5;
    hurtPlayer(rg, 999, nullptr);
    CHECK(rg.mode == Mode::Play && rg.p.hp == rg.p.maxHp / 2 && rg.p.items[IT_GOLDFISH] == 0 && rg.p.revives == 1);
    rg.p.iframes = 0, rg.p.hp = 5;
    hurtPlayer(rg, 999, nullptr);
    CHECK(rg.mode == Mode::Play && rg.p.revives == 0);
    // Pumpkin Shell blocks a hit, then recharges.
    rg.p.items[IT_PUMPKIN] = 1, rg.p.iframes = 0;
    float hp = rg.p.hp;
    hurtPlayer(rg, 10, nullptr);
    CHECK(rg.p.hp == hp && rg.p.shieldT > 0);

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

    // Several levels at once queue several picks; no new weapons are offered.
    Game lv;
    lv.mode = Mode::Play;
    lv.p.weapons.push_back({WAND});
    gainExp(lv, 500);
    CHECK(lv.mode == Mode::LevelUp && lv.p.pendingLevels > 1 && lv.options.size() == 3);
    for (int i = 0; i < 300; i++) {
        buildOptions(lv);
        for (const Option& o : lv.options) {
            CHECK(o.kind == OPT_UPGRADE || o.weapon == WAND);
            CHECK(!(o.kind == OPT_UPGRADE && o.index == MULTI_ATTACK) || o.value == 1);
            CHECK(!(o.kind == OPT_BUFF && o.index >= B_RICOCHET) || o.value <= 2);
            CHECK(!o.title.empty() && !o.desc.empty());
        }
        for (int a = 0; a < 3; a++)
            for (int b = a + 1; b < 3; b++)
                CHECK(lv.options[a].kind != lv.options[b].kind || lv.options[a].index != lv.options[b].index || lv.options[a].weapon != lv.options[b].weapon);
    }
    int rr = lv.p.rerolls;
    reroll(lv);
    CHECK(lv.p.rerolls == rr - 1 && lv.options.size() == 3);
    CHECK(lv.p.weapons.size() == 1);
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
    // Unlocks: thresholds on lifetime stats, reported once when crossed.
    CHECK(!unlocked(pr, UL_CHARACTER, 3) && unlocked(pr, UL_CHARACTER, 0) && !unlocked(pr, UL_ITEM, IT_GOLDFISH));
    int run[STAT_COUNT]{};
    run[ST_KILLS] = 1999, run[ST_FLOOR] = 3;
    std::vector<std::string> got = addStats(pr, run);
    CHECK(!unlocked(pr, UL_CHARACTER, 3) && unlocked(pr, UL_CHARACTER, 2) && got.size() == 1);
    run[ST_KILLS] = 1, run[ST_FLOOR] = 1;
    got = addStats(pr, run);
    CHECK(unlocked(pr, UL_CHARACTER, 3) && pr.stats[ST_FLOOR] == 3 && got.size() == 1 && got[0] == CHARACTERS[3].name);
    pr.character = 2;
    auto path = std::filesystem::temp_directory_path() / "convallaria_selftest" / "save.txt";
    CHECK(saveProfile(pr, path));
    Profile back = loadProfile(path);
    CHECK(back.coins == 750 && back.character == 2 && back.levels[PERM_MAX_HP] == 2 && back.levels[PERM_SPEED] == 0);
    CHECK(back.stats[ST_KILLS] == 2000 && back.stats[ST_FLOOR] == 3);
    pr.stats[ST_FLOOR] = 0;  // a locked character in a save falls back to the default
    CHECK(saveProfile(pr, path) && loadProfile(path).character == 0);
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
#ifdef __EMSCRIPTEN__
    // The browser build keeps its save in IndexedDB; wait for it to load.
    EM_ASM(FS.mkdirTree('/home/web_user/Convallaria'); FS.mount(IDBFS, {}, '/home/web_user/Convallaria');
           Module.synced = 0; FS.syncfs(true, function() { Module.synced = 1; }););
    while (!EM_ASM_INT(return Module.synced;)) emscripten_sleep(10);
#endif
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
