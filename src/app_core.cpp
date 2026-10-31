#include "app_internal.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <sys/stat.h>
#ifdef __vita__
#include <psp2/io/stat.h>
#endif

namespace app {
State S;
WNode g_tank, g_store;
const float WS = (float)SCREEN_H / 1080.f;

static const float TANK_L = -720, TANK_R = 720, TANK_T = 348, TANK_B = -280;

// ------------------------------------------------------------------ world scene
WNode buildW(const JV& j) {
    WNode n;
    n.name = j["n"].str();
    n.active = !j.has("off");
    n.x = j["p"][0].f(); n.y = j["p"][1].f(); n.z = j["p"][2].f();
    if (j.has("s")) { n.sx = j["s"][0].f(1); n.sy = j["s"][1].f(1); }
    n.rz = j["rz"].f();
    for (auto& c : j["c"].a) {
        if (c["t"].str() == "sr") {
            n.sp = gfx::sprite(c["sp"].str());
            n.c = mkcol(c["c"]); n.flip = c["fx"].i() != 0; n.order = c["o"].i();
        }
    }
    for (auto& k : j["k"].a) n.kids.push_back(buildW(k));
    return n;
}
WNode* findW(WNode& n, const std::string& name) {
    if (n.name == name) return &n;
    for (auto& k : n.kids) { WNode* r = findW(k, name); if (r) return r; }
    return nullptr;
}
void collectW(const WNode& n, float px, float py, float pz, float psx, float psy, std::vector<WDraw>& out, Color tint) {
    if (!n.active) return;
    float x = px + n.x * psx, y = py + n.y * psy, z = pz + n.z;
    float sx = psx * n.sx, sy = psy * n.sy;
    if (n.sp) out.push_back({n.sp, x, y, z, sx, sy, n.rz, n.c * tint, n.flip, n.order, SDL_BLENDMODE_BLEND});
    for (auto& k : n.kids) collectW(k, x, y, z, sx, sy, out, tint);
}

// ------------------------------------------------------------------ ui helpers
static std::unordered_map<std::string, UINode*> g_nodeCache;
UINode* N(const std::string& path) {
    auto it = g_nodeCache.find(path);
    if (it != g_nodeCache.end()) return it->second;
    UINode* n = ui::root ? ui::root->find(path) : nullptr;
    if (!n) SDL_Log("UI node not found: %s", path.c_str());
    g_nodeCache[path] = n;
    return n;
}
void setText(const std::string& p, const std::string& t) { if (UINode* n = N(p)) n->text = t; }
void show(const std::string& p, bool v) { if (UINode* n = N(p)) n->active = v; }
void onClick(const std::string& p, std::function<void()> f) { if (UINode* n = N(p)) { n->onClick = f; n->button = true; } }
std::string fmtNum(int v) {
    std::string s = std::to_string(std::abs(v)), o;
    int c = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) { o.insert(o.begin(), s[i]); if (++c % 3 == 0 && i > 0) o.insert(o.begin(), ','); }
    return v < 0 ? "-" + o : o;
}
void save() {
    G.musicVol = audio::musicVolume(); G.sfxVol = audio::sfxVolume();
    if (!G.save(S.saveDir + "/save.json")) SDL_Log("save failed");
}
void showToast(const std::string& t) { S.toast = t; S.toastT = 2.5f; }

float fishScaleForAge(int age) {
    if (age < GS::ageUntilTeen) return 1.3f + (float)age / GS::ageUntilTeen * 0.3f;   // baby art is drawn small
    float k = std::min(1.f, (float)(age - GS::ageUntilTeen) / (GS::ageUntilAdult - GS::ageUntilTeen));
    return 0.55f + k * 0.45f;
}
void artFor(int body, int fin, int age, int& b, int& f) {
    if (Game::magicKind(body, fin) == MAGIC_GOLDENGUPPY) { b = f = 900; return; }
    if (age < GS::ageUntilTeen) { b = f = 999; return; }
    b = body; f = fin;
}
void drawFishInRect(int body, int fin, int age, FRect r, float t, bool right) {
    int b, f; artFor(body, fin, age, b, f);
    float hw, hh; fishgfx::extents(b, hw, hh);
    float k = std::min(r.w / (hw * 2.2f), r.h / (hh * 2.4f));
    k = std::min(k, r.h / 90.f);
    fishgfx::draw(b, f, 0, r.x + r.w * .5f, r.y + r.h * .5f, k, right, t);
}

