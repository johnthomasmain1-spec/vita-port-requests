// Fish Tycoon 2 simulation – re-implemented from the original game logic (see NOTES.md)
#include "game.h"
#include "catalog.h"
#include <ctime>
#include <cmath>
#include <random>
#include <algorithm>
#include <SDL2/SDL.h>

Game G;

const int envEffect[3][4] = {{3, 9, 12, 15}, {-3, 3, 6, 9}, {-57, -18, -9, 0}};
const int foodEffect[3][4] = {{10, 5, 3, 1}, {20, 10, 6, 3}, {30, 20, 10, 5}};
const int magicBody[MAGIC_COUNT] = {9, 8, 18, 15, 6, 19, 7};
const int magicFin[MAGIC_COUNT] = {7, 12, 15, 8, 2, 3, 13};

static std::mt19937 g_rng((unsigned)time(nullptr));
int rnd(int lo, int hi) { if (hi <= lo) return lo; return std::uniform_int_distribution<int>(lo, hi - 1)(g_rng); }
float rndf(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(g_rng); }

double Game::unixNow() { return (double)time(nullptr); }
int Game::now() const { return (int)(time(nullptr) - startEpoch); }

Fish* Game::get(int id) { for (auto& f : fish) if (f.id == id) return &f; return nullptr; }
int Game::countInTank(int t, bool aliveOnly) const {
    int n = 0;
    for (auto& f : fish) if (f.tank == t && (!aliveOnly || f.alive())) n++;
    return n;
}
bool Game::tankUnlocked(int t) const {
    if (t == 0 || t == TANK_BREEDING || t == TANK_SALES) return true;
    if (t == 1) return has("upgrade_tank upgrade1");
    if (t == 2) return has("upgrade_tank upgrade2");
    if (t == TANK_SALES2) return has("upgrade_sellingtank");
    return false;
}
int Game::capacity(int t) const {
    if (t == TANK_BREEDING) return 2;
    if (t == TANK_SALES || t == TANK_SALES2) return 8;
    return GS::tankCapacity;
}
int Game::environmentLevel() const {
    if (!has("research_icon_environment1") && !has("research_icon_environment")) return 1;
    if (!has("research_icon_environment2")) return 2;
    return has("research_icon_environment3") ? 4 : 3;
}
int Game::foodLevel() const {
    if (!has("research_icon_food")) return 1;
    if (!has("research_icon_food2")) return 2;
    return has("research_icon_food3") ? 4 : 3;
}
int Game::advertisingLevel() const {
    if (!has("research_icon_advertizing")) return 1;
    if (!has("research_icon_advertizing2")) return 2;
    return has("research_icon_advertizing3") ? 4 : 3;
}
int Game::magicKind(int b, int f) {
    for (int k = 0; k < MAGIC_COUNT; k++) if (magicBody[k] == b && magicFin[k] == f) return k;
    return -1;
}
bool Game::magicInTank(int kind, int tank) const {
    for (auto& f : fish) if (f.tank == tank && f.body == magicBody[kind] && f.fin == magicFin[kind]) return true;
    return false;
}
int Game::value(int b, int f) const { return catalog::fishValue(b, f); }
int Game::price(int b, int f) const {
    auto it = breedPrice.find(b * 21 + f);
    return it == breedPrice.end() ? value(b, f) : it->second;
}
int Game::numDiscovered(int r) const {
    int n = 0;
    for (auto& kv : breedPrice) if (r < 0 || rarity(kv.first / 21, kv.first % 21) == r) n++;
    return n;
}

void Game::newGame() {
    auto w = std::move(waves); auto e = onEvent; auto b = onFishBorn;
    *this = Game();
    waves = std::move(w); onEvent = e; onFishBorn = b;
    startEpoch = (long long)time(nullptr);
    inventory["food"] = 1;
    inventory["supplies_eggs_common"] = 1;   // starter eggs
    inventory["supplies_growthhormone"] = 1;
    dirty = true;
}

