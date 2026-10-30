#include "app_internal.h"
#include <cmath>
#include <algorithm>

namespace app {
void startItemDrag(const std::string& id, float x, float y);
const Sprite* iconFor(const std::string& id);

static void hideRec(UINode* n, const std::string& prefix) {
    if (!n) return;
    for (auto* k : n->kids) {
        if (k->name.rfind(prefix, 0) == 0) k->active = false;
        else hideRec(k, prefix);
    }
}
static UINode* C(UINode* n, const std::string& path) { return n ? n->find(path) : nullptr; }
static void ctext(UINode* n, const std::string& path, const std::string& t) { if (UINode* c = C(n, path)) c->text = t; }
static void cshow(UINode* n, const std::string& path, bool v) { if (UINode* c = C(n, path)) c->active = v; }
static void clearKids(UINode* n) { if (!n) return; while (!n->kids.empty()) ui::destroy(n->kids.back()); }

// horizontal/vertical scrolling for a grid inside a mask
struct Scroller { UINode* content = nullptr; UINode* view = nullptr; bool horiz = true; float pos = 0, start = 0, contentLen = 0, baseApos = 0; };
static std::map<UINode*, Scroller> g_scrollers;
static void setupScroll(UINode* mask, UINode* content, bool horiz) {
    if (!mask || !content) return;
    Scroller& s = g_scrollers[mask];
    if (!s.content) s.baseApos = horiz ? content->apos[0] : content->apos[1];
    s.content = content; s.view = mask; s.horiz = horiz;
    mask->onScroll = [mask](float dx, float dy, int ph) {
        Scroller& s = g_scrollers[mask];
        if (ph == 0) s.start = s.pos;
        float k = 1.f / std::max(0.01f, s.view->wsx);
        s.pos = s.start - (s.horiz ? dx : -dy) * k * (s.horiz ? 1 : -1);
        float viewLen = s.horiz ? s.view->rect.w / std::max(.01f, s.view->wsx) : s.view->rect.h / std::max(.01f, s.view->wsy);
        float maxp = std::max(0.f, s.contentLen - viewLen);
        s.pos = std::max(0.f, std::min(maxp, s.pos));
        if (s.horiz) s.content->apos[0] = s.baseApos + (s.contentLen - viewLen) * .5f * (s.contentLen > viewLen ? 1 : 0) - s.pos;
        else s.content->apos[1] = s.pos;
    };
}
static void arrangeScroll(UINode* mask, UINode* grid, bool horiz) {
    float cw = 0, ch = 0;
    ui::gridArrange(grid, &cw, &ch);
    Scroller& s = g_scrollers[mask];
    setupScroll(mask, grid, horiz);
    if (horiz) {
        float viewLen = mask->size[0] > 0 ? mask->size[0] : grid->size[0];
        s.contentLen = cw;
        grid->size[0] = std::max(cw, 1.f);
        s.pos = 0;
        grid->apos[0] = s.baseApos + (cw > viewLen ? (cw - viewLen) * .5f : 0);
        // grid anchor is center: children laid out from the grid's top-left
    } else {
        s.contentLen = ch;
        s.pos = 0; grid->apos[1] = 0;
    }
    (void)ch;
}

// ------------------------------------------------------------------ HUD
void refreshHUD() {
    setText("CommonGameView/TopLeft/CoinWindow/WindowImage/CurrencyHolder/CoinAmountText", fmtNum(G.coins));
    setText("CommonGameView/TopLeft/GemWindow/WindowImage/CurrencyHolder/GemAmountText", fmtNum(G.gems));
    show("CommonGameView/TopLeft/PausedIndicator", G.paused);
    // allure potion
    double left = G.allureUntil - Game::unixNow();
    show("CommonUI/TopLeft/AllurePotionDisplay", left > 0 && S.screen == SCR_GAME);
    if (left > 0) { char b[16]; snprintf(b, sizeof b, "%d:%02d", (int)left / 60, (int)left % 60); setText("CommonUI/TopLeft/AllurePotionDisplay/Text", b); }
    // task button state
    bool anyReady = false, allDone = G.taskWave >= (int)G.waves.size();
    if (!allDone) {
        auto& w = G.waves[G.taskWave];
        for (int i = 0; i < 3 && i < (int)w.tasks.size(); i++) if (!G.taskCollected[i] && G.taskProgress[i] >= w.tasks[i].need) anyReady = true;
        bool allCol = true; for (int i = 0; i < 3 && i < (int)w.tasks.size(); i++) if (!G.taskCollected[i]) allCol = false;
        if (allCol) anyReady = true;
    }
    show("CommonUI/TopLeft/TaskButton/TasksSeenRoot", !anyReady && !allDone);
    show("CommonUI/TopLeft/TaskButton/TaskCompleteRoot", anyReady);
    show("CommonUI/TopLeft/TaskButton/AllTasksCompleteRoot", allDone);
}

// ------------------------------------------------------------------ tank buttons
static const char* TANKBTN[3] = {"GameView/TopRight/TankWindow/WindowImage/Grid/FirstTankButton",
                                 "GameView/TopRight/TankWindow/WindowImage/Grid/SecondTankButton",
                                 "GameView/TopRight/TankWindow/WindowImage/Grid/ThirdTankButton"};
void refreshTankButtons() {
    for (int t = 0; t < 3; t++) {
        UINode* b = N(TANKBTN[t]);
        if (!b) continue;
        bool un = G.tankUnlocked(t);
        cshow(b, "LockedImage", !un);
        cshow(b, "SelectedIndicator", t == S.tank && S.screen == SCR_GAME);
        cshow(b, "Text", un);
        ctext(b, "Text", std::to_string(t + 1));
        ctext(b, "TankCapacityText", un ? std::to_string(G.countInTank(t)) + "/" + std::to_string(G.capacity(t)) : "");
    }
    UINode* s1 = N("GameView/TopRight/TankWindow/WindowImage/SellTankButton");
    if (s1) { ctext(s1, "TankCapacityText", std::to_string(G.countInTank(TANK_SALES)) + "/" + std::to_string(G.capacity(TANK_SALES))); cshow(s1, "SelectedIndicator", false); }
    UINode* s2 = N("GameView/TopRight/TankWindow/WindowImage/SellTank2Button");
    if (s2) {
        bool un = G.tankUnlocked(TANK_SALES2);
        cshow(s2, "LockedImage", !un); cshow(s2, "SelectedIndicator", false);
        ctext(s2, "TankCapacityText", un ? std::to_string(G.countInTank(TANK_SALES2)) + "/" + std::to_string(G.capacity(TANK_SALES2)) : "");
    }
}

// ------------------------------------------------------------------ inventory
static const std::vector<std::string> INV_ORDER = {
    "food", "supplies_eggs_common", "supplies_eggs_uncommon", "supplies_eggs_rare", "supplies_eggs_legendary",
    "supplies_medicine_generic", "supplies_icktreatment", "supplies_fungaltreatment", "supplies_growthhormone",
    "supplies_gestationpotion", "supplies_allurepotion", "supplies_hatchingformula"};
void refreshInventory() {
    UINode* grid = N("GameView/TopRight/InventoryWindow/Mask/InventoryItemsRoot");
    UINode* mask = N("GameView/TopRight/InventoryWindow/Mask");
    if (!grid) return;
    clearKids(grid);
    std::vector<std::string> ids;
    for (auto& id : INV_ORDER) if (id == "food" || G.inventory.count(id)) ids.push_back(id);
    for (auto& kv : G.inventory) if (std::find(ids.begin(), ids.end(), kv.first) == ids.end() && kv.second > 0) ids.push_back(kv.first);
    for (auto& id : ids) {
        UINode* cell = ui::instantiate("InventoryItem", grid);
        if (!cell) continue;
        if (UINode* im = C(cell, "InventoryItemImage")) { im->sp = iconFor(id); im->preserve = true; }
        cshow(cell, "UseButton", false);
        int cnt = id == "food" ? -1 : G.inventory[id];
        const catalog::Item* ci = catalog::item(id);
        int d = ci ? ci->doses : 1;
        std::string q = cnt < 0 ? "" : std::to_string(cnt);
        if (d > 1 && G.doses.count(id) && G.doses[id] > 0) q += "*";
        cshow(cell, "QuantityHolder", cnt >= 0);
        ctext(cell, "QuantityHolder/InventoryItemImage", q);
        ctext(cell, "ItemNameText", id == "food" ? "FOOD" : ci ? ci->name : id);
        cshow(cell, "ItemNameText", false);
        cell->raycast = true;
        std::string cid = id;
        cell->onPress = [cid](UINode*, float x, float y) { startItemDrag(cid, x, y); };
    }
    float cw, ch; ui::gridArrange(grid, &cw, &ch);
    if (mask) setupScroll(mask, grid, false), g_scrollers[mask].contentLen = ch;
}
static void setInventoryOpen(bool open) {
    S.invOpen = open;
    UINode* w = N("GameView/TopRight/InventoryWindow");
    static float baseY = w ? w->apos[1] : 0;
    if (w) { w->active = true; w->apos[1] = open ? baseY - 768 : baseY; }
    if (open) refreshInventory();
}

// ------------------------------------------------------------------ fish info
static std::string ageText(const Fish& f) {
    if (!f.alive()) return "DEAD";
    if (f.age < GS::ageUntilTeen) return "BABY";
    if (f.age < GS::ageUntilAdult) return "JUVENILE";
    return "ADULT (" + std::to_string(f.age) + ")";
}
static std::string statusText(const Fish& f) {
    if (!f.alive()) return "DEAD";
    if (f.infested == 2) return "SICK (ICK)";
    if (f.infested == 1) return "SICK (FUNGAL)";
    if (f.pregnant()) return "PREGNANT";
    if (f.food <= 10) return "STARVING";
    if (f.food < 35) return "HUNGRY";
    if (f.health < GS::maxHealth / 3) return "WEAK";
    return "HEALTHY";
}
void refreshFishInfo() {
    UINode* panel = N("GameView/BottomMiddle/FishInfoPanel");
    if (!panel) return;
    Fish* f = S.selFish >= 0 ? G.get(S.selFish) : nullptr;
    if (f && f->tank != S.tank) { S.selFish = -1; f = nullptr; }
    panel->active = f != nullptr && S.screen == SCR_GAME;
    if (!f) return;
    int mk = Game::magicKind(f->body, f->fin);
    cshow(panel, "MagicFishInfo", false);
    UINode* p = C(panel, "NormalFishInfo/WindowImage");
    cshow(panel, "NormalFishInfo", true);
    if (!p) return;
    bool disc = G.discovered(f->body, f->fin) || f->adult();
    ctext(p, "FishNameText", disc ? catalog::fishName(f->body, f->fin) : "UNDISCOVERED");
    ctext(p, "StatusText", statusText(*f));
    ctext(p, "AgeText", ageText(*f));
    ctext(p, "RarityText", mk >= 0 ? "MAGIC" : catalog::rarityName(Game::rarity(f->body, f->fin)));
    UINode* pt = nullptr;
    for (auto* k : p->kids) if (k->name == "PriceText") { pt = k; break; }
    if (pt) pt->text = fmtNum(G.price(f->body, f->fin));
    // second price text next to the coin: hide
    int n = 0; for (auto* k : p->kids) if (k->name == "PriceText") { if (n++ == 1) k->active = false; }
    cshow(p, "CoinImage", false);
    if (UINode* mf = C(p, "HealthMeter/MeterHolder/MeterFill")) { mf->imgType = 3; mf->fillMethod = 0; mf->fill = f->health / (float)GS::maxHealth; }
    if (UINode* mf = C(p, "FedMeter/MeterHolder/MeterFill")) { mf->imgType = 3; mf->fillMethod = 0; mf->fill = f->food / 100.f; }
    cshow(p, "AutopsyButton", !f->alive());
    cshow(p, "DeleteImage", true); cshow(p, "DeleteText", true);
}
static void cycleFish(int dir) {
    std::vector<int> ids; for (auto& f : G.fish) if (f.tank == S.tank) ids.push_back(f.id);
    if (ids.empty()) return;
    auto it = std::find(ids.begin(), ids.end(), S.selFish);
    int i = it == ids.end() ? 0 : (int)(it - ids.begin()) + dir;
    S.selFish = ids[(i + (int)ids.size()) % ids.size()];
    refreshFishInfo();
}
static std::string damageReport(const Fish& f) {
    std::string r;
    if (f.envDamage > 0) r += "<color=#E3521CFF>Environmental</color> effects caused this fish to lose health\n";
    if (f.starveDamage > 0) r += "<color=#FF24DEFF>This fish suffered from starvation</color> and lost health as a result\n";
    if (f.overfedDamage > 0) r += "<color=#4F7BFFFF>This fish was overfed</color> and lost health as a result\n";
    if (f.ickDamage > 0) r += "<color=#38D94AFF>This fish got sick with ick</color> and lost health as a result\n";
    if (f.fungalDamage > 0) r += "<color=#C062FFFF>This fish caught fungus</color> and lost health as a result\n";
    if (r.empty()) r = "This fish died of old age.";
    return r;
}

// ------------------------------------------------------------------ supply store
static std::string g_storeTab = "supplies";
static const char* TAB_BTN[5][2] = {{"supplies", "SupplyStore/Bottom/SuppliesButton"}, {"bank", "SupplyStore/Bottom/BankButton"},
                                    {"upgrades", "SupplyStore/Bottom/UpgradesButton"}, {"research", "SupplyStore/Bottom/ResearchButton"},
                                    {"decorations", "SupplyStore/Bottom/DecorationsButton"}};
static const char* TAB_ROOT[5] = {"SupplyStore/SuppliesRoot", "SupplyStore/BankRoot", "SupplyStore/UpgradesRoot", "SupplyStore/ResearchRoot", "SupplyStore/DecorationsRoot"};

static void fillStoreTab();
static void buyItem(const std::string& id) {
    const catalog::Item* it = catalog::item(id);
    if (!it) return;
    if (it->permanent && G.has(id)) return;
    if (!it->prereq.empty() && !G.has(it->prereq)) { audio::play("SFX_Error"); showToast("Research the previous level first."); return; }
    bool gem = it->currency == "GM";
    if ((gem ? G.gems : G.coins) < it->cost) {
        audio::play("SFX_Error");
        alert("NOT ENOUGH " + std::string(gem ? "GEMS" : "COINS"), std::string("You need ") + fmtNum(it->cost) + (gem ? " gems" : " coins") + " to buy " + it->name + ".");
        return;
    }
    if (G.buy(id)) {
        audio::play(gem ? "SFX_UseGems" : "SFX_UseCoins");
        if (it->tab == "research") alert("RESEARCH COMPLETE!", std::string("You Researched:\n") + it->name);
        else showToast(std::string(it->name) + (it->permanent ? " installed!" : " added to My Items."));
        fillStoreTab();
        refreshTankButtons();
        save();
    }
}
static void setCurrencyIcon(UINode* btn, bool gem) {
    if (UINode* ci = C(btn, "CurrencyImage")) { const Sprite* sp = gfx::spriteByName(gem ? "gem_hud" : "coin_button"); if (sp) ci->sp = sp; ci->preserve = true; }
}
static void fillStoreTab() {
    int ti = 0; for (int i = 0; i < 5; i++) if (g_storeTab == TAB_BTN[i][0]) ti = i;
    for (int i = 0; i < 5; i++) {
        show(TAB_ROOT[i], i == ti);
        if (UINode* b = N(TAB_BTN[i][1])) cshow(b, "SelectedImage", i == ti);
    }
    UINode* root = N(TAB_ROOT[ti]);
    UINode* mask = C(root, "Mask");
    UINode* grid = mask && !mask->kids.empty() ? mask->kids[0] : nullptr;
    if (!grid) return;
    clearKids(grid);
    cshow(root, "OfferwallButton", false);
    UINode* desc = C(root, "ItemDescriptionText");
    if (desc) desc->text = "Select an item to view its description";
    std::string prefab = g_storeTab == "research" ? "ResearchItem" : g_storeTab == "bank" ? "BankItem" : g_storeTab == "decorations" ? "StoreItemSmall" : "UpgradeItem";
    std::vector<const catalog::Item*> list;
    if (g_storeTab == "research") {
        // show the next level of each research track
        const char* tracks[3][3] = {{"research_icon_environment", "research_icon_environment2", "research_icon_environment3"},
                                    {"research_icon_food", "research_icon_food2", "research_icon_food3"},
                                    {"research_icon_advertizing", "research_icon_advertizing2", "research_icon_advertizing3"}};
        for (auto& t : tracks) {
            const catalog::Item* pick = catalog::item(t[2]);
            for (auto* id : t) if (!G.has(id)) { pick = catalog::item(id); break; }
            list.push_back(pick);
        }
    } else for (auto& it : catalog::all()) if (it.tab == g_storeTab) list.push_back(&it);
    for (auto* it : list) {
        UINode* cell = ui::instantiate(prefab, grid);
        if (!cell) continue;
        std::string id = it->id;
        bool owned = it->permanent && G.has(id);
        bool locked = !it->prereq.empty() && !G.has(it->prereq);
        if (UINode* im = C(cell, "ItemImage")) { im->sp = iconFor(id); im->preserve = true; }
        ctext(cell, "ItemNameText", it->name);
        cshow(cell, "BannerImage", false); cshow(cell, "AnimatedBadge", false); cshow(cell, "BonusHolder", false); cshow(cell, "PlaceButton", false);
        UINode* buy = C(cell, "BuyButton");
        if (buy) {
            setCurrencyIcon(buy, it->currency == "GM");
            UINode* amt = C(buy, "AmountText");
            if (amt) amt->text = owned ? "OWNED" : fmtNum(it->cost);
            if (owned) cshow(buy, "CurrencyImage", false);
            buy->interactable = !owned;
            buy->onClick = [id]() { buyItem(id); };
            if (g_storeTab == "bank" && amt) amt->text = std::string(it->currency == "GM" ? "" : "") + fmtNum(it->cost) + (it->currency == "GM" ? " GEMS" : " COINS");
        }
        if (g_storeTab == "decorations") {
            int have = G.inventory.count(id) ? G.inventory[id] : 0;
            for (auto& ti2 : G.items) if (ti2.id == id) have++;
            if (UINode* nmN = C(cell, "ItemNameText")) nmN->text = have > 0 ? std::string(it->name) + " (" + std::to_string(have) + ")" : it->name;
        }
        cell->raycast = true;
        std::string d = it->desc, nm = it->name;
        UINode* descN = desc;
        UINode* nameN = C(root, "ItemNameText");
        cell->onClick = [descN, nameN, d, nm]() { if (descN) descN->text = d; if (nameN) nameN->text = nm; };
    }
    arrangeScroll(mask, grid, true);
}
void openStore(const std::string& tab) {
    g_storeTab = tab;
    S.storeOpen = true;
    show("SupplyStore", true);
    fillStoreTab();
    audio::play("SFX_DoorOpen");
}
void closeStore() { S.storeOpen = false; show("SupplyStore", false); refreshInventory(); }

// ------------------------------------------------------------------ species section
static int g_specFish = -1;
static int g_findexBody = 0;
static std::string g_specTab = "species";
static void drawSpecFish(UINode* root, int body, int fin, bool known) {
    UINode* fr = C(root, "FishRoot");
    if (!fr) return;
    fr->customDraw = [body, fin, known](UINode* n, FRect r) {
        if (body < 0) return;
        FRect rr{r.x - r.w * 0.6f, r.y - r.h * 0.3f, r.w * 2.2f, r.h * 1.6f};
        int b = body, f = fin; if (Game::magicKind(b, f) == MAGIC_GOLDENGUPPY) b = f = 900;
        float hw, hh; fishgfx::extents(b, hw, hh);
        float k = std::min(rr.w / (hw * 2.2f), rr.h / (hh * 2.4f));
        fishgfx::draw(b, f, 0, rr.x + rr.w * .5f, rr.y + rr.h * .5f, k, false, S.time, known ? Color() : Color{0, 0, 0, .75f});
    };
}
static void setSpeciesItem(UINode* it, Anc a, bool selected) {
    if (!it) return;
    bool known = a.body >= 0 && G.discovered(a.body, a.fin);
    int r = a.body >= 0 ? Game::rarity(a.body, a.fin) : 0;
    int mk = a.body >= 0 ? Game::magicKind(a.body, a.fin) : -1;
    ctext(it, "Banner/BannerText", a.body < 0 ? "" : mk >= 0 ? "MAGIC" : catalog::rarityName(r));
    if (UINode* bn = C(it, "Banner")) { bn->active = a.body >= 0; bn->sp = gfx::spriteByName(r == 2 ? "banner_rare" : r == 1 ? "banner_uncommon" : "banner_common") ?: bn->sp; }
    ctext(it, "FishNameText", a.body < 0 ? "" : (known || selected) ? catalog::fishName(a.body, a.fin) : "UNDISCOVERED");
    cshow(it, "SelectedText", selected);
    drawSpecFish(it, a.body, a.fin, true);
}
static void fillSpecies() {
    const char* tabs[3][3] = {{"species", "SpeciesSection/Top/SpeciesButton", "SpeciesSection/SpeciesView"},
                              {"findex", "SpeciesSection/Top/FindexButton", "SpeciesSection/FindexView"},
                              {"magic", "SpeciesSection/Top/MagicFishButton", "SpeciesSection/MagicFishView"}};
    for (auto& t : tabs) {
        bool on = g_specTab == t[0];
        show(t[2], on);
        if (UINode* b = N(t[1])) { cshow(b, "SelectedImage", on); for (auto* k : b->kids) if (k->name.rfind("NormalImage", 0) == 0) k->active = !on; }
    }
    if (g_specTab == "species") {
        Fish* f = g_specFish >= 0 ? G.get(g_specFish) : nullptr;
        show("SpeciesSection/SpeciesView/SpeciesRoot", f != nullptr);
        show("SpeciesSection/SpeciesView/SelectFishRoot", f == nullptr);
        if (!f) return;
        UINode* sr = N("SpeciesSection/SpeciesView/SpeciesRoot");
        setSpeciesItem(C(sr, "SelectedFish"), Anc{f->body, f->fin}, true);
        setSpeciesItem(C(sr, "Parent1"), f->p1, false);
        setSpeciesItem(C(sr, "Parent2"), f->p2, false);
        setSpeciesItem(C(sr, "Grandparent1"), f->g1, false);
        setSpeciesItem(C(sr, "Grandparent2"), f->g2, false);
        setSpeciesItem(C(sr, "Grandparent3"), f->g3, false);
        setSpeciesItem(C(sr, "Grandparent4"), f->g4, false);
        UINode* ps = C(sr, "PriceSetter");
        bool disc = G.discovered(f->body, f->fin);
        if (ps) {
            ps->active = disc;
            ctext(ps, "PriceText", fmtNum(G.price(f->body, f->fin)));
            int key = f->body * 21 + f->fin;
            auto mod = [key](int dir) {
                int v = G.breedPrice[key];
                int step = std::max(1, v / 20);
                G.breedPrice[key] = std::max(1, std::min(999999, v + dir * step));
                G.dirty = true; fillSpecies();
            };
            if (UINode* d = C(ps, "DecreasePriceButton")) { d->onClick = [mod]() { mod(-1); }; d->button = true; }
            if (UINode* u = C(ps, "IncreasePriceButton")) { u->onClick = [mod]() { mod(1); }; u->button = true; }
            if (UINode* df = C(ps, "DefaultPriceButton")) df->onClick = [key]() { G.breedPrice[key] = G.value(key / 21, key % 21); G.dirty = true; fillSpecies(); };
        }
    } else if (g_specTab == "findex") {
        setText("SpeciesSection/FindexView/Stats/SpeciesDiscoveredAmount", std::to_string(G.numDiscovered()) + "/441");
        setText("SpeciesSection/FindexView/Stats/CoinsEarnedAmount", fmtNum(G.coinsEarned));
        setText("SpeciesSection/FindexView/Stats/FishCuredAmount", fmtNum(G.fishCured));
        setText("SpeciesSection/FindexView/Stats/FishSoldAmount", fmtNum(G.fishSold));
        UINode* cg = N("SpeciesSection/FindexView/Mask/CategoryGrid");
        UINode* ig = N("SpeciesSection/FindexView/Mask/ItemGrid");
        UINode* mask = N("SpeciesSection/FindexView/Mask");
        if (!cg || !ig) return;
        clearKids(cg); clearKids(ig);
        for (int b = 0; b < 21; b++) {
            UINode* c = ui::instantiate("UIFindexCategoryItem", cg);
            if (!c) continue;
            int n = 0; for (int f = 0; f < 21; f++) if (G.discovered(b, f)) n++;
            ctext(c, "BodyNameText", catalog::partName(b));
            ctext(c, "QuantityText", std::to_string(n) + "/21");
            c->raycast = true;
            c->tintMul = b == g_findexBody ? Color{1, 1, .7f, 1} : Color();
            c->onClick = [b]() { g_findexBody = b; fillSpecies(); };
        }
        for (int f = 0; f < 21; f++) {
            UINode* c = ui::instantiate("UIFindexItem", ig);
            if (!c) continue;
            bool d = G.discovered(g_findexBody, f);
            ctext(c, "FishNameText", d ? catalog::fishName(g_findexBody, f) : "UNDISCOVERED");
            ctext(c, "StatusText", d ? catalog::rarityName(Game::rarity(g_findexBody, f)) : "");
            drawSpecFish(c, g_findexBody, f, d);
        }
        // category row on top, items below: both scroll together horizontally
        arrangeScroll(mask, ig, true);
        float cw; ui::gridArrange(cg, &cw);
        cg->size[0] = cw; cg->apos[0] = cw > 1920 ? (cw - 1920) * .5f : 0;
        if (mask) {
            Scroller& s = g_scrollers[mask];
            mask->onScroll = [mask, cg, ig](float dx, float, int ph) {
                Scroller& s = g_scrollers[mask];
                static float st = 0, cst = 0, ist = 0;
                if (ph == 0) { st = s.pos; cst = cg->apos[0]; ist = ig->apos[0]; }
                float k = 1.f / std::max(.01f, mask->wsx);
                cg->apos[0] = cst + dx * k;
                ig->apos[0] = ist + dx * k;
                (void)st;
            };
            (void)s;
        }
    } else {
        UINode* g = N("SpeciesSection/MagicFishView/Mask/ItemGrid");
        UINode* mask = N("SpeciesSection/MagicFishView/Mask");
        if (!g) return;
        clearKids(g);
        setText("SpeciesSection/MagicFishView/MagicFishDiscoveredAmountText", std::to_string(G.magicDiscovered.size()) + "/7");
        setText("SpeciesSection/MagicFishView/MagicFishDescriptionText", "SELECT A FISH TO VIEW DESCRIPTION");
        for (int k = 0; k < MAGIC_COUNT; k++) {
            UINode* c = ui::instantiate("UIMagicFishViewItem", g);
            if (!c) continue;
            bool d = G.magicDiscovered.count(k) > 0;
            ctext(c, "FishNameText", catalog::magicName(k));
            ctext(c, "SpeciesNameText", d ? catalog::fishName(magicBody[k], magicFin[k]) : "???");
            drawSpecFish(c, magicBody[k], magicFin[k], d);
            c->raycast = true;
            c->onClick = [k, d]() {
                std::string t = std::string(catalog::magicDesc(k));
                if (!d) t = "UNDISCOVERED - breed a " + catalog::partName(magicBody[k]) + " body with " + catalog::partName(magicFin[k]) + " fins to find it!";
                setText("SpeciesSection/MagicFishView/MagicFishDescriptionText", t);
            };
        }
        arrangeScroll(mask, g, true);
    }
}
void openSpecies(int fishId) {
    g_specFish = fishId;
    g_specTab = fishId >= 0 ? "species" : "findex";
    S.speciesOpen = true;
    show("SpeciesSection", true);
    fillSpecies();
}
void closeSpecies() { S.speciesOpen = false; show("SpeciesSection", false); refreshFishInfo(); }

// ------------------------------------------------------------------ tasks
static std::string taskText(const Task& t) {
    std::string s = t.text;
    size_t p = s.find("##"); if (p != std::string::npos) s.replace(p, 2, std::to_string(t.need));
    p = s.find("_item_");
    if (p != std::string::npos) { const catalog::Item* it = catalog::item(t.param); s.replace(p, 6, it ? it->name : t.param); }
    return s;
}
static void rewardIcon(UINode* img, const std::string& reward, UINode* amtText) {
    size_t a = reward.find('^'), b = reward.find('^', a + 1);
    std::string what = a == std::string::npos ? "" : reward.substr(a + 1, b - a - 1);
    int amt = b == std::string::npos ? 1 : atoi(reward.c_str() + b + 1);
    if (img) { img->sp = what == "CN" ? gfx::spriteByName("coin_hud") : what == "GM" ? gfx::spriteByName("gem_hud") : iconFor(what); img->preserve = true; }
    if (amtText) amtText->text = std::to_string(amt);
}
static void fillTasks() {
    bool done = G.taskWave >= (int)G.waves.size();
    show("Tasks/TasksAvailableHolder", !done);
    show("Tasks/TasksCompleteHolder", done);
    show("Tasks/TipCenter", false);
    if (done) return;
    auto& w = G.waves[G.taskWave];
    const char* items[3] = {"Tasks/TasksAvailableHolder/TaskItemsHolder/TaskItem1", "Tasks/TasksAvailableHolder/TaskItemsHolder/TaskItem2", "Tasks/TasksAvailableHolder/TaskItemsHolder/TaskItem3"};
    bool allCollected = true;
    for (int i = 0; i < 3; i++) {
        UINode* it = N(items[i]);
        if (!it) continue;
        if (i >= (int)w.tasks.size()) { it->active = false; continue; }
        it->active = true;
        const Task& t = w.tasks[i];
        bool complete = G.taskProgress[i] >= t.need;
        ctext(it, "TaskText", taskText(t));
        ctext(it, "ProgressText", G.taskCollected[i] ? "" : std::to_string(G.taskProgress[i]) + "/" + std::to_string(t.need));
        cshow(it, "CollectButton", complete && !G.taskCollected[i]);
        cshow(it, "ProgressText", !complete);
        cshow(it, "CheckmarkImage", G.taskCollected[i]);
        cshow(it, "TipButton", !G.taskCollected[i] && !t.tip.empty());
        cshow(it, "RewardRoot", !G.taskCollected[i]);
        rewardIcon(C(it, "RewardRoot/RewardItemImage"), t.reward, C(it, "RewardRoot/RewardAmountText"));
        if (!G.taskCollected[i]) allCollected = false;
        if (UINode* cb = C(it, "CollectButton")) cb->onClick = [i]() {
            auto& w2 = G.waves[G.taskWave];
            if (G.taskCollected[i]) return;
            G.giveReward(w2.tasks[i].reward);
            G.taskCollected[i] = true;
            audio::play("SFX_BubbleCollect");
            refreshInventory(); fillTasks(); save();
        };
        std::string tip = t.tip, cmd = t.tipCmd;
        if (UINode* tb = C(it, "TipButton")) tb->onClick = [tip, cmd]() {
            show("Tasks/TipCenter", true);
            setText("Tasks/TipCenter/WindowHolder/ContentText", tip);
            if (UINode* go = N("Tasks/TipCenter/WindowHolder/GoThereButton")) {
                go->active = !cmd.empty();
                go->onClick = [cmd]() {
                    show("Tasks/TipCenter", false); closeTasks();
                    size_t p = cmd.find('^');
                    std::string what = p == std::string::npos ? "" : cmd.substr(p + 1);
                    const catalog::Item* ci = catalog::item(what);
                    openStore(ci ? ci->tab : "supplies");
                };
            }
        };
    }
    show("Tasks/TasksAvailableHolder/ChestArea/CollectableRoot", allCollected);
    show("Tasks/TasksAvailableHolder/ChestArea/NotCollectableRoot", !allCollected);
    if (UINode* oc = N("Tasks/TasksAvailableHolder/ChestArea/CollectableRoot/OpenChestButton")) oc->onClick = []() {
        auto& w2 = G.waves[G.taskWave];
        std::string r = w2.chestReward;
        G.giveReward(r);
        audio::play("DailyRewardCollect");
        G.taskWave++;
        for (int i = 0; i < 3; i++) { G.taskProgress[i] = 0; G.taskCollected[i] = false; }
        size_t a = r.find('^'), b = r.find('^', a + 1);
        std::string what = a == std::string::npos ? "" : r.substr(a + 1, b - a - 1);
        int amt = b == std::string::npos ? 1 : atoi(r.c_str() + b + 1);
        const catalog::Item* ci = catalog::item(what);
        std::string nm = what == "CN" ? "coins" : what == "GM" ? "gems" : ci ? ci->name : what;
        alert("CHEST OPENED!", "You received " + std::to_string(amt) + " " + nm + "!");
        refreshInventory(); fillTasks(); save();
    };
    // chest images
    UINode* ch = N("Tasks/TasksAvailableHolder/ChestArea/ChestHolder/ItemHolder/CurrentChestImage");
    if (ch) { std::string t = w.chest; ch->sp = gfx::spriteByName(t == "gold" ? "reward(chest_ready_gold)" : t == "silver" ? "reward(chest_ready_silver)" : "reward(chest_ready)") ?: ch->sp; }
}
void openTasks() { S.tasksOpen = true; show("Tasks", true); fillTasks(); }
void closeTasks() { S.tasksOpen = false; show("Tasks", false); }

// ------------------------------------------------------------------ options
static void fillOptions() {
    UINode* o = N("MenuController/OptionsMenu/WindowHolder/Holder");
    if (!o) return;
    cshow(o, "MusicHolder/Toggle/Checkmark", audio::musicVolume() > 0);
    cshow(o, "SoundHolder/Toggle/Checkmark", audio::sfxVolume() > 0);
    ctext(o, "ButtonsHolder/Pause/Text", G.paused ? "Resume" : "Pause");
    ctext(o, "ButtonsHolder/ResetGame/Text", "Reset");
    if (UINode* p = C(o, "ButtonsHolder/Pause")) ui::setRect(p, .5f, .5f, .5f, .5f, -150, 10, 260, 100);
    if (UINode* r = C(o, "ButtonsHolder/ResetGame")) ui::setRect(r, .5f, .5f, .5f, .5f, 150, 10, 260, 100);
}
void openOptions() {
    S.optionsOpen = true;
    show("MenuController", true);
    show("MenuController/OptionsMenu", true);
    fillOptions();
}
static void closeOptions() { S.optionsOpen = false; show("MenuController", false); }

// ------------------------------------------------------------------ discoveries
void showDiscoveries() {
    if (S.alertOpen) return;
    if (!S.newMagic.empty()) {
        int k = S.newMagic.front(); S.newMagic.erase(S.newMagic.begin());
        show("FishDiscoveryCenter", true);
        show("FishDiscoveryCenter/NormalFishDiscovered", false);
        show("FishDiscoveryCenter/MagicFishDiscovered", true);
        UINode* w = N("FishDiscoveryCenter/MagicFishDiscovered/WindowHolder");
        ctext(w, "Title", "YOU HAVE DISCOVERED THE");
        ctext(w, "FishName", catalog::magicName(k));
        ctext(w, "Description", catalog::magicDesc(k));
        cshow(w, "ButtonHolder-FishDiscovery/ShareButtonHolder", false);
        if (UINode* fr = C(w, "FishRoot")) {
            int b = magicBody[k], f = magicFin[k];
            fr->customDraw = [b, f](UINode*, FRect r) {
                int bb = b, ff = f; if (Game::magicKind(b, f) == MAGIC_GOLDENGUPPY) bb = ff = 900;
                fishgfx::draw(bb, ff, 0, r.x + r.w * .5f, r.y + r.h * .5f, 0.9f, false, S.time);
            };
        }
        audio::play("SFX_DiscoverMagicFish");
        if (UINode* ok = C(w, "ButtonHolder-FishDiscovery/OkButtonHolder/OkButton")) ok->onClick = []() { show("FishDiscoveryCenter", false); showDiscoveries(); };
        return;
    }
    if (!S.newSpecies.empty()) {
        std::vector<std::string> list = S.newSpecies; S.newSpecies.clear();
        show("FishDiscoveryCenter", true);
        show("FishDiscoveryCenter/MagicFishDiscovered", false);
        show("FishDiscoveryCenter/NormalFishDiscovered", true);
        UINode* w = N("FishDiscoveryCenter/NormalFishDiscovered/WindowHolder");
        ctext(w, "TitleHolder/AmountText", list.size() == 1 ? "A NEW SPECIES!" : std::to_string(list.size()) + " NEW SPECIES!");
        cshow(w, "ButtonHolder-FishDiscovery/ShareButtonHolder", false);
        UINode* grid = C(w, "TitleHolder/Mask/Grid");
        UINode* mask = C(w, "TitleHolder/Mask");
        if (grid) {
            clearKids(grid);
            for (auto& s : list) {
                int key = atoi(s.c_str());
                UINode* c = ui::instantiate("UINormalFishDiscoveredItem", grid);
                if (!c) continue;
                ctext(c, "FishNameText", catalog::fishName(key / 21, key % 21));
                ctext(c, "StatusText", catalog::rarityName(Game::rarity(key / 21, key % 21)));
                cshow(c, "Image", false);
                drawSpecFish(c, key / 21, key % 21, true);
            }
            arrangeScroll(mask, grid, true);
        }
        audio::play("SFX_BubbleHappySting");
        if (UINode* ok = C(w, "ButtonHolder-FishDiscovery/OkButtonHolder/OkButton")) ok->onClick = []() { show("FishDiscoveryCenter", false); showDiscoveries(); };
    }
}

// ------------------------------------------------------------------ screens
static void enterGame() {
    S.screen = SCR_GAME;
    show("MainMenuView", false);
    show("GameView", true);
    show("CommonGameView", true);
    show("CommonUI", true);
    show("FishStoreView", false);
    audio::play("SFX_PlayButton");
    refreshTankButtons(); refreshInventory(); refreshFishInfo();
    setInventoryOpen(false);
    if (!G.triggers.count("intro")) {
        G.triggers.insert("intro");
        alert("WELCOME TO ISOLA!", "Let's get started with some eggs!\nOpen <color=#7DFF26FF>MY ITEMS</color> and drag the eggs into your tank.\nFeed your fish, breed adults in the breeding tank and sell them in your store!");
    }
}

void initScreens() {
    for (auto* k : ui::root->kids) k->active = false;
    show("MainMenuView", true);
    // hide things that need online services or are not part of this port
    for (const char* p : {"MainMenuView/FacebookButton", "MainMenuView/ResetGame", "MainMenuView/AchievementButton",
                          "CommonUI/TopLeft/CoinAdReward", "CommonUI/TopLeft/GemAdReward", "CommonUI/TopLeft/DailyRewardsButton",
                          "CommonUI/TopLeft/ResearchReminder", "CommonUI/TopLeft/SpecialOfferButton", "CommonUI/TopLeft/BannerFish",
                          "CommonUI/TopLeft/BannerPlane", "GameView/BottomLeft/FullscreenButton", "GameView/BottomMiddle/RescalePanel",
                          "GameView/SelectedFishCanvas", "GameView/SelectedObjectCanvas", "GameView/TopRight/InventoryWindow/Scrollbar",
                          "FishStoreView/TopRight/ExitFishStoreAfterConstruction", "FishStoreView/TopRight/ExitArea/ConstructionMessage",
                          "FishStoreView/Center/RepairAttractionPopup", "MenuController/FacebookOverrideMenu", "MenuController/GameDeleter",
                          "MenuController/OptionsMenu/WindowHolder/GooglePlayRoot", "MenuController/OptionsMenu/WindowHolder/Holder/ButtonsHolder/GetHelp",
                          "MenuController/OptionsMenu/WindowHolder/Holder/ButtonsHolder/Privacy_Eula", "MenuController/OptionsMenu/WindowHolder/Holder/ButtonsHolder/FacebookLogin",
                          "MenuController/OptionsMenu/WindowHolder/Holder/ButtonsHolder/FacebookLogout", "MenuController/OptionsMenu/WindowHolder/Holder/InfoHolder/UUID",
                          "MenuController/InputBlocker", "FishStoreView/BottomMiddle/Window/FishStoreCustomerWindow/FishStoreCustomerView/MaleCustomer",
                          "FishStoreView/BottomMiddle/Window/FishStoreCustomerWindow/FishStoreCustomerView/FemaleCustomer"})
        show(p, false);
    show("MenuController/OptionsMenu/WindowHolder/Holder/ButtonsHolder/ResetGame", true);
    // static layout groups: lay out once (Unity drives these at runtime)
    ui::layout();
    std::function<void(UINode*)> arrangeAll = [&](UINode* n) { if (n->grid && !n->kids.empty()) ui::gridArrange(n); if (n->linear && !n->kids.empty()) { ui::linearArrange(n); ui::layout(); } for (auto* k : n->kids) arrangeAll(k); };
    arrangeAll(ui::root);
    for (UINode* bg : {N("MainMenuView/Background")}) if (bg) ui::setRect(bg, 0, 0, 1, 1, 0, 0, 0, 0);
    hideRec(N("GameView"), "Tutorial");
    hideRec(N("FishStoreView"), "Tutorial");
    show("GameView/BottomMiddle/FishInfoPanel", false);
    show("GameView/BottomRight/BreedingTank/Timer", false);
    for (auto* n : {N("GameView/BottomRight/BreedingTank/SteamParticleSystem"), N("GameView/BottomRight/BreedingTank/HeartParticleSystem")}) if (n) n->active = false;
    setText("MenuController/OptionsMenu/WindowHolder/Holder/InfoHolder/Version", "Version: Vita port 1.0 (game data 1.11.6)");

    onClick("MainMenuView/PlayButton", enterGame);
    onClick("MainMenuView/SettingsButton", openOptions);
    onClick("CommonGameView/TopLeft/CoinWindow/WindowImage", []() { openStore("bank"); });
    onClick("CommonGameView/TopLeft/GemWindow/WindowImage", []() { openStore("bank"); });
    for (const char* p : {"CommonUI/TopLeft/TaskButton/TasksSeenRoot/Button", "CommonUI/TopLeft/TaskButton/TaskCompleteRoot/Button", "CommonUI/TopLeft/TaskButton/AllTasksCompleteRoot/Button"})
        onClick(p, openTasks);
    onClick("GameView/BottomLeft/FishStoreButton", enterFishStore);
    onClick("GameView/BottomLeft/SpeciesButton", []() { openSpecies(S.selFish); });
    onClick("GameView/BottomLeft/SuppliesButton", []() { openStore("supplies"); });
    onClick("GameView/BottomLeft/MenuButton", openOptions);
    onClick("GameView/BottomLeft/InfoButton", []() { S.showInfo = !S.showInfo; showToast(S.showInfo ? "Fish status labels ON" : "Fish status labels OFF"); });
    onClick("GameView/TopRight/ItemsButton", []() { setInventoryOpen(!S.invOpen); });
    onClick("GameView/TopRight/TankWindow/WindowImage/TanksLabel", []() {});
    for (int t = 0; t < 3; t++)
        onClick(TANKBTN[t], [t]() {
            if (!G.tankUnlocked(t)) { alert("TANK LOCKED", "Buy the tank upgrade in Supplies > Upgrades to unlock this tank.", "SUPPLIES", []() { openStore("upgrades"); }, "CLOSE", nullptr); return; }
            S.tank = t; S.selFish = -1; refreshTankButtons(); refreshFishInfo(); audio::play("SFX_TapButton2");
        });
    onClick("GameView/TopRight/TankWindow/WindowImage/SellTankButton", enterFishStore);
    onClick("GameView/TopRight/TankWindow/WindowImage/SellTank2Button", []() {
        if (!G.tankUnlocked(TANK_SALES2)) { alert("LOCKED", "Buy the Second Selling Tank in Supplies > Upgrades.", "SUPPLIES", []() { openStore("upgrades"); }, "CLOSE", nullptr); return; }
        enterFishStore();
    });
    // fish info panel
    UINode* info = N("GameView/BottomMiddle/FishInfoPanel");
    if (info) {
        if (UINode* l = C(info, "LeftButton")) { l->onClick = []() { cycleFish(-1); }; l->button = true; }
        if (UINode* r = C(info, "RightButton")) { r->onClick = []() { cycleFish(1); }; r->button = true; }
        UINode* p = C(info, "NormalFishInfo/WindowImage");
        if (UINode* d = C(p, "DeleteImage")) { d->button = true; d->onClick = []() {
            Fish* f = G.get(S.selFish); if (!f) return;
            int id = f->id;
            alert("DELETE FISH", "Remove this fish from your tank? This cannot be undone.", "DELETE", [id]() { G.removeFish(id); S.selFish = -1; refreshFishInfo(); refreshTankButtons(); save(); }, "CANCEL", nullptr);
        }; }
        if (UINode* a = C(p, "AutopsyButton")) a->onClick = []() {
            Fish* f = G.get(S.selFish); if (!f) return;
            int id = f->id;
            alert("AUTOPSY", damageReport(*f), "REMOVE", [id]() { G.removeFish(id); S.selFish = -1; refreshFishInfo(); refreshTankButtons(); audio::play("SFX_ToiletFlush"); save(); }, "CLOSE", nullptr);
        };
        if (UINode* n = C(p, "FishNameText")) { n->raycast = true; n->onClick = []() { if (S.selFish >= 0) openSpecies(S.selFish); }; }
    }
    // supply store
    onClick("SupplyStore/Top/CloseButton", closeStore);
    for (auto& t : TAB_BTN) { std::string tab = t[0]; onClick(t[1], [tab]() { g_storeTab = tab; fillStoreTab(); }); }
    // species
    onClick("SpeciesSection/Top/CloseButton", closeSpecies);
    for (const char* p : {"SpeciesSection/Top/SpeciesButton/NormalImage"}) onClick(p, []() { g_specTab = "species"; fillSpecies(); });
    onClick("SpeciesSection/Top/FindexButton/NormalImage", []() { g_specTab = "findex"; fillSpecies(); });
    onClick("SpeciesSection/Top/MagicFishButton/NormalImage (1)", []() { g_specTab = "magic"; fillSpecies(); });
    // tasks
    onClick("Tasks/TasksAvailableHolder/CloseButton", closeTasks);
    onClick("Tasks/TasksCompleteHolder/CloseButton", closeTasks);
    onClick("Tasks/TipCenter/WindowHolder/CloseButton", []() { show("Tasks/TipCenter", false); });
    onClick("Tasks/TipCenter/Background", []() { show("Tasks/TipCenter", false); });
    // options
    onClick("MenuController/OptionsMenu/WindowHolder/Holder/CloseButton", closeOptions);
    onClick("MenuController/OptionsMenu/WindowHolder/Holder/MusicHolder/Toggle", []() {
        float v = audio::musicVolume() > 0 ? 0 : 0.6f; audio::setVolumes(v, audio::sfxVolume()); audio::music(v > 0); fillOptions(); save(); });
    onClick("MenuController/OptionsMenu/WindowHolder/Holder/SoundHolder/Toggle", []() {
        float v = audio::sfxVolume() > 0 ? 0 : 0.8f; audio::setVolumes(audio::musicVolume(), v); fillOptions(); save(); });
    onClick("MenuController/OptionsMenu/WindowHolder/Holder/ButtonsHolder/Pause", []() {
        G.setPaused(!G.paused);
        if (!G.paused) { long long d = G.now() - G.pausedAt; if (d > 0) { G.startEpoch += d; } G.tick(); }
        fillOptions(); save();
        showToast(G.paused ? "Time is stopped - your fish won't age." : "Time is running again.");
    });
    onClick("MenuController/OptionsMenu/WindowHolder/Holder/ButtonsHolder/ResetGame", []() {
        alert("RESET PROFILE", "Delete all progress and start over?", "RESET", []() {
            G.newGame(); S.vis.clear(); S.selFish = -1; S.tank = 0; save(); closeOptions(); refreshTankButtons(); refreshInventory(); refreshFishInfo();
        }, "CANCEL", nullptr);
    });
    // fish store
    onClick("FishStoreView/TopRight/ExitArea/ExitButton", leaveFishStore);
    refreshTankButtons();
}

void updateScreens(float) {
    if (S.screen == SCR_GAME && !S.alertOpen && (!S.newSpecies.empty() || !S.newMagic.empty())) {
        UINode* fd = N("FishDiscoveryCenter");
        if (fd && !fd->active) showDiscoveries();
    }
    if (S.tasksOpen) { static float t = 0; t += 0.016f; if (t > 1) { t = 0; } }
}

bool backPressed() {
    if (S.alertOpen) { show("AlertScreen", false); S.alertOpen = false; return true; }
    if (UINode* fd = N("FishDiscoveryCenter")) if (fd->active) { fd->active = false; return true; }
    if (S.optionsOpen) { closeOptions(); return true; }
    if (S.storeOpen) { closeStore(); return true; }
    if (S.speciesOpen) { closeSpecies(); return true; }
    if (S.tasksOpen) { closeTasks(); return true; }
    if (S.screen == SCR_STORE) { leaveFishStore(); return true; }
    if (S.invOpen) { setInventoryOpen(false); return true; }
    if (S.selFish >= 0) { S.selFish = -1; refreshFishInfo(); return true; }
    if (S.screen == SCR_GAME) { openOptions(); return true; }
    return false;
}
}  // namespace app