// ------------------------------------------------------------------ fish visuals / AI
static void pickTarget(FishVis& v) {
    v.tx = rndf(TANK_L + 80, TANK_R - 80);
    v.ty = rndf(TANK_B + 40, TANK_T - 40);
}
static FishVis& visFor(const Fish& f) {
    FishVis& v = S.vis[f.id];
    if (!v.init) {
        v.init = true;
        v.x = rndf(TANK_L + 100, TANK_R - 100); v.y = rndf(TANK_B + 60, TANK_T - 60);
        v.right = rnd(0, 2) == 0;
        v.mouthT = rndf(0, 2); v.wobble = rndf(0, 6.28f);
        pickTarget(v);
    }
    return v;
}
static void updateFishVis(const Fish& f, FishVis& v, float dt, bool dragged) {
    v.wobble += dt;
    v.mouthT -= dt; if (v.mouthT < -0.3f) v.mouthT = rndf(1.0f, 3.0f);
    if (v.bornFlash > 0) v.bornFlash -= dt;
    if (dragged) return;
    if (!f.alive()) {   // dead fish float to the surface
        v.y = std::min(TANK_T - 20, v.y + 40 * dt);
        return;
    }
    if (v.turnT >= 0) {
        v.turnT += dt;
        if (v.turnT > 0.42f) { v.turnT = -1; v.right = v.turnTo; }
        return;
    }
    if (v.rest > 0) { v.rest -= dt; v.vx *= 0.95f; v.vy *= 0.95f; v.x += v.vx * dt; v.y += v.vy * dt + sinf(v.wobble * 2) * 4 * dt; return; }
    float dx = v.tx - v.x, dy = v.ty - v.y;
    float d = sqrtf(dx * dx + dy * dy);
    float speed = f.age < GS::ageUntilTeen ? 70.f : 110.f;
    if (f.health < GS::maxHealth / 3) speed *= 0.6f;
    if (d < 25) {
        pickTarget(v);
        if (rnd(0, 100) < 25) v.rest = rndf(0.8f, 2.5f);
        return;
    }
    float tvx = dx / d * speed, tvy = dy / d * speed * 0.45f;
    v.vx += (tvx - v.vx) * std::min(1.f, dt * 1.8f);
    v.vy += (tvy - v.vy) * std::min(1.f, dt * 1.8f);
    bool wantRight = v.vx > 0;
    if (fabsf(v.vx) > 18 && wantRight != v.right) { v.turnT = 0; v.turnTo = wantRight; v.vx *= 0.3f; return; }
    v.x += v.vx * dt; v.y += v.vy * dt;
    v.x = std::max(TANK_L, std::min(TANK_R, v.x));
    v.y = std::max(TANK_B, std::min(TANK_T, v.y));
}
static int turnFrame(const FishVis& v) {
    if (v.turnT < 0) return 0;
    int f = 1 + (int)(v.turnT / 0.42f * 7.f);
    return std::min(7, std::max(1, f));
}
static Color fishTint(const Fish& f) {
    if (!f.alive()) return Color{0.55f, 0.55f, 0.55f, 1};
    if (f.infested == 2) return Color{0.85f, 1.0f, 0.8f, 1};
    if (f.infested == 1) return Color{0.95f, 0.82f, 1.0f, 1};
    return Color();
}
static void drawTankFish(const Fish& f, const FishVis& v, float alpha) {
    int b, fn; artFor(f.body, f.fin, f.age, b, fn);
    float k = WS * 0.8f * fishScaleForAge(f.age);
    Color c = fishTint(f); c.a = alpha;
    if (!f.alive()) {
        const Sprite* ds = gfx::spriteByName("Dead_Fish");
        if (ds) { gfx::drawSprite(ds, wsx(v.x), wsy(v.y), k * 2, k * 2, 0, c, v.right); return; }
    }
    int frame = turnFrame(v);
    bool right = v.right;
    if (v.turnT >= 0 && frame >= 4) right = v.turnTo;
    fishgfx::draw(b, fn, frame, wsx(v.x), wsy(v.y), k, right, S.time + v.wobble, c, v.mouthT < 0);
    if (S.selFish == f.id) {
        const Sprite* cs = gfx::spriteByName("fishselect_corner");
        float hw, hh; fishgfx::extents(b, hw, hh);
        float w = hw * k * 1.1f, h = hh * k * 1.2f;
        float cx = wsx(v.x), cy = wsy(v.y);
        if (cs) {
            float s = 0.45f;
            gfx::drawSprite(cs, cx - w, cy - h, s, s, 0, Color(), false);
            gfx::drawSprite(cs, cx + w, cy - h, s, s, 90, Color(), false);
            gfx::drawSprite(cs, cx + w, cy + h, s, s, 180, Color(), false);
            gfx::drawSprite(cs, cx - w, cy + h, s, s, 270, Color(), false);
        }
    }
    if (S.showInfo && f.alive()) {
        std::string st = f.infested == 2 ? "ICK" : f.infested == 1 ? "FUNGUS" : f.pregnant() ? "PREGNANT" : f.food < 20 ? "HUNGRY" : f.age < GS::ageUntilAdult ? "YOUNG" : "";
        if (!st.empty()) {
            gfx::TextStyle ts; ts.size = 13; ts.color = Color{1, 1, .6f, 1}; ts.outline = true; ts.outlineColor = Color{0, 0, 0, .8f};
            gfx::drawText(st, FRect{wsx(v.x) - 60, wsy(v.y) - 50, 120, 20}, ts);
        }
    }
}

// ------------------------------------------------------------------ particles
struct Particle { float x, y, vx, vy, life, max, size; int kind; };   // kind 0 bubble, 1 food, 2 coin
static std::vector<Particle> g_parts;
static void spawnBubble(float x, float y) { g_parts.push_back({x, y, rndf(-6, 6), rndf(60, 110), 0, rndf(3, 6), rndf(0.15f, 0.4f), 0}); }
static void spawnFood(float x) {
    for (int i = 0; i < 26; i++) g_parts.push_back({x + rndf(-120, 120), TANK_T + rndf(0, 60), rndf(-8, 8), -rndf(40, 90), 0, rndf(4, 7), rndf(3, 5), 1});
}
static void updateParticles(float dt) {
    for (auto& p : g_parts) {
        p.life += dt;
        p.x += p.vx * dt; p.y += p.vy * dt;
        if (p.kind == 0) p.x += sinf(p.life * 4 + p.size * 10) * 10 * dt;
        if (p.kind == 1 && p.y < TANK_B - 40) p.vy = 0;
    }
    g_parts.erase(std::remove_if(g_parts.begin(), g_parts.end(), [](const Particle& p) { return p.life > p.max || p.y > TANK_T + 200; }), g_parts.end());
}
static void drawParticles() {
    static const Sprite* bub = gfx::spriteByName("BubbleParticle");
    for (auto& p : g_parts) {
        float a = std::min(1.f, (p.max - p.life) * 1.5f);
        if (p.kind == 0 && bub) gfx::drawSprite(bub, wsx(p.x), wsy(p.y), p.size * WS * 2, p.size * WS * 2, 0, Color{1, 1, 1, .7f * a});
        else if (p.kind == 1) gfx::fillRect(FRect{wsx(p.x), wsy(p.y), p.size * WS, p.size * WS}, Color{.55f, .35f, .15f, a});
    }
}

// ------------------------------------------------------------------ tank drawing
static const char* THEMES[4] = {"CoralReef", "Atlantis", "MuddyRiver", "RockyShore"};
static const std::pair<const char*, const char*> UPGRADE_NODES[] = {
    {"AutoFeeder", "upgrade_auto feeder"}, {"PowerFilter", "upgrade_power filter"}, {"TemperatureRegulator", "upgrade_temperature regulator"},
    {"AerationSystem", "upgrade_aeration system"}, {"CleaningSnail", "upgrade_cleaningsnail"}, {"GoldenSeahorse", "upgrade_golden seahorse"},
    {"PlantOfCuring", "upgrade_plant of curing"}, {"PlantOfEggFertility", "upgrade_plant of egg fertility"},
    {"PlantOfFertility", "upgrade_plant of fertility"}, {"PlantOfNutrition", "upgrade_plant of nutrition"}};