Fish& Game::addFish(int body, int fin, int tank, int age) {
    Fish f;
    f.id = nextFishId++;
    f.body = body; f.fin = fin; f.tank = tank;
    f.age = age; f.agesChecked = age;
    f.p1 = f.p2 = f.g1 = f.g2 = f.g3 = f.g4 = Anc{body, fin};
    f.lastTimeChecked = now();
    fish.push_back(f);
    dirty = true;
    return fish.back();
}

void Game::healthTax(Fish& f) {
    if (f.health < 1) return;
    int r = rarity(f.body, f.fin);
    int effect = envEffect[r][environmentLevel() - 1];
    if (effect < 0) f.envDamage -= effect;
    if (has("upgrade_temperature regulator") && rnd(0, 100) <= GS::tempRegChance) effect += GS::tempRegBenefit;
    if (has("upgrade_aeration system") && rnd(0, 100) <= GS::aerationChance) effect += GS::aerationBenefit;
    if (has("upgrade_power filter") && rnd(0, 100) <= GS::filterChance) effect += GS::filterBenefit;
    int delta;
    if (f.infested > 0) {
        delta = std::min(effect, 0) - GS::sicknessPenalty;
        if (f.infested == 1) f.fungalDamage += GS::sicknessPenalty; else f.ickDamage += GS::sicknessPenalty;
    } else if (f.food <= 4) delta = effect < 1 ? effect : 0;
    else delta = effect;
    f.health = std::max(0, std::min(GS::maxHealth, f.health + delta));
}

void Game::chanceOfInfestation(Fish& f) {
    if (f.infested > 0) return;
    int fl = foodLevel() - 1;
    int chance = foodEffect[partRarity(f.body)][fl];
    for (auto& o : fish) if (o.tank == f.tank && o.infested >= 1) chance += GS::infestationSpreadChance;
    if (f.age > GS::infestationOldAgeThreshold) chance += GS::infestationOldAgeChance;
    if (has("upgrade_plant of curing")) chance = (int)(chance / 2.0f);
    if (has("upgrade_cleaningsnail") && rnd(0, 100) <= GS::cleaningSnailChance) chance = (int)(chance / 2.0f);
    if (rnd(0, GS::infestationChanceRange) < chance) {
        f.infested = rnd(0, 2) + 1;
        if (onEvent) onEvent("sick");
    }
}

bool Game::spawnBaby(Fish& m, int tank) {
    if (countInTank(tank) >= capacity(tank)) { if (onEvent) onEvent("tankfull"); return false; }
    Fish b;
    b.id = nextFishId++;
    b.body = m.babyBody; b.fin = m.babyFin; b.tank = tank;
    b.p1 = m.bp1; b.p2 = m.bp2; b.g1 = m.bg1; b.g2 = m.bg2; b.g3 = m.bg3; b.g4 = m.bg4;
    b.age = std::max(0, m.age - m.ageToBirth);
    b.agesChecked = 0;
    b.lastTimeChecked = now();
    fish.push_back(b);
    report("hatch_eggs_internal", "", 0);
    if (onFishBorn) onFishBorn(b.id);
    return true;
}

void Game::impregnate(Fish& m, Fish& fa) {
    m.ageToBirth = m.age + GS::agesOfPregnancy;
    m.babyBody = (m.body + fa.body) / 2;
    m.babyFin = (m.fin + fa.fin) / 2;
    m.bp1 = {m.body, m.fin}; m.bp2 = {fa.body, fa.fin};
    m.bg1 = m.p1; m.bg2 = m.p2; m.bg3 = fa.p1; m.bg4 = fa.p2;
    report("breed_fish", "any");
    dirty = true;
}

