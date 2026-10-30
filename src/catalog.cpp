#include "catalog.h"
#include "game.h"
#include <unordered_map>

namespace catalog {
static std::vector<Item> g_items;
static std::unordered_map<std::string, size_t> g_idx;

static void add(const char* id, const char* tab, const char* name, const char* desc, int cost, const char* cur,
                bool perm = false, const char* prereq = "", int doses = 1, int amount = 1, const char* icon = nullptr) {
    Item it; it.id = id; it.tab = tab; it.name = name; it.desc = desc; it.cost = cost; it.currency = cur;
    it.permanent = perm; it.prereq = prereq; it.doses = doses; it.amount = amount; it.icon = icon ? icon : id;
    g_idx[it.id] = g_items.size();
    g_items.push_back(it);
}

static void init() {
    if (!g_items.empty()) return;
    // ---- supplies
    add("supplies_eggs_common", "supplies", "Common Eggs", "Hatch 3-5 common fish. Drag the eggs into a tank and wait for them to hatch!", 100, "CN");
    add("supplies_eggs_uncommon", "supplies", "Uncommon Eggs", "Hatch 3-5 uncommon fish. Uncommon fish need better tank environments to thrive.", 400, "CN");
    add("supplies_eggs_rare", "supplies", "Rare Eggs", "Hatch 3-5 rare fish. They need special care to survive!", 25, "GM");
    add("supplies_eggs_legendary", "supplies", "Legendary Eggs", "Rare fish with a chance of hatching one of the magic fish of Isola!", 60, "GM");
    add("supplies_medicine_generic", "supplies", "Medicine", "Restores health to every injured fish in the tank.", 75, "CN");
    add("supplies_icktreatment", "supplies", "Ick Treatment", "Treatments will cure your fish and help prevent Ick from spreading. It may take multiple treatments to fully rid your tank of Ick.", 120, "CN", false, "", 3);
    add("supplies_fungaltreatment", "supplies", "Fungal Treatment", "Treatments will cure your fish and help prevent fungus from spreading. It may take multiple treatments to fully rid your tank of Fungus.", 120, "CN", false, "", 3);
    add("supplies_growthhormone", "supplies", "InstaGrow", "Instantly grows every baby fish in the tank into an adult.", 5, "GM");
    add("supplies_gestationpotion", "supplies", "InstaBirth", "Instabirth skips pregnancy - pregnant fish give birth right away.", 3, "GM");
    add("supplies_allurepotion", "supplies", "Allure Potion", "For 5 minutes, fish in the breeding tank breed instantly.", 4, "GM");
    add("supplies_hatchingformula", "supplies", "Hatching Formula", "Eggs in the tank hatch instantly.", 2, "GM");
    // ---- research
    add("research_icon_environment", "research", "Environment Research I", "Research Environment to improve your tanks! Keeps uncommon fish healthy.", 300, "CN", true);
    add("research_icon_environment2", "research", "Environment Research II", "Better tank environments keep rarer fish alive.", 1500, "CN", true, "research_icon_environment");
    add("research_icon_environment3", "research", "Environment Research III", "The best environment research - even rare fish thrive.", 5000, "CN", true, "research_icon_environment2");
    add("research_icon_food", "research", "Food Research I", "Higher Quality food provides increased nutrition and helps avoid the spread of disease.", 400, "CN", true);
    add("research_icon_food2", "research", "Food Research II", "Higher Quality food provides increased nutrition and helps avoid the spread of Ick.", 2000, "CN", true, "research_icon_food");
    add("research_icon_food3", "research", "Food Research III", "Premium food keeps fish fed for much longer.", 6000, "CN", true, "research_icon_food2");
    add("research_icon_advertizing", "research", "Advertising I", "More customers visit your fish store.", 500, "CN", true);
    add("research_icon_advertizing2", "research", "Advertising II", "Even more customers visit your fish store.", 2500, "CN", true, "research_icon_advertizing");
    add("research_icon_advertizing3", "research", "Advertising III", "Your store is the talk of Isola!", 7500, "CN", true, "research_icon_advertizing2");
    // ---- upgrades
    add("upgrade_tank upgrade1", "upgrades", "Second Tank", "Unlocks a second tank for your fish.", 1500, "CN", true, "", 1, 1, "upgrade_tank upgrade1");
    add("upgrade_tank upgrade2", "upgrades", "Third Tank", "Unlocks a third tank for your fish.", 5000, "CN", true, "upgrade_tank upgrade1", 1, 1, "upgrade_tank upgrade2");
    add("upgrade_sellingtank", "upgrades", "Second Selling Tank", "Show more fish for sale in your store.", 2500, "CN", true);
    add("upgrade_auto feeder", "upgrades", "Auto Feeder", "Adding an Autofeeder will help keep your fish fed on time.", 2000, "CN", true);
    add("upgrade_temperature regulator", "upgrades", "Temperature Regulator", "Regulating tank temperature helps keep rare fish alive.", 900, "CN", true);
    add("upgrade_aeration system", "upgrades", "Aeration System", "Aeration Systems transfer oxygen to your water, allowing rarer fish to survive.", 1200, "CN", true);
    add("upgrade_power filter", "upgrades", "Power Filter", "Filtering the water in your tank keeps it clean, reducing sickness and allowing rarer fish to survive.", 1500, "CN", true);
    add("upgrade_cleaningsnail", "upgrades", "Cleaning Snail", "This adorable creature will never tire of cleaning your tanks, reducing the risk of sickness.", 700, "CN", true);
    add("upgrade_plant of nutrition", "upgrades", "Plant of Nutrition", "The Plant of Nutrition releases nutritious fibers which can help prevent starvation.", 900, "CN", true);
    add("upgrade_plant of curing", "upgrades", "Plant of Curing", "This rare plant helps prevent diseases on fish that are exposed to the plant.", 1100, "CN", true);
    add("upgrade_plant of fertility", "upgrades", "Plant of Fertility", "Pregnant fish sometimes give birth to an extra fry.", 1800, "CN", true);
    add("upgrade_plant of egg fertility", "upgrades", "Plant of Egg Fertility", "Each batch of eggs hatches an extra fish.", 1400, "CN", true);
    add("upgrade_golden seahorse", "upgrades", "Golden Seahorse", "A lucky charm - customers are more eager to buy.", 30, "GM", true);
    add("upgrade_tv", "upgrades", "Store TV", "Keeps customers entertained while they shop.", 1000, "CN", true);
    add("upgrade_bookshelf", "upgrades", "Bookshelf", "Customers love browsing fish books.", 800, "CN", true);
    add("upgrade_vendingmachine", "upgrades", "Vending Machine", "Snacks for your customers - and coins for you.", 1500, "CN", true);
    add("upgrade_revealfountain", "upgrades", "Fountain", "A beautiful fountain that attracts more customers.", 2500, "CN", true);
    // ---- decorations
    struct D { const char* id; const char* name; int cost; const char* cur; };
    static const D decos[] = {
        {"3_pillars", "Ancient Pillars", 150, "CN"}, {"anchor", "Anchor", 120, "CN"}, {"anemone", "Anemone", 80, "CN"},
        {"aquaticpant_01", "Aquatic Plant", 40, "CN"}, {"aquaticpant_02", "Aquatic Plant", 40, "CN"}, {"aquaticpant_03", "Aquatic Plant", 45, "CN"},
        {"aquaticpant_04", "Aquatic Plant", 45, "CN"}, {"aquaticpant_05", "Aquatic Plant", 50, "CN"}, {"aquaticpant_06", "Aquatic Plant", 50, "CN"},
        {"aquaticpant_07", "Aquatic Plant", 55, "CN"}, {"aquaticpant_08", "Aquatic Plant", 55, "CN"}, {"aquaticpant_09", "Aquatic Plant", 60, "CN"},
        {"aquaticpant_10", "Aquatic Plant", 60, "CN"}, {"bubblemaker", "Bubble Maker", 200, "CN"}, {"deluxebubblemaker", "Deluxe Bubble Maker", 8, "GM"},
        {"coffeecan", "Coffee Can", 60, "CN"}, {"commodore", "Commodore", 250, "CN"}, {"deflatedbeachball", "Beach Ball", 70, "CN"},
        {"divingsuit", "Diving Suit", 300, "CN"}, {"driftwood", "Driftwood", 90, "CN"}, {"golfball", "Golf Ball", 60, "CN"},
        {"harpoon", "Harpoon", 140, "CN"}, {"hookhand", "Hook Hand", 160, "CN"}, {"lantern", "Lantern", 110, "CN"},
        {"mermaid", "Mermaid", 10, "GM"}, {"necklace", "Necklace", 220, "CN"}, {"pirateskull", "Pirate Skull", 180, "CN"},
        {"poseidontrident", "Poseidon's Trident", 12, "GM"}, {"pyramid", "Pyramid", 200, "CN"}, {"radio", "Radio", 100, "CN"},
        {"rustybike", "Rusty Bike", 120, "CN"}, {"satellite", "Satellite", 260, "CN"}, {"seamonster", "Sea Monster", 9, "GM"},
        {"ship", "Sunken Ship", 300, "CN"}, {"shuttlecock", "Shuttlecock", 50, "CN"}, {"skull", "Skull", 130, "CN"},
        {"starfish", "Starfish", 70, "CN"}, {"submarine", "Submarine", 350, "CN"}, {"sunkenhotairballoon", "Sunken Balloon", 280, "CN"},
        {"television", "Television", 160, "CN"}, {"trafficcone", "Traffic Cone", 50, "CN"}, {"treasurepile", "Treasure Pile", 400, "CN"},
        {"windmill", "Windmill", 240, "CN"}, {"thanksgiving_cornucopiabubblemaker", "Cornucopia", 6, "GM"}, {"thanksgiving_turkeydeco", "Turkey", 5, "GM"},
        {"christmas_teetertotterdeco", "Teeter Totter", 5, "GM"}, {"christmas_treedeco", "Holiday Tree", 6, "GM"}, {"valentines_cupid", "Cupid", 5, "GM"},
        {"easter_egg", "Easter Egg", 5, "GM"}, {"stpatricksday_leprechaundeco", "Leprechaun", 6, "GM"}, {"stpatricksday_potogolddeco", "Pot o' Gold", 6, "GM"},
    };
    for (auto& d : decos) add(d.id, "decorations", d.name, "Drag it into a tank to decorate. Tap a decoration to remove it.", d.cost, d.cur);
    // ---- bank (no real-money purchases in this port: currency exchange instead)
    add("bank_gems_small", "bank", "Handful of Gems", "Exchange 1,500 coins for 10 gems.", 1500, "CN", false, "", 1, 1, "iap_icon_handfulofgems");
    add("bank_gems_large", "bank", "Chest of Gems", "Exchange 7,500 coins for 60 gems.", 7500, "CN", false, "", 1, 1, "iap_icon_chestofgems");
    add("bank_coins_small", "bank", "Handful of Coins", "Exchange 10 gems for 1,000 coins.", 10, "GM", false, "", 1, 1, "iap_icon_handfulofcoins");
    add("bank_coins_large", "bank", "Chest of Coins", "Exchange 50 gems for 6,000 coins.", 50, "GM", false, "", 1, 1, "iap_icon_chestofcoins");
}

const std::vector<Item>& all() { init(); return g_items; }
const Item* item(const std::string& id) {
    init();
    auto it = g_idx.find(id);
    return it == g_idx.end() ? nullptr : &g_items[it->second];
}

static const char* PARTS[21] = {
    "Redtail", "Catfish", "Arrow", "Snout", "Chubby", "Betta", "Gold Shark", "Angel", "Leporinus", "Flash", "Carp",
    "Comet", "Stick", "Puffer", "Leaf", "Greenfin", "Golden Betta", "Pygmy", "Golden Stink", "Foxface", "Spotanus"};
std::string partName(int i) { return (i >= 0 && i < 21) ? PARTS[i] : "Fish"; }

std::string fishName(int b, int f) {
    int mk = Game::magicKind(b, f);
    if (mk >= 0) return magicName(mk);
    if (b == f) return std::string(PARTS[b]) + (std::string(PARTS[b]).find("Fish") == std::string::npos && b != 1 ? " Fish" : "");
    return std::string(PARTS[f]) + " " + PARTS[b];
}

int fishValue(int b, int f) {
    int mk = Game::magicKind(b, f);
    if (mk == MAGIC_GOLDENGUPPY) return 5000;
    if (mk >= 0) return 800;
    static const int pv[3] = {12, 35, 90};
    return pv[Game::partRarity(b)] + pv[Game::partRarity(f)] + (b == f ? 6 : 0);
}

const char* rarityName(int r) { return r == 2 ? "RARE" : r == 1 ? "UNCOMMON" : "COMMON"; }
const char* magicName(int k) {
    static const char* n[] = {"Magic Fish of Fertility", "Magic Fish of Nutrition", "Magic Fish of Curing", "Magic Fish of Growth",
                              "Magic Fish of Health", "Magic Fish of Mutation", "Golden Guppy of Isola"};
    return (k >= 0 && k < 7) ? n[k] : "Magic Fish";
}
const char* magicDesc(int k) {
    static const char* d[] = {
        "With this fish in your tank, pregnant fish give birth to extra fry.",
        "With this fish in your tank, fish will not lose health from starvation.",
        "With this fish in your tank, sick fish have a better chance of being cured.",
        "With this fish in your tank, fish grow up faster.",
        "With this fish in your tank, fish regain health over time.",
        "With this fish in your tank, newborn fish may mutate into new species.",
        "The Golden Guppy of Isola has been reborn! This legendary fish will certainly make you a rich breeder!"};
    return (k >= 0 && k < 7) ? d[k] : "";
}

// generic customer name lists (as used by the original store simulation)
static const char* FEMALE[] = {"MARY", "PATRICIA", "LINDA", "BARBARA", "ELIZABETH", "JENNIFER", "MARIA", "SUSAN", "MARGARET", "DOROTHY", "LISA", "NANCY", "KAREN", "BETTY", "HELEN", "SANDRA", "DONNA", "CAROL", "RUTH", "SHARON", "MICHELLE", "LAURA", "SARAH", "KIMBERLY", "DEBORAH", "JESSICA", "SHIRLEY", "CYNTHIA", "ANGELA", "MELISSA", "BRENDA", "AMY", "ANNA"};
static const char* MALE[] = {"JAMES", "JOHN", "ROBERT", "MICHAEL", "WILLIAM", "DAVID", "RICHARD", "CHARLES", "JOSEPH", "THOMAS", "CHRISTOPHER", "DANIEL", "PAUL", "MARK", "DONALD", "GEORGE", "KENNETH", "STEVEN", "EDWARD", "BRIAN", "RONALD", "ANTHONY", "KEVIN", "JASON", "MATTHEW", "GARY", "TIMOTHY", "JOSE", "LARRY", "JEFFREY", "FRANK", "SCOTT", "ERIC"};
static const char* LAST[] = {"SMITH", "JOHNSON", "WILLIAMS", "JONES", "BROWN", "DAVIS", "MILLER", "WILSON", "MOORE", "TAYLOR", "ANDERSON", "THOMAS", "JACKSON", "WHITE", "HARRIS", "MARTIN", "THOMPSON", "GARCIA", "MARTINEZ", "ROBINSON", "CLARK", "RODRIGUEZ", "LEWIS", "LEE", "WALKER", "HALL", "ALLEN", "YOUNG", "HERNANDEZ", "KING", "WRIGHT", "LOPEZ", "HILL", "SCOTT", "GREEN", "ADAMS", "BAKER", "NELSON"};
static const char* PROF[] = {"Plumber", "Astronaut", "Game Designer", "Manager", "Director", "Sales Agent", "Firefighter", "Florist", "Dentist", "Teacher", "Chef", "Pilot", "Librarian", "Nurse", "Mechanic", "Baker", "Architect", "Lifeguard", "Marine Biologist", "Ketchup Bottler", "Accountant", "Mail Carrier", "Painter", "Photographer", "Zookeeper"};
std::string customerName(int s) {
    unsigned u = (unsigned)s * 2654435761u;
    bool female = u & 1;
    std::string first = female ? FEMALE[(u >> 1) % (sizeof FEMALE / sizeof *FEMALE)] : MALE[(u >> 1) % (sizeof MALE / sizeof *MALE)];
    return first + " " + LAST[(u >> 9) % (sizeof LAST / sizeof *LAST)];
}
std::string profession(int s) { unsigned u = (unsigned)s * 40503u + 7; return PROF[(u >> 3) % (sizeof PROF / sizeof *PROF)]; }
}