static std::unordered_map<std::string, JV> g_itemPrefabs;
static std::unordered_map<std::string, WNode> g_itemW;
static void loadItems() {
    JV items = jload(S.dataDir + "/cfg/items.json");
    for (auto& it : items.a) {
        if (it.has("prefab")) g_itemW[it["id"].str()] = buildW(it["prefab"]);
    }
}
static const Sprite* firstSprite(const WNode& n) {
    if (n.sp) return n.sp;
    for (auto& k : n.kids) if (const Sprite* s = firstSprite(k)) return s;
    return nullptr;
}
static const Sprite* itemIcon(const std::string& id) {
    static std::unordered_map<std::string, const Sprite*> cache;
    auto c = cache.find(id);
    if (c != cache.end()) return c->second;
    const Sprite* s = nullptr;
    const catalog::Item* ci = catalog::item(id);
    if (ci && ci->tab == "decorations") {
        auto w = g_itemW.find(id);
        if (w != g_itemW.end()) s = firstSprite(w->second);
    }
    if (!s && id == "research_icon_environment") s = gfx::spriteByName("research_icon_environment0");
    if (!s) {
        static JV items = jload(S.dataDir + "/cfg/items.json");
        for (auto& it : items.a) if (it["id"].str() == id) s = gfx::sprite(it["icon"].str());
    }
    if (!s) s = gfx::spriteByName(ci ? ci->icon : id);
    cache[id] = s;
    return s;
}
const Sprite* iconFor(const std::string& id) { return itemIcon(id); }

static void drawTankWorld() {
    std::vector<WDraw> draws;
    // art
    for (auto& k : g_tank.kids) {
        if (k.name.rfind("Caustics", 0) == 0) continue;
        bool isTheme = false; int ti = -1;
        for (int i = 0; i < 4; i++) if (k.name == THEMES[i]) { isTheme = true; ti = i; }
        if (!isTheme || ti != G.tankTheme[S.tank]) continue;
        WNode art = k;
        for (auto& sub : art.kids) {
            if (sub.name == "HourlyReward" || sub.name == "VVO2CrossPromo" || sub.name == "Masks") sub.active = false;
            if (sub.name == "Upgrades")
                for (auto& u : sub.kids) {
                    u.active = false;
                    for (auto& un : UPGRADE_NODES) if (u.name == un.first) u.active = G.has(un.second);
                }
        }
        collectW(art, 0, 0, 0, 1, 1, draws);
    }
    // eggs & decorations in this tank
    for (auto& it : G.items) {
        if (it.tank != S.tank) continue;
        auto w = g_itemW.find(it.id);
        if (it.hatchAt > 0 && w != g_itemW.end()) {
            // egg clutch: small eggs scattered on the gravel, sized like real fish eggs next to the fish
            const Sprite* egg = nullptr;
            std::function<void(const WNode&)> findEgg = [&](const WNode& n) { if (!egg && n.sp) egg = n.sp; for (auto& k : n.kids) findEgg(k); };
            findEgg(w->second);
            if (egg) {
                unsigned seed = (unsigned)it.uid * 2654435761u;
                auto r01 = [&]() { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffff) / 65535.f; };
                for (int e = 0; e < 9; e++) {
                    float ex = it.x + (r01() - .5f) * 70, ey = it.y + (r01() - .5f) * 18;
                    float s = 0.28f + r01() * 0.08f;
                    // tiny wobble while incubating
                    float wob = sinf(S.time * 3 + e) * 1.5f;
                    draws.push_back({egg, ex, ey + wob, -3.f - ey * 0.001f, s, s, 0, Color{1, 1, 1, .95f}, false, 0, SDL_BLENDMODE_BLEND});
                }
            }
        } else if (w != g_itemW.end()) {
            WNode n = w->second; n.x = it.x; n.y = it.y; n.z = -3; n.sx = n.sy = it.scale;
            collectW(n, 0, 0, 0, 1, 1, draws);
        } else if (const Sprite* ic = itemIcon(it.id)) {
            draws.push_back({ic, it.x, it.y, -3, it.scale, it.scale, 0, Color(), false, 0, SDL_BLENDMODE_BLEND});
        }
    }
    std::stable_sort(draws.begin(), draws.end(), [](const WDraw& a, const WDraw& b) { return a.z > b.z; });
    // draw far-to-near with fish inserted at z = -20
    bool fishDrawn = false;
    auto drawFishes = [&]() {
        for (auto& f : G.fish) if (f.tank == S.tank && f.id != S.dragFish) { FishVis& v = visFor(f); drawTankFish(f, v, 1); }
        fishDrawn = true;
    };
    for (auto& d : draws) {
        if (!fishDrawn && d.z < -20) drawFishes();
        gfx::drawSprite(d.sp, wsx(d.x), wsy(d.y), d.sx * WS * 2, d.sy * WS * 2, -d.rz, d.c, d.flip, d.bm);
    }
    if (!fishDrawn) drawFishes();
    // caustics overlay
    for (auto& k : g_tank.kids) {
        if (k.name.rfind("Caustics", 0) != 0 || !k.sp) continue;
        float off = sinf(S.time * 0.3f + (k.name.size() > 8 ? 1.7f : 0)) * 60;
        Color c = k.c;
        gfx::drawSprite(k.sp, wsx(k.x + off), wsy(k.y), k.sx * WS * 2 * 4, k.sy * WS * 2 * 4, 0, c, false, SDL_BLENDMODE_ADD);
    }
    // egg timers
    for (auto& it : G.items) {
        if (it.tank != S.tank || it.hatchAt <= 0) continue;
        int left = (int)std::max(0.0, it.hatchAt - Game::unixNow());
        gfx::TextStyle ts; ts.size = 11; ts.color = Color{1, 1, 1, .9f}; ts.outline = true; ts.outlineColor = Color{0, 0, 0, .8f};
        char buf[16]; snprintf(buf, sizeof buf, "%d:%02d", left / 60, left % 60);
        gfx::drawText(buf, FRect{wsx(it.x) - 40, wsy(it.y) - 28, 80, 16}, ts);
    }
    drawParticles();
}