// GameControl.UpdateAllFish – one fish
bool Game::updateFish(Fish& f, int nowT, int& cured) {
    if (f.health < 1) { f.lastTimeChecked = nowT; return false; }
    int acc = std::min(nowT - f.lastTimeChecked + f.secondsAccumulated, GS::maxAccumulated);
    f.secondsAccumulated = std::max(0, acc);
    int ages;
    if (paused) { f.secondsAccumulated = 0; f.agesChecked = f.age; ages = 0; }
    else ages = f.secondsAccumulated / GS::secondsPerAge;
    f.lastTimeChecked = nowT;
    if (magicInTank(MAGIC_GROWTH, f.tank) && f.secondsAccumulated >= 271 && ages >= 0) {
        for (int i = 0; i <= ages; i++) if (rnd(0, 100) <= 10) f.age++;
    }
    int prevAge = f.age;
    f.age += ages;
    f.secondsAccumulated = std::max(0, f.secondsAccumulated - ages * GS::secondsPerAge);
    if (f.age >= GS::ageUntilAdult && f.health >= 1 && !discovered(f.body, f.fin)) {
        breedPrice[f.body * 21 + f.fin] = value(f.body, f.fin);
        int mk = magicKind(f.body, f.fin);
        if (mk >= 0) { magicDiscovered.insert(mk); if (onEvent) onEvent("magic:" + std::to_string(mk)); }
        else if (onEvent) onEvent("species:" + std::to_string(f.body * 21 + f.fin));
        report("discover_species", "any");
        const char* rn[3] = {"common", "uncommon", "rare"};
        report("discover_species", rn[rarity(f.body, f.fin)]);
    }
    if (f.age >= GS::ageUntilAdult && prevAge < GS::ageUntilAdult && f.health >= 1) report("age_fish", "20");
    // birth
    if (f.ageToBirth != 0 && f.age >= f.ageToBirth && f.tank != TANK_NONE) {
        if (magicInTank(MAGIC_MUTATION, f.tank) && rnd(0, 100) <= 15) {
            if (rnd(0, 100) > 49) { if (f.babyFin != 20 && (rnd(0, 100) >= 50 || f.babyFin == 0)) f.babyFin++; else if (f.babyFin > 0) f.babyFin--; }
            else { if (f.babyBody != 20 && (rnd(0, 100) >= 50 || f.babyBody == 0)) f.babyBody++; else if (f.babyBody > 0) f.babyBody--; }
        }
        int t = f.tank < 0 ? 0 : f.tank;
        spawnBaby(f, t);
        if (magicInTank(MAGIC_FERTILITY, f.tank)) spawnBaby(f, t);
        if (has("upgrade_plant of fertility")) spawnBaby(f, t);
        f.ageToBirth = 0;
    }
    bool died = false;
    for (int a = f.agesChecked; a < f.age && f.health > 0; a++) {
        healthTax(f);
        if (magicInTank(MAGIC_HEALTH, f.tank)) f.health += GS::magicFishOfHealthBenefit;
        if (f.age >= GS::ageUntilAdult) chanceOfInfestation(f);
        if (f.infested >= 1) {
            if (magicInTank(MAGIC_CURING, f.tank) && rnd(0, 100) <= 19) { f.infested = 0; cured++; }
            else if (f.ageToBirth >= 1) { f.ageToBirth = 0; if (onEvent) onEvent("miscarried"); }
        }
        int fl = foodLevel();
        bool dec = fl == 2 ? rnd(0, 100) <= 60 : fl == 3 ? rnd(0, 100) <= 45 : fl == 4 ? rnd(0, 100) <= 30 : true;
        if (dec) f.food--;
        if (f.food <= 2) {
            f.food = 0;
            if (!magicInTank(MAGIC_SUSTENANCE, f.tank)) { f.health -= GS::starvationPenalty; f.starveDamage += GS::starvationPenalty; }
        }
        if (has("upgrade_plant of nutrition") && rnd(0, 100) <= 20) f.food += 3;
        if (has("upgrade_auto feeder") && rnd(0, 100) <= 40) f.food += 6;
        f.health = std::max(0, std::min(GS::maxHealth, f.health));
        f.food = std::max(0, std::min(100, f.food));
        if (f.health <= 0) {
            f.age = a + 1;
            fishDead++;
            died = true;
            if (onEvent) onEvent("dead");
            break;
        }
    }
    f.agesChecked = f.age;
    return died;
}

