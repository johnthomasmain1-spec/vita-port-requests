#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "json.h"

#define SCREEN_W 960
#define SCREEN_H 544

struct Color { float r = 1, g = 1, b = 1, a = 1; };
inline Color mkcol(const JV& v) { Color c; if (v.isArr() && v.size() >= 4) { c.r = v[0].f(); c.g = v[1].f(); c.b = v[2].f(); c.a = v[3].f(); } return c; }
inline Color operator*(Color a, Color b) { return {a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a}; }

struct Sprite {
    SDL_Texture* tex = nullptr;
    SDL_Rect src{0, 0, 0, 0};
    float fw = 0, fh = 0;   // full (untrimmed) size in atlas pixels
    float ox = 0, oy = 0;   // trimmed rect offset inside full rect (from bottom-left, y-up)
    float px = .5f, py = .5f;
    float b[4] = {0, 0, 0, 0}; // border l,b,r,t (atlas px)
    float ppu = 1;
    std::string name;
};

struct FRect { float x = 0, y = 0, w = 0, h = 0; bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

namespace gfx {
extern SDL_Renderer* R;
extern SDL_Window* W;
extern std::string dataDir;

bool init(const std::string& dataDir);
void shutdown();
const Sprite* sprite(const std::string& key);         // by sprite key "file:pid"
const Sprite* spriteByName(const std::string& name);  // by unity sprite name (first match)
SDL_Texture* loadTexture(const std::string& relpath);

// world/sprite drawing: (x,y) = screen position of the sprite pivot. scale in screen px per atlas px.
void drawSprite(const Sprite* s, float x, float y, float sx, float sy, float rotDeg = 0, Color c = Color(), bool flipX = false, SDL_BlendMode bm = SDL_BLENDMODE_BLEND);
// draw sprite stretched into screen rect
void drawSpriteRect(const Sprite* s, FRect r, Color c = Color(), bool flipX = false);
// 9-slice; border scale converts atlas-px borders to screen px
void drawSliced(const Sprite* s, FRect r, float borderScale, Color c = Color(), bool fillCenter = true);
void drawFilled(const Sprite* s, FRect r, int method, int origin, float amount, Color c = Color());
void fillRect(FRect r, Color c);

void pushClip(FRect r);
void popClip();

// ---- text
enum Align { UL = 0, UC, UR, ML, MC, MR, LL, LC, LR };
struct TextStyle {
    std::string font = "INSANIBU";
    float size = 20;      // screen px
    Color color;
    int align = MC;
    bool wrap = true;
    bool bestFit = false;
    float minSize = 6, maxSize = 40;
    bool outline = false; Color outlineColor; float outlineDist = 1;
    bool shadow = false; Color shadowColor; float shadowDx = 0, shadowDy = 1;
    float lineSpacing = 1;
    int style = 0; // 1 bold
};
void drawText(const std::string& text, FRect r, const TextStyle& st);
float measureText(const std::string& text, const std::string& font, float size, float* h = nullptr);
void frameEnd();  // cache housekeeping
}
