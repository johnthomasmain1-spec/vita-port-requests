#pragma once
#include <string>
#include <vector>

// Stand-in for the server-delivered store catalogue & fish definitions
// (in the original these come from PlayFab title data, not the APK).
namespace catalog {
struct Item {
    std::string id, tab, name, desc, currency;
    int cost = 0;
    bool permanent = false;   // upgrades / research
    std::string prereq;
    int doses = 1;            // uses per purchase
    int amount = 1;           // quantity per purchase
    std::string icon;         // sprite name
};
const std::vector<Item>& all();
const Item* item(const std::string& id);
std::string fishName(int body, int fin);
std::string partName(int idx);
int fishValue(int body, int fin);
const char* rarityName(int r);
const char* magicName(int k);
const char* magicDesc(int k);
std::string customerName(int seed);
std::string profession(int seed);
}