void Game::tick() {
    int t = now();
    int cured = 0;
    // iterate by index – births append to vector
    for (size_t i = 0; i < fish.size(); i++) {
        Fish copy = fish[i];
        updateFish(copy, t, cured);
        // fish vector may have grown; write back by id
        if (Fish* f = get(copy.id)) *f = copy;
    }
    fishCured += cured;
    // eggs hatching
    double un = unixNow();
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i].id.rfind("supplies_eggs", 0) == 0 && items[i].hatchAt > 0 && un >= items[i].hatchAt) {
            TankItem e = items[i];
            items.erase(items.begin() + i); i--;
            hatchEggs(e);
        }
    }
    lastTick = t;
    dirty = true;
}

void Game::hatchEggs(TankItem& e) {
    int n = rnd(0, 2) + (has("upgrade_plant of egg fertility") ? 1 : 0) + 3;
    bool starter = !triggers.count("UsedStarterEggs");
    std::string kind = e.id;
    for (int i = 0; i < n; i++) {
        int b, f;
        if (starter) {   // starter eggs: guaranteed breedable commons
            b = 4 * rnd(0, 6); f = b;
        } else if (kind == "supplies_eggs_common") { b = 4 * rnd(0, 6); f = 4 * rnd(0, 6); }
        else if (kind == "supplies_eggs_uncommon") { b = 4 * rnd(0, 5) + 2; f = 4 * rnd(0, 5) + 2; }
        else if (kind == "supplies_eggs_rare") { b = 2 * rnd(0, 10) + 1; f = 2 * rnd(0, 10) + 1; }
        else {   // legendary: rare fish, small chance of a magic fish
            if (rnd(0, 100) < 12) { int k = rnd(0, MAGIC_COUNT - 1); b = magicBody[k]; f = magicFin[k]; }
            else { b = 2 * rnd(0, 10) + 1; f = 2 * rnd(0, 10) + 1; }
        }
        if (countInTank(e.tank) >= capacity(e.tank)) break;
        Fish& nf = addFish(b, f, e.tank, 0);
        if (onFishBorn) onFishBorn(nf.id);
    }
    if (starter) triggers.insert("UsedStarterEggs");
    const char* rn = kind == "supplies_eggs_uncommon" ? "uncommon" : kind == "supplies_eggs_rare" ? "rare" : "common";
    report("hatch_eggs", rn);
    report("hatch_eggs", "any");
    if (onEvent) onEvent("hatched");
}

void Game::feed(int tank) {
    for (auto& f : fish) {
        if (f.tank != tank || f.health < 1) continue;
        f.food += 50;
        if (f.food > 100) {
            f.food = 100;
            f.health -= GS::overfedPenalty;
            f.overfedDamage += GS::overfedPenalty;
            if (f.health < 0) f.health = 0;
        }
    }
    dirty = true;
}

