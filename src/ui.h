#pragma once
#include "gfx.h"
#include <functional>
#include <memory>

struct UINode {
    std::string name;
    bool active = true;
    bool isRect = false;
    float amin[2] = {.5f, .5f}, amax[2] = {.5f, .5f}, apos[2] = {0, 0}, size[2] = {0, 0}, pivot[2] = {.5f, .5f};
    float lpos[2] = {0, 0};
    float scale[2] = {1, 1};
    float rotz = 0;

    bool hasImg = false;
    const Sprite* sp = nullptr;
    Color imgColor;
    int imgType = 0; float fill = 1; int fillMethod = 0, fillOrigin = 0; bool preserve = false, fillCenter = true, raycast = false;

    bool hasTxt = false;
    std::string text;
    Color txtColor;
    std::string font = "INSANIBU";
    float fontSize = 14; int fontStyle = 0; bool bestFit = false; float minSize = 2, maxSize = 40; int align = 0; int hOverflow = 0, vOverflow = 0; float lineSpacing = 1;
    bool outline = false, shadow = false; Color effColor; float effDist[2] = {1, -1};

    bool button = false, interactable = true;
    bool mask = false;
    bool scroll = false, scrollH = false, scrollV = false;
    int linear = 0;  // 1 horizontal, 2 vertical layout group
    bool grid = false; float gPad[4] = {0, 0, 0, 0}, gCell[2] = {100, 100}, gSpacing[2] = {0, 0}; int gAlign = 0, gConstraint = 0, gCount = 1, gAxis = 0;
    std::function<void(float dx, float dy, int phase)> onScroll;   // phase 0 start, 1 move, 2 end
    float cgAlpha = -1;    // canvas group alpha (-1 = none)
    std::vector<std::string> scripts;

    UINode* parent = nullptr;
    std::vector<UINode*> kids;

    // runtime
    std::function<void()> onClick;
    std::function<void(UINode*, float, float)> onPress;  // pressed (for drag starts)
    void* user = nullptr;
    int tag = 0;
    float alphaMul = 1;
    Color tintMul;
    std::function<void(UINode*, FRect)> customDraw;  // extra drawing after node visuals
    bool customOnly = false;  // skip default visuals

    // computed
    FRect rect;           // screen rect
    float wsx = .5f, wsy = .5f;  // screen px per local unit
    float drawAlpha = 1;

    UINode* child(const std::string& n) const;          // direct child by name
    UINode* find(const std::string& path);               // path "A/B/C" relative to this; also searches descendants if path has no '/'
    UINode* findDeep(const std::string& n);              // first descendant with name
    bool hasScript(const std::string& s) const;
    void setText(const std::string& t) { text = t; }
    void show(bool v) { active = v; }
    bool visibleInTree() const;
};

namespace ui {
extern UINode* root;
extern const float CANVAS_W, CANVAS_H;

bool load(const std::string& path, const std::string& prefabPath);
UINode* build(const JV& j, UINode* parent);                     // build subtree from json
UINode* instantiate(const std::string& prefabName, UINode* parent);  // clone a runtime prefab
UINode* clone(UINode* src, UINode* parent);
void destroy(UINode* n);
void layout();
void draw(UINode* n = nullptr);

// input; returns true if UI consumed the event
bool pointerDown(float x, float y);
bool pointerMove(float x, float y);
bool pointerUp(float x, float y);
bool hitAny(float x, float y);       // true if a raycast target is under the point
UINode* hitNode(float x, float y);   // top-most raycast target
UINode* pressed();
// arrange active children of a GridLayoutGroup node; returns content size (canvas units)
void linearArrange(UINode* g);
void gridArrange(UINode* g, float* contentW = nullptr, float* contentH = nullptr);
void setRect(UINode* n, float ax0, float ay0, float ax1, float ay1, float px, float py, float w, float h, float pivx = .5f, float pivy = .5f);
extern std::function<void()> clickSound;
}
