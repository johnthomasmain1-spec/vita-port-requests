#include "app_internal.h"
#include <cmath>
#include <algorithm>

namespace app {
struct Peep {
    int seed; bool female;
    float x, y;            // world
    int state;             // 0 walk in, 1 browsing, 2 to register, 3 leaving, 4 done
    float t;
    int thought = 0;       // 0 none, 1 sold, 2 price outrage, 3 sick fish
    float thoughtT = 0;
    int fishId = -1;
};
static std::vector<Peep> g_peeps;
static float g_spawnT = 2;
static int g_peepSeed = 1;
static int g_curCustomer = -1;
struct CoinFx { float x, y, t; int amount; };
static std::vector<CoinFx> g_coins;

// path points (world units) over the isometric store art
static const float DOOR[2] = {417, -175}, INSIDE[2] = {300, 0}, TANK_R[2] = {437, 83}, TANK_L[2] = {-556, 24}, REG[2] = {-119, 242}, EXIT[2] = {953, -353};
static const float FISH_R[2] = {470, 300}, FISH_L[2] = {-720, 150};

static std::vector<Fish*> forSale() {
    std::vector<Fish*> v;
    for (auto& f : G.fish) if (f.tank == TANK_SALES || f.tank == TANK_SALES2) v.push_back(&f);
    return v;
}

void refreshFishStoreUI() {
    UINode* grid = N("FishStoreView/BottomMiddle/Window/FishStoreFishWindow/Mask/FishGrid");
    UINode* mask = N("FishStoreView/BottomMiddle/Window/FishStoreFishWindow/Mask");
    auto list = forSale();
    setText("FishStoreView/BottomMiddle/Window/FishStoreFishWindow/FishForSaleAmountText", std::to_string(list.size()));
    if (!grid) return;
    while (!grid->kids.empty()) ui::destroy(grid->kids.back());
    for (Fish* f : list) {
        UINode* c = ui::instantiate("FishStoreFishItem", grid);
        if (!c) continue;
        int id = f->id;
        if (UINode* n = c->find("FishNameText")) n->text = catalog::fishName(f->body, f->fin);
        if (UINode* n = c->find("AskingPriceAmountText")) n->text = fmtNum(f->salePrice > 0 ? f->salePrice : G.price(f->body, f->fin));
        if (UINode* n = c->find("HealthMeter/MeterHolder/MeterFill")) { n->imgType = 3; n->fillMethod = 0; n->fill = f->health / (float)GS::maxHealth; }
        if (UINode* n = c->find("FedMeter/MeterHolder/MeterFill")) { n->imgType = 3; n->fillMethod = 0; n->fill = f->food / 100.f; }
        if (UINode* n = c->find("SoldImage")) n->active = false;
        if (UINode* fr = c->find("FishPictureHolder/FishRoot")) {
            int b = f->body, fn = f->fin, age = f->age;
            fr->customDraw = [b, fn, age](UINode*, FRect r) {
                FRect rr{r.x - r.w * .6f, r.y - r.h * .25f, r.w * 2.2f, r.h * 1.5f};
                drawFishInRect(b, fn, age, rr, S.time);
            };
        }
        c->raycast = true;
        c->onClick = [id]() {
            Fish* f = G.get(id); if (!f) return;
            std::string nm = catalog::fishName(f->body, f->fin);
            alert(nm, "Asking price: " + fmtNum(G.price(f->body, f->fin)) + " coins\nFish value: " + fmtNum(G.value(f->body, f->fin)) +
                      " coins\n\nCustomers happily pay up to 10% over value.",
                  "BACK TO TANK", [id]() {
                      Fish* f2 = G.get(id); if (!f2) return;
                      int t = S.homeTank.count(id) ? S.homeTank[id] : 0;
                      if (!G.tankUnlocked(t) || G.countInTank(t) >= G.capacity(t)) t = 0;
                      G.moveFish(id, t); S.vis[id].init = false; refreshFishStoreUI(); save();
                  },
                  "SET PRICE", [id]() { openSpecies(id); });
        };
    }
    float cw = 0, ch = 0;
    ui::gridArrange(grid, &cw, &ch);
    grid->size[0] = std::max(cw, 1.f);
    if (mask) {
        float viewW = mask->rect.w / std::max(.01f, mask->wsx);
        static float base = grid->apos[0];
        // grid's anchored centre is offset; align its left edge with the mask's left edge
        grid->apos[0] = cw * .5f - viewW * .5f;
        (void)base;
        mask->onScroll = [mask, grid, cw](float dx, float, int ph) {
            static float st = 0;
            if (ph == 0) st = grid->apos[0];
            float viewW = mask->rect.w / std::max(.01f, mask->wsx);
            float k = 1.f / std::max(.01f, mask->wsx);
            float lo = cw * .5f - viewW * .5f - std::max(0.f, cw - viewW), hi = cw * .5f - viewW * .5f;
            grid->apos[0] = std::max(lo, std::min(hi, st + dx * k));
        };
    }
}

static void setCustomerUI(const Peep* p) {
    UINode* v = N("FishStoreView/BottomMiddle/Window/FishStoreCustomerWindow/FishStoreCustomerView");
    if (!v) return;
    if (!p) {
        if (UINode* n = v->find("CustomerNameText")) n->text = "NO CUSTOMERS";
        if (UINode* n = v->find("CustomerProfessionText")) n->text = forSale().empty() ? "Drag adult fish to the sell tank!" : "Waiting for customers...";
        v->customDraw = nullptr;
        return;
    }
    if (UINode* n = v->find("CustomerNameText")) n->text = catalog::customerName(p->seed);
    if (UINode* n = v->find("CustomerProfessionText")) n->text = catalog::profession(p->seed);
    bool female = p->female;
    int th = p->thought;
    v->customDraw = [female, th](UINode* n, FRect r) {
        const Sprite* s = gfx::spriteByName(female ? "Woman" : "Man");
        if (s) {
            float k = r.h * 0.95f / std::max(1.f, s->fh);
            gfx::drawSprite(s, r.x + r.w * 0.18f, r.y + r.h * 0.5f, k, k);
        }
        const Sprite* ic = th == 1 ? gfx::spriteByName("icon_sold") : th == 2 ? gfx::spriteByName("icon_priceoutrage") : nullptr;
        if (ic) gfx::drawSprite(ic, r.x + r.w * 0.85f, r.y + r.h * 0.45f, 0.8f, 0.8f);
        (void)n;
    };
}

void enterFishStore() {
    S.screen = SCR_STORE;
    S.selFish = -1;
    show("GameView", false);
    show("FishStoreView", true);
    show("GameView/BottomMiddle/FishInfoPanel", false);
    g_peeps.clear(); g_spawnT = 1.5f; g_curCustomer = -1;
    refreshFishStoreUI();
    setCustomerUI(nullptr);
    audio::play("SFX_DoorOpen");
}
void leaveFishStore() {
    S.screen = SCR_GAME;
    show("FishStoreView", false);
    show("GameView", true);
    g_peeps.clear();
    refreshTankButtons(); refreshFishInfo();
    save();
}

static bool moveTo(Peep& p, const float* tgt, float speed, float dt) {
    float dx = tgt[0] - p.x, dy = tgt[1] - p.y, d = sqrtf(dx * dx + dy * dy);
    if (d < 6) { p.x = tgt[0]; p.y = tgt[1]; return true; }
    float s = std::min(d, speed * dt);
    p.x += dx / d * s; p.y += dy / d * s;
    return false;
}

static void tryBuy(Peep& p) {
    auto list = forSale();
    std::vector<Fish*> ok;
    for (Fish* f : list) if (f->alive() && f->adult()) ok.push_back(f);
    if (ok.empty()) { p.thought = 0; return; }
    int lowHealth = 0; for (Fish* f : ok) if (f->health < GS::maxHealth / 2) lowHealth++;
    int base = (int)ok.size() - lowHealth + (int)g_peeps.size() + (G.has("upgrade_golden seahorse") ? 10 : 0) + 20;
    for (int i = (int)ok.size() - 1; i > 0; i--) std::swap(ok[i], ok[rnd(0, i + 1)]);
    for (Fish* f : ok) {
        int price = f->salePrice > 0 ? f->salePrice : G.price(f->body, f->fin);
        float ratio = price / (float)std::max(1, G.value(f->body, f->fin));
        bool accept;
        if (ratio <= 1.1f) accept = true;
        else if (ratio <= 1.4f) accept = rnd(0, 100) <= 30;
        else if (ratio <= 1.9f) accept = rnd(0, 100) <= 10;
        else accept = false;
        if (!accept) { p.thought = 2; continue; }
        if (f->infested >= 1 && rnd(0, 100) < 21) { p.thought = 3; continue; }
        int chance = base - rnd(0, 3) + rnd(0, 3) * 8 + 30;
        if (rnd(0, 100) < chance) {
            p.fishId = f->id;
            p.thought = 1;
            return;
        }
    }
}

void updateFishStore(float dt) {
    float interval = 15.f / G.advertisingLevel();
    for (const char* a : {"upgrade_tv", "upgrade_bookshelf", "upgrade_vendingmachine", "upgrade_revealfountain"}) if (G.has(a)) interval *= 0.88f;
    g_spawnT -= dt;
    if (g_spawnT <= 0 && g_peeps.size() < 4 && !forSale().empty()) {
        g_spawnT = interval * rndf(0.7f, 1.3f);
        Peep p; p.seed = g_peepSeed++ * 7919 + rnd(0, 100000); p.female = (p.seed & 1) != 0;
        p.x = DOOR[0]; p.y = DOOR[1]; p.state = 0; p.t = 0;
        g_peeps.push_back(p);
        if (g_curCustomer < 0) g_curCustomer = 0;
    }
    bool changed = false;
    for (auto& p : g_peeps) {
        p.t += dt;
        if (p.thoughtT > 0) p.thoughtT -= dt;
        switch (p.state) {
            case 0: { const float* tank = (p.seed % 3 == 0 && G.tankUnlocked(TANK_SALES2)) ? TANK_L : TANK_R;
                      if (moveTo(p, p.t < 1.2f ? INSIDE : tank, 160, dt) && p.t >= 1.2f) { p.state = 1; p.t = 0; } break; }
            case 1:
                if (p.t > 2.5f) {
                    tryBuy(p);
                    p.thoughtT = 2.5f;
                    if (p.fishId >= 0) {
                        Fish* f = G.get(p.fishId);
                        if (f) {
                            int price = f->salePrice > 0 ? f->salePrice : G.price(f->body, f->fin);
                            G.sellFish(p.fishId, price);
                            g_coins.push_back({p.x, p.y + 120, 0, price});
                            audio::play("SFX_CashRegister");
                            changed = true;
                        }
                        p.state = 2;
                    } else {
                        if (p.thought == 2) audio::play("sfx_bonus_jeers1");
                        p.state = 3;
                    }
                    p.t = 0;
                }
                break;
            case 2: if (moveTo(p, REG, 150, dt)) { p.state = 3; p.t = 0; } break;
            case 3: if (moveTo(p, p.t < 1.0f ? DOOR : EXIT, 170, dt) && p.t > 1.0f) p.state = 4; break;
        }
    }
    g_peeps.erase(std::remove_if(g_peeps.begin(), g_peeps.end(), [](const Peep& p) { return p.state == 4; }), g_peeps.end());
    for (auto& c : g_coins) c.t += dt;
    g_coins.erase(std::remove_if(g_coins.begin(), g_coins.end(), [](const CoinFx& c) { return c.t > 1.6f; }), g_coins.end());
    // the customer window shows the customer currently browsing/deciding
    const Peep* show_ = nullptr;
    for (auto& p : g_peeps) if (p.state >= 1 && p.state <= 2) { show_ = &p; break; }
    if (!show_ && !g_peeps.empty()) show_ = &g_peeps.front();
    setCustomerUI(show_);
    if (changed) { refreshFishStoreUI(); save(); }
}

void drawFishStoreWorld() {
    std::vector<WDraw> draws;
    for (auto& k : g_store.kids) {
        if (k.name == "PSDRoot") {
            WNode r = k;
            for (auto& c : r.kids) {
                if (c.name == "Tank2Cover") c.active = !G.tankUnlocked(TANK_SALES2);
                if (c.name == "Tank3Cover") c.active = true;
            }
            collectW(r, 0, 0, 0, 1, 1, draws);
        }
    }
    // fish in the selling tanks: small swimming sprites at the tank area
    std::stable_sort(draws.begin(), draws.end(), [](const WDraw& a, const WDraw& b) { return a.z > b.z; });
    for (auto& d : draws) gfx::drawSprite(d.sp, wsx(d.x), wsy(d.y), d.sx * WS * 2, d.sy * WS * 2, -d.rz, d.c, d.flip, d.bm);
    auto list = forSale();
    int i = 0;
    for (Fish* f : list) {
        const float* base = (f->tank == TANK_SALES2) ? FISH_L : FISH_R;
        float px = base[0] + (i % 4) * 45 + sinf(S.time * .7f + i) * 14;
        float py = base[1] - (i / 4) * 30 + cosf(S.time * .9f + i * 2) * 6;
        int b, fn; artFor(f->body, f->fin, f->age, b, fn);
        fishgfx::draw(b, fn, 0, wsx(px), wsy(py), WS * 0.22f, sinf(S.time * .7f + i) > 0, S.time + i);
        i++;
    }
    // customers (sorted by y: higher y = further back)
    std::vector<const Peep*> ps; for (auto& p : g_peeps) ps.push_back(&p);
    std::sort(ps.begin(), ps.end(), [](const Peep* a, const Peep* b) { return a->y > b->y; });
    for (const Peep* p : ps) {
        const Sprite* s = gfx::spriteByName(p->female ? "Woman" : "Man");
        if (!s) continue;
        float bob = (p->state == 0 || p->state >= 2) ? fabsf(sinf(p->t * 8)) * 4 : 0;
        float k = WS * 0.42f * 2;
        gfx::drawSprite(s, wsx(p->x), wsy(p->y) - bob - s->fh * k * 0.5f, k, k, 0, Color(), p->state >= 2);
        if (p->thoughtT > 0 && p->thought) {
            const Sprite* bub = gfx::spriteByName("ThoughtBubble");
            const Sprite* ic = p->thought == 1 ? gfx::spriteByName("icon_sold") : p->thought == 2 ? gfx::spriteByName("icon_priceoutrage") : gfx::spriteByName("icon_attention");
            float tx = wsx(p->x) + 30, ty = wsy(p->y) - s->fh * k - 20;
            if (bub) gfx::drawSprite(bub, tx, ty, 1.2f, 1.2f);
            if (ic) gfx::drawSprite(ic, tx, ty - 4, 0.7f, 0.7f);
        }
    }
    const Sprite* coin = gfx::spriteByName("coin_hud");
    for (auto& c : g_coins) {
        float a = std::max(0.f, 1 - c.t / 1.6f);
        float y = wsy(c.y) - c.t * 60;
        if (coin) gfx::drawSprite(coin, wsx(c.x) - 22, y, 0.7f, 0.7f, 0, Color{1, 1, 1, a});
        gfx::TextStyle ts; ts.size = 20; ts.color = Color{1, .95f, .3f, a}; ts.outline = true; ts.outlineColor = Color{.3f, .15f, 0, a}; ts.align = gfx::ML;
        gfx::drawText("+" + fmtNum(c.amount), FRect{wsx(c.x) - 4, y - 14, 140, 28}, ts);
    }
}
}  // namespace app