bool Game::useItem(const std::string& id, int tank, float x, float y) {
    auto consume = [&]() {
        int d = catalog::item(id) ? catalog::item(id)->doses : 1;
        if (d > 1) {
            int& left = doses[id];
            if (left <= 0) left = d;
            left--;
            if (left > 0) return;
        }
        if (--inventory[id] <= 0) inventory.erase(id);
    };
    if (id == "food") { feed(tank); return true; }
    if (!inventory.count(id) || inventory[id] <= 0) return false;
    const catalog::Item* it = catalog::item(id);
    if (id.rfind("supplies_eggs", 0) == 0) {
        TankItem e; e.id = id; e.tank = tank; e.x = x; e.y = y; e.uid = nextItemUid++;
        e.hatchAt = unixNow() + 60;   // eggs hatch after a minute
        items.push_back(e);
        consume();
        return true;
    }
    if (id == "supplies_hatchingformula") {
        bool any = false;
        for (auto& e : items) if (e.tank == tank && e.id.rfind("supplies_eggs", 0) == 0) { e.hatchAt = unixNow(); any = true; }
        if (!any) return false;
        consume(); report("use_item", id); return true;
    }
    if (id == "supplies_medicine_generic") {
        for (auto& f : fish) if (f.tank == tank && f.health >= 1 && f.health < GS::maxHealth)
            f.health = std::min(GS::maxHealth, f.health + GS::medicineBaseEffect + rnd(0, GS::medicineRandomEffect));
        consume(); report("use_item", id); return true;
    }
    if (id == "supplies_icktreatment" || id == "supplies_fungaltreatment") {
        int want = id == "supplies_icktreatment" ? 2 : 1;
        int c = 0;
        for (auto& f : fish) if (f.tank == tank && f.infested == want && rnd(0, 100) > 39) { f.infested = 0; c++; }
        fishCured += c;
        consume(); report("use_item", id); return true;
    }
    if (id == "supplies_growthhormone") {
        bool any = false;
        for (auto& f : fish) if (f.tank == tank && f.health >= 1 && f.age < GS::ageUntilAdult) {
            f.age = GS::ageUntilAdult + rnd(0, 5);
            f.agesChecked = f.age - 3;
            any = true;
            if (!discovered(f.body, f.fin)) {
                breedPrice[f.body * 21 + f.fin] = value(f.body, f.fin);
                int mk = magicKind(f.body, f.fin);
                if (mk >= 0) { magicDiscovered.insert(mk); if (onEvent) onEvent("magic:" + std::to_string(mk)); }
                else if (onEvent) onEvent("species:" + std::to_string(f.body * 21 + f.fin));
                report("discover_species", "any");
                const char* rn[3] = {"common", "uncommon", "rare"};
                report("discover_species", rn[rarity(f.body, f.fin)]);
            }
            report("age_fish", "20");
        }
        if (!any) return false;
        consume(); report("use_item", id); return true;
    }
    if (id == "supplies_gestationpotion") {
        bool any = false;
        for (auto& f : fish) if (f.tank == tank && f.ageToBirth >= 1) { f.ageToBirth = f.age; any = true; }
        if (!any) return false;
        consume(); report("use_item", id); tick(); return true;
    }
    if (id == "supplies_allurepotion") {
        allureUntil = unixNow() + GS::allurePotionDuration;
        consume(); report("use_item", id); return true;
    }
    if (it && it->tab == "decorations") {
        TankItem d; d.id = id; d.tank = tank; d.x = x; d.y = y; d.uid = nextItemUid++;
        items.push_back(d);
        consume();
        return true;
    }
    return false;
}

void Game::moveFish(int id, int tank) {
    Fish* f = get(id);
    if (!f) return;
    f->tank = tank;
    dirty = true;
}
void Game::removeFish(int id) {
    fish.erase(std::remove_if(fish.begin(), fish.end(), [&](const Fish& f) { return f.id == id; }), fish.end());
    dirty = true;
}
int Game::sellFish(int id, int p) {
    Fish* f = get(id);
    if (!f) return 0;
    int r = rarity(f->body, f->fin);
    removeFish(id);
    coins += p; coinsEarned += p; fishSold++;
    report("sell_fish", "any");
    report("sell_fish", "");
    const char* rn[3] = {"common", "uncommon", "rare"};
    report("sell_fish", rn[r]);
    report("earn_coins", "", p);
    return p;
}

void Game::addCurrency(const std::string& cur, int amt) {
    if (cur == "CN") { coins += amt; report("earn_coins", "", amt); }
    else if (cur == "GM") gems += amt;
}
void Game::giveReward(const std::string& r) {
    // "gift^CN^20" / "gift^supplies_growthhormone^1"
    size_t a = r.find('^'); if (a == std::string::npos) return;
    size_t b = r.find('^', a + 1);
    std::string what = r.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
    int amt = b == std::string::npos ? 1 : atoi(r.c_str() + b + 1);
    if (what == "CN" || what == "GM") addCurrency(what, amt);
    else inventory[what] += amt;
    dirty = true;
}

