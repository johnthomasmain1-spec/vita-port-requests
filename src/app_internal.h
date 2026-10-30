#pragma once
#include "app.h"
#include "gfx.h"
#include "ui.h"
#include "game.h"
#include "catalog.h"
#include "fishgfx.h"
#include "audio.h"
#include <map>
#include <vector>
#include <functional>

namespace app {
enum Screen { SCR_MENU, SCR_GAME, SCR_STORE };

// world (tank) scene node
struct WNode {
    std::string name;
    float x = 0, y = 0, z = 0, sx = 1, sy = 1, rz = 0;
    bool active = true;
    const Sprite* sp = nullptr; Color c; bool flip = false; int order = 0;
    std::vector<WNode> kids;
};
WNode buildW(const JV& j);
WNode* findW(WNode& n, const std::string& name);
struct WDraw { const Sprite* sp; float x, y, z, sx, sy, rz; Color c; bool flip; int order; SDL_BlendMode bm; };
void collectW(const WNode& n, float px, float py, float pz, float psx, float psy, std::vector<WDraw>& out, Color tint = Color());

extern const float WS;     // screen px per world unit
inline float wsx(float x) { return SCREEN_W * .5f + x * WS; }
inline float wsy(float y) { return SCREEN_H * .5f - y * WS; }
inline float swx(float x) { return (x - SCREEN_W * .5f) / WS; }
inline float swy(float y) { return (SCREEN_H * .5f - y) / WS; }

struct FishVis {
    float x = 0, y = 0, vx = 0, vy = 0, tx = 0, ty = 0;
    bool right = false;
    float turnT = -1;    // >=0 while turning
    bool turnTo = false;
    float mouthT = 0, rest = 0, wobble = 0;
    float bornFlash = 0;
    bool init = false;
};

struct State {
    Screen screen = SCR_MENU;
    int tank = 0;                 // current tank index 0..2
    int selFish = -1;             // selected fish id
    std::map<int, FishVis> vis;   // fish visuals by id
    float time = 0;
    float tickTimer = 0, saveTimer = 0;
    std::string saveDir, dataDir;
    bool quit = false;
    // drag & drop
    int dragFish = -1; std::string dragItem; float dragX = 0, dragY = 0; bool dragging = false; float pressX = 0, pressY = 0;
    int dragDeco = -1;
    bool pointerDown = false;
    // breeding
    bool breeding = false; float breedStart = 0; int breedDuration = 3;
    std::map<int, int> homeTank;  // fish id -> tank it came from (for breeding/sales)
    // overlays
    bool storeOpen = false, speciesOpen = false, tasksOpen = false, optionsOpen = false, alertOpen = false, invOpen = false;
    std::vector<std::string> newSpecies;
    std::vector<int> newMagic;
    float toastT = 0; std::string toast;
    bool showInfo = false;
};
extern State S;
extern WNode g_tank, g_store;

// core
UINode* N(const std::string& path);     // ui node by path from CanvasRoot (cached)
void setText(const std::string& path, const std::string& t);
void show(const std::string& path, bool v);
void onClick(const std::string& path, std::function<void()> f);
std::string fmtNum(int v);
void save();
void showToast(const std::string& t);
void alert(const std::string& title, const std::string& text, const std::string& b1 = "OK", std::function<void()> f1 = nullptr,
           const std::string& b2 = "", std::function<void()> f2 = nullptr);
void drawFishInRect(int body, int fin, int age, FRect r, float t, bool facingRight = false);
float fishScaleForAge(int age);
void artFor(int body, int fin, int age, int& b, int& f);

// screens
void initScreens();
void refreshHUD();
void openStore(const std::string& tab);
void closeStore();
void openSpecies(int fishId);
void closeSpecies();
void openTasks();
void closeTasks();
void openOptions();
void enterFishStore();
void leaveFishStore();
void updateFishStore(float dt);
void drawFishStoreWorld();
void refreshFishStoreUI();
void refreshInventory();
void refreshTankButtons();
void refreshFishInfo();
void showDiscoveries();
void updateScreens(float dt);
bool backPressed();
}
