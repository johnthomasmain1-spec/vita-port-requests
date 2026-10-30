#include "gfx.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <map>
#include <algorithm>

namespace gfx {
SDL_Renderer* R = nullptr;
SDL_Window* W = nullptr;
std::string dataDir;

static std::unordered_map<std::string, SDL_Texture*> g_tex;
static std::unordered_map<std::string, Sprite> g_sprites;
static std::unordered_map<std::string, std::string> g_byName;
static std::vector<SDL_Rect> g_clip;
static unsigned g_frame = 0;

SDL_Texture* loadTexture(const std::string& rel) {
    auto it = g_tex.find(rel);
    if (it != g_tex.end()) return it->second;
    std::string p = dataDir + "/" + rel;
    SDL_Surface* s = IMG_Load(p.c_str());
    SDL_Texture* t = nullptr;
    if (s) {
        t = SDL_CreateTextureFromSurface(R, s);
        SDL_FreeSurface(s);
        if (t) { SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND); SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear); }
    } else {
        SDL_Log("loadTexture failed %s: %s", p.c_str(), IMG_GetError());
    }
    g_tex[rel] = t;
    return t;
}

bool init(const std::string& dd) {
    dataDir = dd;
    JV js = jload(dd + "/cfg/sprites.json");
    if (!js.isObj()) { SDL_Log("missing sprites.json in %s", dd.c_str()); return false; }
    for (auto& kv : js.o) {
        const JV& v = kv.second;
        Sprite s;
        std::string a = v["a"].str();
        std::string file = a.find('.') == std::string::npos ? "gfx/" + a + ".png" : "gfx/" + a;
        s.tex = loadTexture(file);
        s.src = {v["x"].i(), v["y"].i(), v["w"].i(), v["h"].i()};
        s.fw = v["fw"].f(); s.fh = v["fh"].f(); s.ox = v["ox"].f(); s.oy = v["oy"].f();
        s.px = v["px"].f(.5f); s.py = v["py"].f(.5f); s.ppu = v["ppu"].f(1);
        for (int i = 0; i < 4; i++) s.b[i] = v["b"][i].f();
        s.name = v["n"].str();
        if (!g_byName.count(s.name)) g_byName[s.name] = kv.first;
        g_sprites[kv.first] = s;
    }
    TTF_Init();
    return true;
}

void shutdown() {
    for (auto& t : g_tex) if (t.second) SDL_DestroyTexture(t.second);
    g_tex.clear();
}

const Sprite* sprite(const std::string& key) {
    auto it = g_sprites.find(key);
    return it == g_sprites.end() ? nullptr : &it->second;
}
const Sprite* spriteByName(const std::string& name) {
    auto it = g_byName.find(name);
    return it == g_byName.end() ? nullptr : sprite(it->second);
}

static inline Uint8 cb(float v) { return (Uint8)std::max(0.f, std::min(255.f, v * 255.f + .5f)); }
static void tint(SDL_Texture* t, Color c) {
    SDL_SetTextureColorMod(t, cb(c.r), cb(c.g), cb(c.b));
    SDL_SetTextureAlphaMod(t, cb(c.a));
}