bool Game::buy(const std::string& id) {
    const catalog::Item* it = catalog::item(id);
    if (!it) return false;
    if (it->permanent && has(id)) return false;
    if (!it->prereq.empty() && !has(it->prereq)) return false;
    if (it->currency == "GM") { if (gems < it->cost) return false; gems -= it->cost; }
    else { if (coins < it->cost) return false; coins -= it->cost; }
    if (it->permanent) acquired.insert(id);
    else if (id == "bank_gems_small") gems += 10;
    else if (id == "bank_gems_large") gems += 60;
    else if (id == "bank_coins_small") coins += 1000;
    else if (id == "bank_coins_large") coins += 6000;
    else inventory[id] += std::max(1, it->amount);
    report("buy_item", id);
    report("buy_item_type", it->tab == "decorations" ? "decorate" : it->tab);
    dirty = true;
    return true;
}

// ------------------------------------------------------------ tasks
static std::vector<std::string> csvSplit(const std::string& line) {
    std::vector<std::string> out; std::string cur; bool q = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '"') { q = !q; continue; }
        if (c == ',' && !q) { out.push_back(cur); cur.clear(); continue; }
        if (c != '\r') cur += c;
    }
    out.push_back(cur);
    return out;
}
void Game::loadTasks(const std::string& path) {
    std::string s; if (!readFile(path, s)) return;
    waves.clear();
    size_t p = 0; bool first = true;
    while (p < s.size()) {
        size_t e = s.find('\n', p); if (e == std::string::npos) e = s.size();
        std::string line = s.substr(p, e - p); p = e + 1;
        if (first) { first = false; continue; }
        auto c = csvSplit(line);
        if (c.size() < 6 || c[0].empty()) continue;
        int w = atoi(c[0].c_str());
        if ((int)waves.size() <= w) waves.resize(w + 1);
        if (c[1] == "chest") { waves[w].chest = c[2]; waves[w].chestReward = c[5]; continue; }
        Task t; t.type = c[1]; t.param = c[2]; t.need = std::max(1, atoi(c[3].c_str())); t.text = c[4]; t.reward = c[5];
        if (c.size() > 6) t.fallback = c[6];
        if (c.size() > 7) t.tip = c[7];
        if (c.size() > 8) t.tipCmd = c[8];
        waves[w].tasks.push_back(t);
    }
}
void Game::report(const std::string& type, const std::string& param, int amount) {
    if (taskWave >= (int)waves.size()) return;
    auto& w = waves[taskWave];
    for (int i = 0; i < 3 && i < (int)w.tasks.size(); i++) {
        auto& t = w.tasks[i];
        if (taskCollected[i] || taskProgress[i] >= t.need) continue;
        bool match = t.type == type && (t.param.empty() || t.param == "any" ? (param == "any" || param.empty() || type == "earn_coins" || type == "age_fish") : t.param == param);
        if (type == "age_fish") match = t.type == type;
        if (type == "earn_coins") match = t.type == type;
        if (t.type == "buy_item" && type == "buy_item") match = t.param == param;
        if (t.type == "buy_item_type" && type == "buy_item_type") match = t.param == param;
        if (t.type == "use_item" && type == "use_item") match = t.param == param;
        if (t.type == "sell_fish" && type == "sell_fish") match = (t.param.empty() || t.param == "any") ? param == "any" : t.param == param;
        if (t.type == "hatch_eggs" && type == "hatch_eggs") match = (t.param == "any") ? param == "any" : t.param == param;
        if (t.type == "discover_species" && type == "discover_species") match = t.param == param;
        if (match) taskProgress[i] = std::min(t.need, taskProgress[i] + (type == "earn_coins" ? amount : 1));
    }
}

void Game::setPaused(bool p) {
    if (p == paused) return;
    paused = p;
    if (p) pausedAt = now();
    dirty = true;
}

// ------------------------------------------------------------ save / load
static JV anc(const Anc& a) { JV v = JV::mkarr(); v.push(JV::mknum(a.body)); v.push(JV::mknum(a.fin)); return v; }
static Anc ancr(const JV& v) { return Anc{v[0].i(-1), v[1].i(-1)}; }