// ------------------------------------------------------------------ breeding tank (UI)
static FRect breedArea() {
    UINode* tl = N("GameView/BottomRight/BreedingTank/TopLeft");
    UINode* br = N("GameView/BottomRight/BreedingTank/BottomRight");
    if (!tl || !br) return FRect{};
    float x0 = tl->rect.x + tl->rect.w * .5f, y0 = tl->rect.y + tl->rect.h * .5f;
    float x1 = br->rect.x + br->rect.w * .5f, y1 = br->rect.y + br->rect.h * .5f;
    return FRect{x0, y0, x1 - x0, y1 - y0};
}
struct BVis { float x, y, t; bool right; };
static std::map<int, BVis> g_bvis;
static void drawBreedingFish(UINode*, FRect) {
    FRect a = breedArea();
    int i = 0;
    for (auto& f : G.fish) {
        if (f.tank != TANK_BREEDING || f.id == S.dragFish) continue;
        BVis& b = g_bvis[f.id];
        if (b.t == 0) { b.t = rndf(0, 6); b.right = (i % 2) == 0; }
        float px = a.x + a.w * (0.3f + 0.4f * (i % 2)) + sinf(S.time * 0.6f + b.t) * a.w * 0.15f;
        float py = a.y + a.h * 0.45f + cosf(S.time * 0.9f + b.t) * a.h * 0.15f;
        bool right = cosf(S.time * 0.6f + b.t) > 0;
        int bb, ff; artFor(f.body, f.fin, f.age, bb, ff);
        fishgfx::draw(bb, ff, 0, px, py, WS * 0.6f * fishScaleForAge(f.age), right, S.time + b.t, fishTint(f));
        b.x = px; b.y = py; b.right = right;
        i++;
    }
}

// ------------------------------------------------------------------ hit tests
static int fishAt(float sx, float sy) {
    float wx = swx(sx), wy = swy(sy);
    int best = -1; float bd = 1e9f;
    for (auto& f : G.fish) {
        if (f.tank != S.tank) continue;
        FishVis& v = visFor(f);
        int b, fn; artFor(f.body, f.fin, f.age, b, fn);
        float hw, hh; fishgfx::extents(b, hw, hh);
        float s = 0.8f * fishScaleForAge(f.age);
        hw *= s; hh *= s;
        float dx = fabsf(wx - v.x), dy = fabsf(wy - v.y);
        if (dx < std::max(hw, 45.f) && dy < std::max(hh, 35.f)) {
            float d = dx + dy;
            if (d < bd) { bd = d; best = f.id; }
        }
    }
    return best;
}
static int breedFishAt(float sx, float sy) {
    for (auto& kv : g_bvis) {
        Fish* f = G.get(kv.first);
        if (!f || f->tank != TANK_BREEDING) continue;
        if (fabsf(sx - kv.second.x) < 45 && fabsf(sy - kv.second.y) < 30) return kv.first;
    }
    return -1;
}
static int decoAt(float sx, float sy) {
    float wx = swx(sx), wy = swy(sy);
    for (int i = (int)G.items.size() - 1; i >= 0; i--) {
        auto& it = G.items[i];
        if (it.tank != S.tank) continue;
        if (fabsf(wx - it.x) < 70 && fabsf(wy - it.y) < 80) return it.uid;
    }
    return -1;
}

// drop targets: returns tank index or TANK_NONE
static int dropTarget(float x, float y) {
    struct T { const char* p; int t; } ts[] = {
        {"GameView/TopRight/TankWindow/WindowImage/Grid/FirstTankButton", 0},
        {"GameView/TopRight/TankWindow/WindowImage/Grid/SecondTankButton", 1},
        {"GameView/TopRight/TankWindow/WindowImage/Grid/ThirdTankButton", 2},
        {"GameView/TopRight/TankWindow/WindowImage/SellTankButton", TANK_SALES},
        {"GameView/TopRight/TankWindow/WindowImage/SellTank2Button", TANK_SALES2},
        {"GameView/BottomRight/BreedingTank", TANK_BREEDING},
        {"GameView/BottomLeft/FishStoreButton", TANK_SALES}};
    for (auto& t : ts) {
        UINode* n = N(t.p);
        if (n && n->visibleInTree() && n->rect.contains(x, y)) return t.t;
    }
    return TANK_NONE;
}

static void moveFishTo(int id, int tank) {
    Fish* f = G.get(id);
    if (!f) return;
    if (tank == f->tank) return;
    if (!G.tankUnlocked(tank)) { audio::play("SFX_Error"); showToast("That tank is locked. Buy it in Supplies > Upgrades."); return; }
    if (!f->alive()) { audio::play("SFX_Error"); showToast("This fish has died. Tap it to remove it."); return; }
    if ((tank == TANK_SALES || tank == TANK_SALES2) && !f->adult()) { audio::play("SFX_Error"); showToast("Only adult fish can be sold."); return; }
    if (tank == TANK_BREEDING && !f->adult()) { audio::play("SFX_Error"); showToast("Only adult fish can breed."); return; }
    if (tank == TANK_BREEDING && f->pregnant()) {
        int preg = 0; for (auto& o : G.fish) if (o.tank == TANK_BREEDING && o.pregnant()) preg++;
        if (preg >= 1) { audio::play("SFX_Error"); showToast("THIS FISH IS ALREADY PREGNANT."); return; }
    }
    if (G.countInTank(tank) >= G.capacity(tank)) { audio::play("SFX_Error"); showToast("That tank is full!"); return; }
    if (f->tank >= 0) S.homeTank[id] = f->tank;
    G.moveFish(id, tank);
    if (tank >= 0) { FishVis& v = S.vis[id]; v.init = false; }
    if (S.selFish == id && tank != S.tank) S.selFish = -1;
    audio::play("SFX_DropFish");
    if (tank == TANK_SALES || tank == TANK_SALES2) showToast("Fish moved to your store's selling tank.");
    refreshTankButtons();
    save();
}