void drawSprite(const Sprite* s, float x, float y, float sx, float sy, float rot, Color c, bool flipX, SDL_BlendMode bm) {
    if (!s || !s->tex || c.a <= 0.003f) return;
    // trimmed quad in sprite-local, y-up, relative to pivot
    float lx0 = -s->px * s->fw + s->ox;
    float ly0 = -s->py * s->fh + s->oy;   // bottom
    float w = s->src.w, h = s->src.h;
    if (flipX) lx0 = -(lx0 + w);
    SDL_FRect d;
    d.x = x + lx0 * sx;
    d.y = y - (ly0 + h) * sy;
    d.w = w * sx; d.h = h * sy;
    SDL_FPoint ctr{x - d.x, y - d.y};
    tint(s->tex, c);
    SDL_SetTextureBlendMode(s->tex, bm);
    SDL_RenderCopyExF(R, s->tex, &s->src, &d, rot, &ctr, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    if (bm != SDL_BLENDMODE_BLEND) SDL_SetTextureBlendMode(s->tex, SDL_BLENDMODE_BLEND);
}

void drawSpriteRect(const Sprite* s, FRect r, Color c, bool flipX) {
    if (!s || !s->tex || c.a <= 0.003f || r.w <= 0 || r.h <= 0) return;
    // account for trimming: map full rect to r
    float kx = r.w / std::max(1.f, s->fw), ky = r.h / std::max(1.f, s->fh);
    SDL_FRect d;
    float ox = flipX ? (s->fw - s->ox - s->src.w) : s->ox;
    d.x = r.x + ox * kx;
    d.y = r.y + (s->fh - s->oy - s->src.h) * ky;
    d.w = s->src.w * kx; d.h = s->src.h * ky;
    tint(s->tex, c);
    SDL_RenderCopyExF(R, s->tex, &s->src, &d, 0, nullptr, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void drawSliced(const Sprite* s, FRect r, float bs, Color c, bool fillCenter) {
    if (!s || !s->tex || c.a <= 0.003f) return;
    float L = s->b[0], B = s->b[1], Rb = s->b[2], T = s->b[3];
    if (L + Rb + B + T <= 0.01f) { drawSpriteRect(s, r, c); return; }
    // destination borders (screen px)
    float dl = L * bs, dr = Rb * bs, db = B * bs, dt = T * bs;
    if (dl + dr > r.w && dl + dr > 0) { float k = r.w / (dl + dr); dl *= k; dr *= k; }
    if (dt + db > r.h && dt + db > 0) { float k = r.h / (dt + db); dt *= k; db *= k; }
    float sxs[4] = {0, L, s->src.w - Rb, (float)s->src.w};
    float sys[4] = {0, T, s->src.h - B, (float)s->src.h};
    float dxs[4] = {r.x, r.x + dl, r.x + r.w - dr, r.x + r.w};
    float dys[4] = {r.y, r.y + dt, r.y + r.h - db, r.y + r.h};
    tint(s->tex, c);
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) {
            if (!fillCenter && i == 1 && j == 1) continue;
            SDL_Rect sr{s->src.x + (int)sxs[i], s->src.y + (int)sys[j], (int)(sxs[i + 1] - sxs[i]), (int)(sys[j + 1] - sys[j])};
            SDL_FRect dr2{dxs[i], dys[j], dxs[i + 1] - dxs[i], dys[j + 1] - dys[j]};
            if (sr.w <= 0 || sr.h <= 0 || dr2.w <= 0 || dr2.h <= 0) continue;
            SDL_RenderCopyF(R, s->tex, &sr, &dr2);
        }
}

void drawFilled(const Sprite* s, FRect r, int method, int origin, float amt, Color c) {
    if (!s || !s->tex) return;
    amt = std::max(0.f, std::min(1.f, amt));
    if (amt <= 0) return;
    SDL_Rect sr = s->src;
    SDL_FRect d{r.x, r.y, r.w, r.h};
    if (method == 0) {          // horizontal
        int sw = (int)(sr.w * amt);
        if (origin == 1) { sr.x += sr.w - sw; d.x += r.w * (1 - amt); }
        sr.w = sw; d.w = r.w * amt;
    } else if (method == 1) {   // vertical
        int sh = (int)(sr.h * amt);
        if (origin == 0) { sr.y += sr.h - sh; d.y += r.h * (1 - amt); }
        sr.h = sh; d.h = r.h * amt;
    } else {                    // radial → approximate as vertical
        int sh = (int)(sr.h * amt);
        sr.y += sr.h - sh; d.y += r.h * (1 - amt); sr.h = sh; d.h = r.h * amt;
    }
    tint(s->tex, c);
    SDL_RenderCopyF(R, s->tex, &sr, &d);
}

void fillRect(FRect r, Color c) {
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(R, cb(c.r), cb(c.g), cb(c.b), cb(c.a));
    SDL_FRect d{r.x, r.y, r.w, r.h};
    SDL_RenderFillRectF(R, &d);
}

void pushClip(FRect r) {
    SDL_Rect c{(int)floorf(r.x), (int)floorf(r.y), (int)ceilf(r.w), (int)ceilf(r.h)};
    if (!g_clip.empty()) {
        SDL_Rect o; if (!SDL_IntersectRect(&c, &g_clip.back(), &o)) o = {0, 0, 0, 0};
        c = o;
    }
    g_clip.push_back(c);
    SDL_RenderSetClipRect(R, &c);
}
void popClip() {
    if (!g_clip.empty()) g_clip.pop_back();
    if (g_clip.empty()) SDL_RenderSetClipRect(R, nullptr);
    else SDL_RenderSetClipRect(R, &g_clip.back());
}

// ------------------------------------------------------------------ text
struct FontKey { std::string f; int sz; int st; bool operator<(const FontKey& o) const { return f != o.f ? f < o.f : (sz != o.sz ? sz < o.sz : st < o.st); } };
static std::map<FontKey, TTF_Font*> g_fonts;
static TTF_Font* font(const std::string& name, int sz, int style) {
    sz = std::max(4, std::min(160, sz));
    FontKey k{name, sz, style};
    auto it = g_fonts.find(k);
    if (it != g_fonts.end()) return it->second;
    std::string file = name == "INSANIBU" ? "INSANIBU" : "ARIAL";
    std::string p = dataDir + "/fonts/" + file + ".ttf";
    TTF_Font* f = TTF_OpenFont(p.c_str(), sz);
    if (!f) { p = dataDir + "/fonts/ARIAL.ttf"; f = TTF_OpenFont(p.c_str(), sz); }
    if (f && (style & 1)) TTF_SetFontStyle(f, TTF_STYLE_BOLD);
    g_fonts[k] = f;
    return f;
}
struct TexEntry { SDL_Texture* t; int w, h; unsigned used; };
static std::unordered_map<std::string, TexEntry> g_txt;
static TexEntry* textTex(const std::string& s, const std::string& fname, int sz, int style) {
    std::string key = fname + "|" + std::to_string(sz) + "|" + std::to_string(style) + "|" + s;
    auto it = g_txt.find(key);
    if (it != g_txt.end()) { it->second.used = g_frame; return &it->second; }
    TTF_Font* f = font(fname, sz, style);
    if (!f || s.empty()) return nullptr;
    SDL_Color white{255, 255, 255, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s.c_str(), white);
    if (!surf) return nullptr;
    TexEntry e{SDL_CreateTextureFromSurface(R, surf), surf->w, surf->h, g_frame};
    SDL_FreeSurface(surf);
    if (e.t) SDL_SetTextureBlendMode(e.t, SDL_BLENDMODE_BLEND);
    return &(g_txt[key] = e);
}
void frameEnd() {
    g_frame++;
    if (g_txt.size() > 700) {
        for (auto it = g_txt.begin(); it != g_txt.end();) {
            if (g_frame - it->second.used > 120) { SDL_DestroyTexture(it->second.t); it = g_txt.erase(it); }
            else ++it;
        }
    }
}

struct Run { std::string s; Color c; float size; bool hasColor; };
static bool parseHex(const std::string& h, Color& c) {
    std::string x = h;
    if (!x.empty() && x[0] == '#') x = x.substr(1);
    if (x.size() != 6 && x.size() != 8) return false;
    unsigned v = strtoul(x.c_str(), nullptr, 16);
    if (x.size() == 6) v = (v << 8) | 0xFF;
    c.r = ((v >> 24) & 255) / 255.f; c.g = ((v >> 16) & 255) / 255.f; c.b = ((v >> 8) & 255) / 255.f; c.a = (v & 255) / 255.f;
    return true;
}
static void parseRich(const std::string& t, Color base, float size, std::vector<Run>& out) {
    std::vector<Color> cs{base}; std::vector<float> ss{size};
    std::string cur;
    auto flush = [&]() { if (!cur.empty()) { out.push_back({cur, cs.back(), ss.back(), cs.size() > 1}); cur.clear(); } };
    for (size_t i = 0; i < t.size();) {
        if (t[i] == '<') {
            size_t e = t.find('>', i);
            if (e != std::string::npos) {
                std::string tag = t.substr(i + 1, e - i - 1);
                bool known = true;
                if (tag.rfind("color=", 0) == 0) { flush(); Color c = base; parseHex(tag.substr(6), c); c.a *= base.a; cs.push_back(c); }
                else if (tag == "/color") { flush(); if (cs.size() > 1) cs.pop_back(); }
                else if (tag.rfind("size=", 0) == 0) { flush(); ss.push_back(atof(tag.c_str() + 5) * 0.5f); }
                else if (tag == "/size") { flush(); if (ss.size() > 1) ss.pop_back(); }
                else if (tag == "b" || tag == "/b" || tag == "i" || tag == "/i") {}
                else known = false;
                if (known) { i = e + 1; continue; }
            }
        }
        cur += t[i++];
    }
    flush();
}

struct Piece { std::string s; Color c; float size; float w, h; bool space; bool br; };

static void layoutPieces(const std::vector<Run>& runs, const std::string& fname, int style, std::vector<Piece>& out) {
    for (auto& r : runs) {
        std::string word;
        auto push = [&](bool space, bool br) {
            if (!word.empty() || br) {
                Piece p{word, r.c, r.size, 0, 0, space, br};
                if (!word.empty()) {
                    TTF_Font* f = font(fname, (int)lroundf(r.size), style);
                    int w = 0, h = 0; if (f) TTF_SizeUTF8(f, word.c_str(), &w, &h);
                    p.w = (float)w; p.h = f ? (float)TTF_FontLineSkip(f) : r.size;
                } else { TTF_Font* f = font(fname, (int)lroundf(r.size), style); p.h = f ? (float)TTF_FontLineSkip(f) : r.size; }
                out.push_back(p);
            }
            word.clear();
        };
        for (char ch : r.s) {
            if (ch == ' ') { word += ' '; push(true, false); }
            else if (ch == '\n') { push(false, false); push(false, true); }
            else word += ch;
        }
        push(false, false);
    }
}

struct Line { std::vector<int> pcs; float w = 0, h = 0; };
static void breakLines(std::vector<Piece>& pcs, float maxW, bool wrap, std::vector<Line>& lines) {
    lines.clear(); lines.emplace_back();
    for (int i = 0; i < (int)pcs.size(); i++) {
        Piece& p = pcs[i];
        if (p.br) { if (lines.back().h == 0) lines.back().h = p.h; lines.emplace_back(); continue; }
        float pw = p.w;
        if (wrap && !lines.back().pcs.empty() && lines.back().w + pw > maxW + 0.5f) {
            // trim trailing space width approx (ignored)
            lines.emplace_back();
        }
        lines.back().pcs.push_back(i);
        lines.back().w += pw;
        lines.back().h = std::max(lines.back().h, p.h);
    }
}

float measureText(const std::string& text, const std::string& fname, float size, float* h) {
    std::vector<Run> runs; parseRich(text, Color(), size, runs);
    std::vector<Piece> pcs; layoutPieces(runs, fname, 0, pcs);
    std::vector<Line> lines; breakLines(pcs, 1e9f, false, lines);
    float w = 0, th = 0;
    for (auto& l : lines) { w = std::max(w, l.w); th += l.h; }
    if (h) *h = th;
    return w;
}

void drawText(const std::string& text, FRect r, const TextStyle& st) {
    if (text.empty() || st.color.a <= 0.003f) return;
    float size = st.size;
    std::vector<Run> runs; std::vector<Piece> pcs; std::vector<Line> lines;
    auto build = [&](float sz) {
        runs.clear(); pcs.clear();
        parseRich(text, st.color, sz, runs);
        layoutPieces(runs, st.font, st.style, pcs);
        breakLines(pcs, r.w, st.wrap, lines);
    };
    if (st.bestFit) {
        float hi = st.maxSize, lo = st.minSize;
        size = hi;
        for (int it = 0; it < 12; it++) {
            build(size);
            float tw = 0, th = 0;
            for (auto& l : lines) { tw = std::max(tw, l.w); th += l.h * st.lineSpacing; }
            if ((tw <= r.w + 1 || !st.wrap) && th <= r.h + 1) { if (size >= hi - 0.5f) break; lo = size; }
            else hi = size;
            if (hi - lo < 0.75f) { size = lo; build(size); break; }
            size = (hi + lo) * 0.5f;
        }
    } else build(size);
    float th = 0; for (auto& l : lines) th += l.h * st.lineSpacing;
    int va = st.align / 3, ha = st.align % 3;
    float y = r.y + (va == 0 ? 0 : va == 1 ? (r.h - th) * 0.5f : r.h - th);
    for (auto& l : lines) {
        float x = r.x + (ha == 0 ? 0 : ha == 1 ? (r.w - l.w) * 0.5f : r.w - l.w);
        for (int pi : l.pcs) {
            Piece& p = pcs[pi];
            TexEntry* te = textTex(p.s, st.font, (int)lroundf(p.size), st.style);
            if (te && te->t) {
                float yy = y + (l.h - p.h);
                SDL_FRect d{x, yy, (float)te->w, (float)te->h};
                if (st.shadow) {
                    Color sc = st.shadowColor; sc.a *= p.c.a;
                    tint(te->t, sc);
                    SDL_FRect d2 = d; d2.x += st.shadowDx; d2.y += st.shadowDy;
                    SDL_RenderCopyF(R, te->t, nullptr, &d2);
                }
                if (st.outline) {
                    Color oc = st.outlineColor; oc.a *= p.c.a;
                    tint(te->t, oc);
                    float o = st.outlineDist;
                    const float offs[8][2] = {{-o, 0}, {o, 0}, {0, -o}, {0, o}, {-o, -o}, {o, o}, {-o, o}, {o, -o}};
                    for (auto& of : offs) { SDL_FRect d2 = d; d2.x += of[0]; d2.y += of[1]; SDL_RenderCopyF(R, te->t, nullptr, &d2); }
                }
                tint(te->t, p.c);
                SDL_RenderCopyF(R, te->t, nullptr, &d);
            }
            x += p.w;
        }
        y += l.h * st.lineSpacing;
    }
}
}  // namespace gfx