bool Game::save(const std::string& path) {
    JV r = JV::mkobj();
    r.set("v", JV::mknum(1));
    r.set("coins", JV::mknum(coins)); r.set("gems", JV::mknum(gems));
    r.set("start", JV::mknum((double)startEpoch));
    r.set("nextFish", JV::mknum(nextFishId)); r.set("nextItem", JV::mknum(nextItemUid));
    JV fa = JV::mkarr();
    for (auto& f : fish) {
        JV o = JV::mkarr();
        int vals[] = {f.id, f.body, f.fin, f.tank, f.agesChecked, f.age, f.ageToBirth, f.babyBody, f.babyFin, f.lastTimeChecked, f.secondsAccumulated,
                      f.health, f.infested, f.food, f.envDamage, f.ickDamage, f.fungalDamage, f.starveDamage, f.overfedDamage, f.salePrice};
        for (int v : vals) o.push(JV::mknum(v));
        for (const Anc* a : {&f.bp1, &f.bp2, &f.bg1, &f.bg2, &f.bg3, &f.bg4, &f.p1, &f.p2, &f.g1, &f.g2, &f.g3, &f.g4}) o.push(anc(*a));
        fa.push(o);
    }
    r.set("fish", fa);
    JV ia = JV::mkarr();
    for (auto& it : items) {
        JV o = JV::mkobj();
        o.set("id", JV::mkstr(it.id)); o.set("t", JV::mknum(it.tank)); o.set("x", JV::mknum(it.x)); o.set("y", JV::mknum(it.y));
        o.set("s", JV::mknum(it.scale)); o.set("h", JV::mknum(it.hatchAt)); o.set("u", JV::mknum(it.uid));
        ia.push(o);
    }
    r.set("items", ia);
    JV inv = JV::mkobj(); for (auto& kv : inventory) inv.set(kv.first, JV::mknum(kv.second)); r.set("inv", inv);
    JV ds = JV::mkobj(); for (auto& kv : doses) ds.set(kv.first, JV::mknum(kv.second)); r.set("doses", ds);
    JV acq = JV::mkarr(); for (auto& s : acquired) acq.push(JV::mkstr(s)); r.set("acq", acq);
    JV bp = JV::mkarr(); for (auto& kv : breedPrice) { JV p = JV::mkarr(); p.push(JV::mknum(kv.first)); p.push(JV::mknum(kv.second)); bp.push(p); } r.set("breeds", bp);
    JV md = JV::mkarr(); for (int m : magicDiscovered) md.push(JV::mknum(m)); r.set("magic", md);
    JV tr = JV::mkarr(); for (auto& s : triggers) tr.push(JV::mkstr(s)); r.set("trig", tr);
    JV th = JV::mkarr(); for (int i = 0; i < 3; i++) th.push(JV::mknum(tankTheme[i])); r.set("theme", th);
    JV st = JV::mkarr(); for (int v : {fishSold, coinsEarned, fishCured, fishDead}) st.push(JV::mknum(v)); r.set("stats", st);
    JV tk = JV::mkarr(); tk.push(JV::mknum(taskWave));
    for (int i = 0; i < 3; i++) { tk.push(JV::mknum(taskProgress[i])); tk.push(JV::mknum(taskCollected[i])); }
    r.set("tasks", tk);
    r.set("paused", JV::mkbool(paused)); r.set("pausedAt", JV::mknum((double)pausedAt));
    r.set("allure", JV::mknum(allureUntil));
    r.set("mv", JV::mknum(musicVol)); r.set("sv", JV::mknum(sfxVol));
    r.set("tut", JV::mknum(tutorialStep));
    std::string out; jwrite(r, out);
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    // keep two rolling backups of the last good saves
    {
        FILE* chk = fopen(path.c_str(), "rb");
        if (chk) {
            fseek(chk, 0, SEEK_END); long n = ftell(chk); fclose(chk);
            if (n > 10) {
                std::string b1 = path + ".bak", b2 = path + ".bak2";
                static double lastRoll = 0;
                if (unixNow() - lastRoll > 300) {   // rotate the older backup at most every 5 minutes
                    remove(b2.c_str()); rename(b1.c_str(), b2.c_str()); lastRoll = unixNow();
                } else remove(b1.c_str());
                rename(path.c_str(), b1.c_str());
            } else remove(path.c_str());
        }
    }
    rename(tmp.c_str(), path.c_str());
    dirty = false;
    return true;
}