// ------------------------------------------------------------------ input
static void beginPress(float x, float y) {
    S.pressX = x; S.pressY = y; S.pointerDown = true; S.dragging = false;
    S.dragFish = -1; S.dragDeco = -1;
    if (S.screen == SCR_GAME && !S.storeOpen && !S.speciesOpen && !S.tasksOpen && !S.optionsOpen && !S.alertOpen) {
        int bf = breedFishAt(x, y);
        if (bf >= 0) { S.dragFish = bf; return; }
    }
    if (ui::pointerDown(x, y)) return;
    if (S.screen != SCR_GAME) return;
    int f = fishAt(x, y);
    if (f >= 0) { S.dragFish = f; return; }
    int d = decoAt(x, y);
    if (d >= 0) { S.dragDeco = d; return; }
}
static void movePress(float x, float y) {
    S.dragX = x; S.dragY = y;
    if (!S.pointerDown) return;
    if (!S.dragItem.empty()) { S.dragging = true; return; }
    if (S.dragFish >= 0 || S.dragDeco >= 0) {
        if (!S.dragging && (fabsf(x - S.pressX) > 10 || fabsf(y - S.pressY) > 10)) {
            S.dragging = true;
            if (S.dragFish >= 0) audio::play("SFX_PickupFish");
        }
        if (S.dragging && S.dragDeco >= 0) {
            for (auto& it : G.items) if (it.uid == S.dragDeco) { it.x = std::max(TANK_L, std::min(TANK_R, swx(x))); it.y = std::max(TANK_B - 120, std::min(TANK_T, swy(y))); }
        }
        return;
    }
    ui::pointerMove(x, y);
}
static void endPress(float x, float y) {
    S.pointerDown = false;
    if (!S.dragItem.empty()) {
        std::string id = S.dragItem; S.dragItem.clear(); S.dragging = false;
        bool overUI = ui::hitAny(x, y);
        UINode* bt = N("GameView/BottomRight/BreedingTank");
        bool overBreed = bt && bt->visibleInTree() && bt->rect.contains(x, y);
        if (overBreed && id == "supplies_allurepotion") { G.useItem(id, TANK_BREEDING, 0, 0); audio::play("SFX_UseMedicine"); refreshInventory(); save(); return; }
        if (overUI) return;
        float wx = swx(x), wy = swy(y);
        if (id.rfind("supplies_eggs", 0) == 0) wy = std::min(wy, TANK_B + 40);
        bool ok = G.useItem(id, S.tank, wx, wy);
        if (ok) {
            if (id == "food") { audio::play("SFX_UseFood"); spawnFood(wx); }
            else if (id.rfind("supplies_eggs", 0) == 0) audio::play("SFX_UseEggs");
            else audio::play("SFX_UseMedicine");
            refreshInventory(); save();
        } else { audio::play("SFX_Error"); if (id != "food") showToast("That item can't be used here right now."); }
        return;
    }
    if (S.dragFish >= 0) {
        int id = S.dragFish; S.dragFish = -1;
        if (S.dragging) {
            S.dragging = false;
            int t = dropTarget(x, y);
            Fish* f = G.get(id);
            if (t != TANK_NONE) moveFishTo(id, t);
            else if (f && !ui::hitAny(x, y)) {
                if (f->tank != S.tank) moveFishTo(id, S.tank);
                FishVis& v = S.vis[id];
                v.init = true; v.x = std::max(TANK_L, std::min(TANK_R, swx(x))); v.y = std::max(TANK_B, std::min(TANK_T, swy(y)));
                v.vx = v.vy = 0; pickTarget(v);
                audio::play("SFX_DropFish");
            }
        } else {
            S.selFish = id;
            audio::play("SFX_Select_Bubble" + std::to_string(rnd(1, 12)));
            refreshFishInfo();
        }
        return;
    }
    if (S.dragDeco >= 0) {
        int uid = S.dragDeco; S.dragDeco = -1;
        if (S.dragging) { S.dragging = false; save(); return; }
        for (size_t i = 0; i < G.items.size(); i++) {
            if (G.items[i].uid != uid) continue;
            TankItem it = G.items[i];
            if (it.hatchAt > 0) return;   // eggs
            const catalog::Item* ci = catalog::item(it.id);
            alert("DECORATION", std::string("Return the ") + (ci ? ci->name : it.id) + " to your items?", "RETURN", [uid]() {
                for (size_t j = 0; j < G.items.size(); j++) if (G.items[j].uid == uid) { G.inventory[G.items[j].id]++; G.items.erase(G.items.begin() + j); break; }
                refreshInventory(); save();
            }, "KEEP", nullptr);
            return;
        }
        return;
    }
    bool wasUI = ui::pointerUp(x, y);
    if (!wasUI && S.screen == SCR_GAME && !S.dragging) {
        // tap on the glass: deselect, fish swim over
        S.selFish = -1;
        refreshFishInfo();
        float wx = swx(x), wy = swy(y);
        audio::play(rnd(0, 2) ? "SFX_Tapglass1" : "SFX_TapGlass2");
        for (int i = 0; i < 4; i++) spawnBubble(wx + rndf(-20, 20), wy + rndf(-10, 10));
        for (auto& f : G.fish) if (f.tank == S.tank && f.alive() && rnd(0, 100) < 60) {
            FishVis& v = visFor(f); v.tx = wx + rndf(-120, 120); v.ty = wy + rndf(-60, 60); v.rest = 0;
        }
    }
    S.dragging = false;
}

void startItemDrag(const std::string& id, float x, float y) {
    S.dragItem = id; S.dragX = x; S.dragY = y; S.dragging = true; S.pointerDown = true;
}

