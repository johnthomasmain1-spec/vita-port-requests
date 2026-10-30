#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include "json.h"

// tank indices (GameSettings)
enum { TANK_BREEDING = -1, TANK_SALES = -2, TANK_SALES2 = -3, TANK_NONE = -999 };
enum MagicKind { MAGIC_FERTILITY = 0, MAGIC_SUSTENANCE, MAGIC_CURING, MAGIC_GROWTH, MAGIC_HEALTH, MAGIC_MUTATION, MAGIC_GOLDENGUPPY, MAGIC_COUNT };

struct GS {  // GameSettings recovered from the original binary
    static const int startingCoins = 500, startingGems = 100;
    static const int secondsPerAge = 450, ageUntilTeen = 6, ageUntilAdult = 20, agesOfPregnancy = 4;
    static const int maxHealth = 300, overfedPenalty = 6, starvationPenalty = 3, sicknessPenalty = 1;
    static const int medicineBaseEffect = 60, medicineRandomEffect = 40, magicFishOfHealthBenefit = 3;
    static const int tempRegChance = 40, tempRegBenefit = 3, aerationChance = 40, aerationBenefit = 3, filterChance = 40, filterBenefit = 3;
    static const int infestationChanceRange = 10000, infestationSpreadChance = 6, infestationOldAgeThreshold = 600, infestationOldAgeChance = 15;
    static const int cleaningSnailChance = 50;
    static const int numTanks = 3, tankCapacity = 21, numBodies = 21, numFins = 21;
    static const int breedingDuration = 3, allurePotionDuration = 300;
    static const int maxAccumulated = 259200;
    static const int costToReviveFish = 2;
};
extern const int envEffect[3][4];
extern const int foodEffect[3][4];
extern const int magicBody[MAGIC_COUNT], magicFin[MAGIC_COUNT];

struct Anc { int body = -1, fin = -1; };
struct Fish {
    int id = 0, body = 0, fin = 0, tank = 0;
    int agesChecked = 0, age = 0, ageToBirth = 0;
    int babyBody = 0, babyFin = 0; Anc bp1, bp2, bg1, bg2, bg3, bg4;
    int lastTimeChecked = 0, secondsAccumulated = 0;
    int health = GS::maxHealth, infested = 0, food = 90;
    int envDamage = 0, ickDamage = 0, fungalDamage = 0, starveDamage = 0, overfedDamage = 0;
    Anc p1, p2, g1, g2, g3, g4;
    int salePrice = 0;   // price set when placed in sales tank (0 = species price)
    bool alive() const { return health > 0; }
    bool adult() const { return age >= GS::ageUntilAdult; }
    bool pregnant() const { return ageToBirth > 0; }
};

struct TankItem {   // things placed in tanks: eggs, decorations
    std::string id;
    int tank = 0;
    float x = 0, y = 0;   // world units
    float scale = 1;
    double hatchAt = 0;   // eggs: unix time when they hatch
    int uid = 0;
};

struct Task { std::string type, param; int need = 0; std::string text, reward, fallback, tip, tipCmd; };
struct TaskWave { std::vector<Task> tasks; std::string chest; std::string chestReward; };

struct Game {
    int coins = GS::startingCoins, gems = GS::startingGems;
    long long startEpoch = 0;
    int nextFishId = 1, nextItemUid = 1;
    std::vector<Fish> fish;
    std::vector<TankItem> items;
    std::map<std::string, int> inventory;      // consumables / decorations in "My Items"
    std::map<std::string, int> doses;          // remaining doses for multi-use items
    std::set<std::string> acquired;            // upgrades / research bought
    std::map<int, int> breedPrice;             // (body*21+fin) -> price, present = discovered
    std::set<int> magicDiscovered;
    std::set<std::string> triggers;
    int tankTheme[3] = {0, 0, 0};              // 0 coral reef, 1 atlantis, 2 muddy river, 3 rocky shore
    int fishSold = 0, coinsEarned = 0, fishCured = 0, fishDead = 0;
    int taskWave = 0; int taskProgress[3] = {0, 0, 0}; bool taskCollected[3] = {false, false, false};
    bool paused = false; long long pausedAt = 0;
    double allureUntil = 0;
    float musicVol = 0.6f, sfxVol = 0.8f;
    int tutorialStep = 0;
    int lastTick = 0;
    bool dirty = false;

    std::vector<TaskWave> waves;
    std::function<void(const std::string&)> onEvent;   // ui notifications
    std::function<void(int fishId)> onFishBorn;

    // time
    int now() const;
    static double unixNow();

    // queries
    Fish* get(int id);
    int countInTank(int tank, bool aliveOnly = false) const;
    int capacity(int tank) const;
    bool tankUnlocked(int tank) const;
    bool has(const std::string& id) const { return acquired.count(id) > 0; }
    int environmentLevel() const;
    int foodLevel() const;
    int advertisingLevel() const;
    bool magicInTank(int kind, int tank) const;
    static int partRarity(int p) { return (p & 1) ? 2 : ((p % 4) == 2 ? 1 : 0); }
    static int rarity(int body, int fin) { return std::max(partRarity(body), partRarity(fin)); }
    static int magicKind(int body, int fin);
    int value(int body, int fin) const;
    int price(int body, int fin) const;     // species asking price
    bool discovered(int body, int fin) const { return breedPrice.count(body * 21 + fin) > 0; }
    int numDiscovered(int rarity = -1) const;

    // actions
    void newGame();
    void tick();                         // UpdateAllFish
    bool updateFish(Fish& f, int nowT, int& cured);
    void healthTax(Fish& f);
    void chanceOfInfestation(Fish& f);
    bool spawnBaby(Fish& mother, int tank);
    Fish& addFish(int body, int fin, int tank, int age = 0);
    void impregnate(Fish& mother, Fish& father);
    void feed(int tank);
    bool useItem(const std::string& id, int tank, float x, float y);
    void hatchEggs(TankItem& egg);
    void moveFish(int id, int tank);
    void removeFish(int id);
    int sellFish(int id, int price);     // returns coins
    bool buy(const std::string& id);
    void addCurrency(const std::string& cur, int amount);
    void giveReward(const std::string& reward);
    void report(const std::string& type, const std::string& param, int amount = 1);
    void loadTasks(const std::string& csvPath);
    void setPaused(bool p);

    // persistence
    bool save(const std::string& path);
    bool load(const std::string& path);
};

extern Game G;
int rnd(int lo, int hiExclusive);   // UnityEngine.Random.Range(int,int)
float rndf(float lo, float hi);