bool Game::load(const std::string& path) {
    JV r = jload(path);
    if (!r.isObj() || !r.has("coins")) { SDL_Log("save unreadable, trying backup"); r = jload(path + ".bak"); }
    if (!r.isObj() || !r.has("coins")) { SDL_Log("backup unreadable, trying second backup"); r = jload(path + ".bak2"); }
    if (!r.isObj() || !r.has("coins")) return false;
    coins = r["coins"].i(); gems = r["gems"].i();
    startEpoch = (long long)r["start"].num();
    nextFishId = r["nextFish"].i(1); nextItemUid = r["nextItem"].i(1);
    fish.clear();
    for (auto& o : r["fish"].a) {
        Fish f; int k = 0;
        int* vals[] = {&f.id, &f.body, &f.fin, &f.tank, &f.agesChecked, &f.age, &f.ageToBirth, &f.babyBody, &f.babyFin, &f.lastTimeChecked, &f.secondsAccumulated,
                       &f.health, &f.infested, &f.food, &f.envDamage, &f.ickDamage, &f.fungalDamage, &f.starveDamage, &f.overfedDamage, &f.salePrice};
        for (int* v : vals) *v = o[k++].i();
        for (Anc* a : {&f.bp1, &f.bp2, &f.bg1, &f.bg2, &f.bg3, &f.bg4, &f.p1, &f.p2, &f.g1, &f.g2, &f.g3, &f.g4}) *a = ancr(o[k++]);
        fish.push_back(f);
    }
    items.clear();
    for (auto& o : r["items"].a) {
        TankItem it; it.id = o["id"].str(); it.tank = o["t"].i(); it.x = o["x"].f(); it.y = o["y"].f(); it.scale = o["s"].f(1); it.hatchAt = o["h"].num(); it.uid = o["u"].i();
        items.push_back(it);
    }
    inventory.clear(); for (auto& kv : r["inv"].o) inventory[kv.first] = kv.second.i();
    doses.clear(); for (auto& kv : r["doses"].o) doses[kv.first] = kv.second.i();
    acquired.clear(); for (auto& s : r["acq"].a) acquired.insert(s.str());
    breedPrice.clear(); for (auto& p : r["breeds"].a) breedPrice[p[0].i()] = p[1].i();
    magicDiscovered.clear(); for (auto& m : r["magic"].a) magicDiscovered.insert(m.i());
    triggers.clear(); for (auto& s : r["trig"].a) triggers.insert(s.str());
    for (int i = 0; i < 3; i++) tankTheme[i] = r["theme"][i].i();
    fishSold = r["stats"][0].i(); coinsEarned = r["stats"][1].i(); fishCured = r["stats"][2].i(); fishDead = r["stats"][3].i();
    taskWave = r["tasks"][0].i();
    for (int i = 0; i < 3; i++) { taskProgress[i] = r["tasks"][1 + i * 2].i(); taskCollected[i] = r["tasks"][2 + i * 2].i() != 0; }
    paused = r["paused"].b; pausedAt = (long long)r["pausedAt"].num();
    allureUntil = r["allure"].num();
    musicVol = r["mv"].f(.6f); sfxVol = r["sv"].f(.8f);
    tutorialStep = r["tut"].i();
    if (paused) {   // time does not pass while paused: shift clock
        long long nowT = now();
        if (nowT > pausedAt) {
            long long d = nowT - pausedAt;
            startEpoch += d;
            pausedAt = now();
        }
    }
    dirty = false;
    return true;
}