// ------------------------------------------------------------------ alerts
static std::function<void()> g_alertF1, g_alertF2;
void alert(const std::string& title, const std::string& text, const std::string& b1, std::function<void()> f1, const std::string& b2, std::function<void()> f2) {
    UINode* a = N("AlertScreen");
    if (!a) return;
    a->active = true; S.alertOpen = true;
    setText("AlertScreen/WindowHolder/TitleHolder/Title", title);
    show("AlertScreen/WindowHolder/CenteredHolder", true);
    show("AlertScreen/WindowHolder/WrappedHolder", false);
    show("AlertScreen/WindowHolder/FullHolder", false);
    setText("AlertScreen/WindowHolder/CenteredHolder/Text", text);
    g_alertF1 = f1; g_alertF2 = f2;
    UINode* bh = N("AlertScreen/WindowHolder/ButtonHolder");
    UINode* tbh = N("AlertScreen/WindowHolder/TextButtonHolder");
    if (bh) bh->active = false;
    if (tbh) {
        tbh->active = true;
        UINode* btn = tbh->child("Button1");
        UINode* btn2 = tbh->child("Button2");
        for (auto* k : tbh->kids) k->active = false;
        if (btn) {
            btn->active = true;
            if (UINode* t = btn->findDeep("Text")) t->text = b1;
            btn->onClick = [] { N("AlertScreen")->active = false; S.alertOpen = false; if (g_alertF1) { auto f = g_alertF1; g_alertF1 = nullptr; f(); } };
        }
        if (btn2 && !b2.empty()) {
            btn2->active = true;
            if (UINode* t = btn2->findDeep("Text")) t->text = b2;
            btn2->onClick = [] { N("AlertScreen")->active = false; S.alertOpen = false; if (g_alertF2) { auto f = g_alertF2; g_alertF2 = nullptr; f(); } };
        }
        for (auto* k : tbh->kids) { k->size[0] = 330; k->size[1] = 120; if (UINode* t = k->findDeep("Text")) ui::setRect(t, 0, 0, 1, 1, 0, 6, 0, 0); }
        ui::layout();
        ui::linearArrange(tbh);
    }
}

// ------------------------------------------------------------------ lifecycle
bool init(const std::string& dataDir, const std::string& saveDir) {
    S.dataDir = dataDir; S.saveDir = saveDir;
#ifdef __vita__
    sceIoMkdir(saveDir.c_str(), 0777);
#else
    mkdir(saveDir.c_str(), 0777);
#endif
    if (!gfx::init(dataDir)) return false;
    if (!ui::load(dataDir + "/cfg/ui.json", dataDir + "/cfg/uiprefabs.json")) return false;
    fishgfx::load(dataDir);
    audio::init(dataDir);
    ui::clickSound = [] { audio::play("SFX_TapButton1"); };
    g_tank = buildW(jload(dataDir + "/cfg/tank.json"));
    g_store = buildW(jload(dataDir + "/cfg/fishstore.json"));
    loadItems();
    G.loadTasks(dataDir + "/cfg/Tasks.txt");
    G.onEvent = [](const std::string& e) {
        if (e.rfind("species:", 0) == 0) S.newSpecies.push_back(e.substr(8));
        else if (e.rfind("magic:", 0) == 0) S.newMagic.push_back(atoi(e.c_str() + 6));
        else if (e == "dead") showToast("A fish has died...");
        else if (e == "tankfull") showToast("A baby was lost - the tank is full!");
        else if (e == "hatched") { audio::play("SFX_BubbleReveal"); showToast("Your eggs hatched!"); }
    };
    G.onFishBorn = [](int id) { S.vis[id].bornFlash = 1.5f; };
    if (!G.load(saveDir + "/save.json")) G.newGame();
    audio::setVolumes(G.musicVol, G.sfxVol);
    UINode* fr = N("GameView/BottomRight/BreedingTank/FishRoot");
    if (fr) fr->customDraw = drawBreedingFish;
    initScreens();
    audio::music(G.musicVol > 0);
    G.tick();
    return true;
}

void event(const SDL_Event& e) {
    switch (e.type) {
        case SDL_QUIT: S.quit = true; break;
        case SDL_MOUSEBUTTONDOWN: if (e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT) beginPress((float)e.button.x, (float)e.button.y); break;
        case SDL_MOUSEMOTION: if (e.motion.which != SDL_TOUCH_MOUSEID) movePress((float)e.motion.x, (float)e.motion.y); break;
        case SDL_MOUSEBUTTONUP: if (e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT) endPress((float)e.button.x, (float)e.button.y); break;
        case SDL_FINGERDOWN: S.cursorOn = false; if (e.tfinger.touchId == 0 || true) { if (e.tfinger.fingerId == 0 || !S.pointerDown) beginPress(e.tfinger.x * SCREEN_W, e.tfinger.y * SCREEN_H); } break;
        case SDL_FINGERMOTION: movePress(e.tfinger.x * SCREEN_W, e.tfinger.y * SCREEN_H); break;
        case SDL_FINGERUP: endPress(e.tfinger.x * SCREEN_W, e.tfinger.y * SCREEN_H); break;
        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_ESCAPE) { if (!backPressed()) S.quit = true; }
            break;
        case SDL_JOYAXISMOTION: {
            float v = e.jaxis.value / 32767.f;
            if (fabsf(v) < 0.18f) v = 0;   // dead zone
            if (e.jaxis.axis == 0) S.stickX = v;
            else if (e.jaxis.axis == 1) S.stickY = v;
            else if (e.jaxis.axis == 3) S.rstickY = v;
            if (v != 0 && (e.jaxis.axis == 0 || e.jaxis.axis == 1)) S.cursorOn = true;
            break;
        }
        case SDL_JOYBUTTONUP: {
            int b = e.jbutton.button;
            if (b == 2 && S.cursorDown) { S.cursorDown = false; endPress(S.curX, S.curY); }
            if (b >= 6 && b <= 9) S.dpad[b - 6] = false;
            break;
        }
        case SDL_JOYBUTTONDOWN: {
            int b = e.jbutton.button;
            // Vita: 0 triangle, 1 circle, 2 cross, 3 square, 4 L, 5 R, 6 down, 7 left, 8 up, 9 right, 10 select, 11 start
            if (b >= 6 && b <= 9) { S.dpad[b - 6] = true; S.cursorOn = true; break; }
            if (b == 2) { S.cursorOn = true; S.cursorDown = true; beginPress(S.curX, S.curY); break; }
            if (S.cursorDown) break;
            if (b == 10 && S.screen == SCR_MENU) { S.quitViaSelect = true; S.quit = true; break; }
            if (b == 1) { backPressed(); break; }
            if (b == 11) { if (S.screen == SCR_GAME && !S.optionsOpen) openOptions(); else backPressed(); break; }
            if (S.screen == SCR_GAME && !S.storeOpen && !S.speciesOpen && !S.tasksOpen && !S.optionsOpen && !S.alertOpen) {
                if (b == 4 || b == 5) {
                    int dir = b == 5 ? 1 : -1;
                    for (int k = 1; k <= 3; k++) { int t = (S.tank + dir * k + 3) % 3; if (G.tankUnlocked(t)) { S.tank = t; break; } }
                    S.selFish = -1; refreshTankButtons(); refreshFishInfo(); audio::play("SFX_TapButton2");
                } else if (b == 0) openStore("supplies");
                else if (b == 3) openSpecies(S.selFish);
                else if (b == 10) {
                    UINode* n = N("GameView/TopRight/ItemsButton"); if (n && n->onClick) n->onClick();
                }
            } else if (S.storeOpen && (b == 4 || b == 5)) {   // L/R switch store tabs
                static const char* tabs[5] = {"supplies", "bank", "upgrades", "research", "decorations"};
                static int ti = 0; ti = (ti + (b == 5 ? 1 : 4)) % 5; openStore(tabs[ti]);
            }
            break;
        }
        case SDL_APP_WILLENTERBACKGROUND: save(); break;
        default: break;
    }
}

static void updateBreeding(float dt) {
    std::vector<Fish*> bf;
    for (auto& f : G.fish) if (f.tank == TANK_BREEDING) bf.push_back(&f);
    UINode* timer = N("GameView/BottomRight/BreedingTank/Timer");
    if (bf.size() == 2 && bf[0]->alive() && bf[1]->alive() && !(bf[0]->pregnant() && bf[1]->pregnant())) {
        if (!S.breeding) {
            S.breeding = true; S.breedStart = S.time;
            S.breedDuration = Game::unixNow() < G.allureUntil ? 0 : GS::breedingDuration;
        }
        float el = S.time - S.breedStart;
        if (timer) {
            timer->active = true;
            float left = std::max(0.f, S.breedDuration - el);
            char buf[16]; snprintf(buf, sizeof buf, "0:%02d", (int)ceilf(left));
            setText("GameView/BottomRight/BreedingTank/Timer/TimerText", buf);
            if (UINode* mf = N("GameView/BottomRight/BreedingTank/Timer/MeterHolder/MeterFill")) { mf->fill = S.breedDuration > 0 ? el / S.breedDuration : 1; mf->imgType = 3; }
        }
        if (el >= S.breedDuration) {
            Fish* mother = bf[0]->pregnant() ? bf[1] : bf[0];
            Fish* father = mother == bf[0] ? bf[1] : bf[0];
            G.impregnate(*mother, *father);
            audio::play("SFX_Breeding");
            int mid = mother->id, fid = father->id;
            int mt = S.homeTank.count(mid) ? S.homeTank[mid] : S.tank;
            int ft = S.homeTank.count(fid) ? S.homeTank[fid] : S.tank;
            if (!G.tankUnlocked(mt) || G.countInTank(mt) >= G.capacity(mt)) mt = S.tank;
            if (!G.tankUnlocked(ft) || G.countInTank(ft) >= G.capacity(ft)) ft = S.tank;
            G.moveFish(fid, ft); G.moveFish(mid, mt);
            S.vis[mid].init = false; S.vis[fid].init = false;
            S.breeding = false;
            showToast("Your fish is pregnant! A baby will be born soon.");
            refreshTankButtons();
            save();
        }
    } else {
        S.breeding = false;
        if (timer) timer->active = false;
    }
    UINode* steam = N("GameView/BottomRight/BreedingTank/SteamImage");
    if (steam) steam->alphaMul = S.breeding ? 0.5f + 0.5f * sinf(S.time * 4) : 0;
}

static void updateCursor(float dt) {
    float dx = S.stickX, dy = S.stickY;
    if (S.dpad[1]) dx -= 1; if (S.dpad[3]) dx += 1; if (S.dpad[2]) dy -= 1; if (S.dpad[0]) dy += 1;
    if (dx == 0 && dy == 0) { S.curSpeed = 0; }
    else {
        S.curSpeed = std::min(1.f, S.curSpeed + dt * 2.5f);
        float sp = 260 + 520 * S.curSpeed;
        float mag = std::min(1.f, sqrtf(dx * dx + dy * dy));
        float nx = dx / std::max(0.001f, sqrtf(dx * dx + dy * dy)), ny = dy / std::max(0.001f, sqrtf(dx * dx + dy * dy));
        S.curX = std::max(0.f, std::min((float)SCREEN_W - 1, S.curX + nx * mag * mag * sp * dt));
        S.curY = std::max(0.f, std::min((float)SCREEN_H - 1, S.curY + ny * mag * mag * sp * dt));
        movePress(S.curX, S.curY);
    }
    // right stick scrolls lists by simulating a drag on the scroll area under the cursor
    if (S.rstickY != 0 && !S.cursorDown && ui::hitAny(S.curX, S.curY)) {
        if (!S.rscroll) { S.rscroll = true; S.rsY = S.curY; ui::pointerDown(S.curX, S.curY); }
        S.rsY -= S.rstickY * 600 * dt;
        ui::pointerMove(S.curX + (S.rsY - S.curY), S.rsY);
    } else if (S.rscroll) { S.rscroll = false; ui::pointerUp(-1000, -1000); }
}

void update(float dt) {
    S.time += dt;
    updateCursor(dt);
    if (S.toastT > 0) S.toastT -= dt;
    S.tickTimer -= dt;
    if (S.tickTimer <= 0) {
        S.tickTimer = 2.0f;
        G.tick();
        refreshTankButtons();
        if (S.selFish >= 0) refreshFishInfo();
    }
    S.saveTimer -= dt;
    if (S.saveTimer <= 0) { S.saveTimer = 30; if (G.dirty) save(); }
    if (S.screen == SCR_GAME) {
        for (auto& f : G.fish) if (f.tank == S.tank) updateFishVis(f, visFor(f), dt, f.id == S.dragFish && S.dragging);
        if (rnd(0, 100) < 6) spawnBubble(rndf(TANK_L, TANK_R), TANK_B - 60);
        updateBreeding(dt);
    }
    updateParticles(dt);
    if (S.screen == SCR_STORE) updateFishStore(dt);
    updateScreens(dt);
    refreshHUD();
}

void render() {
    SDL_SetRenderDrawColor(gfx::R, 8, 30, 60, 255);
    SDL_RenderClear(gfx::R);
    if (S.screen == SCR_GAME) drawTankWorld();
    else if (S.screen == SCR_STORE) drawFishStoreWorld();
    ui::layout();
    ui::draw();
    // dragged things on top
    if (S.dragging && S.dragFish >= 0) {
        Fish* f = G.get(S.dragFish);
        if (f) {
            int b, fn; artFor(f->body, f->fin, f->age, b, fn);
            fishgfx::draw(b, fn, 0, S.dragX, S.dragY, WS * 0.8f * fishScaleForAge(f->age), false, S.time * 3, Color{1, 1, 1, .9f});
        }
    }
    if (!S.dragItem.empty()) {
        const Sprite* ic = iconFor(S.dragItem);
        if (ic) { float k = std::min(70.f / std::max(1.f, ic->fw), 70.f / std::max(1.f, ic->fh)); gfx::drawSprite(ic, S.dragX, S.dragY, k, k, 0, Color{1, 1, 1, .85f}); }
    }
    if (S.cursorOn) {
        static const Sprite* hand = gfx::spriteByName("hand");
        float k = S.cursorDown ? 0.55f : 0.62f;
        if (hand) gfx::drawSprite(hand, S.curX + hand->fw * k * (hand->px - 0.3f), S.curY + hand->fh * k * (0.88f - hand->py) , k, k);
        else gfx::fillRect(FRect{S.curX - 4, S.curY - 4, 8, 8}, Color{1, 1, 1, 1});
    }
    if (S.toastT > 0 && !S.toast.empty()) {
        float a = std::min(1.f, S.toastT * 2);
        float w = 560, h = 44;
        FRect r{(SCREEN_W - w) * .5f, SCREEN_H * .40f, w, h};
        const Sprite* bg = gfx::spriteByName("messagebox_side");
        if (bg) gfx::drawSliced(bg, r, 0.5f, Color{1, 1, 1, .92f * a}); else gfx::fillRect(r, Color{0, .1f, .3f, .8f * a});
        gfx::TextStyle ts; ts.size = 17; ts.color = Color{1, 1, 1, a}; ts.outline = true; ts.outlineColor = Color{0, .1f, .3f, a};
        gfx::drawText(S.toast, FRect{r.x + 16, r.y, r.w - 32, r.h}, ts);
    }
    SDL_RenderPresent(gfx::R);
    gfx::frameEnd();
}

void shutdown() {
    save();
    audio::shutdown();
    gfx::shutdown();
}
bool wantsQuit() { return S.quit; }
bool quitViaSelect() { return S.quitViaSelect; }
std::string stateSummary() {
    char b[512];
    int alive = 0; for (auto& f : G.fish) if (f.alive()) alive++;
    snprintf(b, sizeof b, "screen=%d tank=%d coins=%d gems=%d fish=%d (alive %d) items=%d species=%d magic=%d taskWave=%d paused=%d playtime=%ds",
             (int)S.screen, S.tank, G.coins, G.gems, (int)G.fish.size(), alive, (int)G.items.size(), G.numDiscovered(), (int)G.magicDiscovered.size(), G.taskWave, (int)G.paused, G.now());
    return b;
}

void debugCommand(const std::string& cmd) {
    if (cmd == "play") { if (UINode* n = N("MainMenuView/PlayButton")) if (n->onClick) n->onClick(); }
    else if (cmd == "grow") { for (auto& f : G.fish) { f.age = std::max(f.age, 22); f.agesChecked = f.age; } G.tick(); }
    else if (cmd == "eggs") { G.inventory["supplies_eggs_common"]++; TankItem e; e.id = "supplies_eggs_common"; e.tank = S.tank; e.x = 0; e.y = -250; e.hatchAt = 1; e.uid = G.nextItemUid++; G.items.push_back(e); G.tick(); }
    else if (cmd == "rich") { G.coins += 100000; G.gems += 1000; }
    else if (cmd.rfind("store ", 0) == 0) openStore(cmd.substr(6));
    else if (cmd == "species") openSpecies(G.fish.empty() ? -1 : G.fish[0].id);
    else if (cmd == "tasks") openTasks();
    else if (cmd == "fishstore") enterFishStore();
    else if (cmd == "options") openOptions();
    else if (cmd == "select") { if (!G.fish.empty()) { S.selFish = G.fish[0].id; refreshFishInfo(); } }
    else if (cmd == "inv") { if (UINode* n = N("GameView/TopRight/ItemsButton")) if (n->onClick) n->onClick(); }
    else if (cmd == "sell") { int n = 0; for (auto& f : G.fish) if (f.adult() && f.alive() && f.tank >= 0 && n < 3) { f.tank = TANK_SALES; n++; } }
    else if (cmd == "breed") { int n = 0; for (auto& f : G.fish) if (f.adult() && f.alive() && f.tank >= 0 && n < 2) { f.tank = TANK_BREEDING; n++; } }
    else if (cmd == "alert") alert("TITLE", "Some alert text here.", "OK", nullptr, "CANCEL", nullptr);
    else if (cmd == "back") backPressed();
    else if (cmd == "placeeggs") { for (int i = 0; i < 2; i++) { TankItem e; e.id = i ? "supplies_eggs_rare" : "supplies_eggs_common"; e.tank = S.tank; e.x = -200 + i * 300; e.y = -240; e.hatchAt = Game::unixNow() + 60; e.uid = G.nextItemUid++; G.items.push_back(e); } }
    else if (cmd.rfind("cursor ", 0) == 0) { sscanf(cmd.c_str() + 7, "%f %f", &S.curX, &S.curY); S.cursorOn = true; }
    else if (cmd.rfind("dump ", 0) == 0) {
        ui::layout();
        std::function<void(UINode*, int)> d = [&](UINode* n, int dep) {
            SDL_Log("%*s%s%s rect=(%.0f,%.0f %.0fx%.0f) img=%d sp=%s txt=%s", dep * 2, "", n->name.c_str(), n->active ? "" : " [off]", n->rect.x, n->rect.y, n->rect.w, n->rect.h,
                    n->hasImg, n->sp ? n->sp->name.c_str() : "-", n->hasTxt ? n->text.substr(0, 20).c_str() : "");
            if (dep < 6) for (auto* k : n->kids) d(k, dep + 1);
        };
        if (UINode* n = N(cmd.substr(5))) d(n, 0);
    }
    else if (cmd.rfind("tap ", 0) == 0) { float x, y; sscanf(cmd.c_str() + 4, "%f %f", &x, &y); beginPress(x, y); endPress(x, y); }
}
}  // namespace app
