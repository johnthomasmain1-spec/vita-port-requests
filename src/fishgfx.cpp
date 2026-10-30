#include "fishgfx.h"
#include <cmath>
#include <map>
#include <algorithm>

namespace fishgfx {
struct PS { const Sprite* sp; float x, y, sx, sy, rot; int mouth; Color c; bool fx; };  // mouth: 0 any, 1 closed, 2 open
struct Part { std::vector<PS> s; bool ok = false; };
struct Frame { Part body, top, bottom, side, tail; float root[4][2] = {{0}}; bool hasRoot[4] = {false, false, false, false}; bool hasOpen = false; };
struct Species { std::string name; int type; Frame fr[8]; float hw = 60, hh = 30; };
static std::map<int, Species> g_sp;
static JV g_finFrames;

static const char* ROOTS[4] = {"TopFinRoot", "BottomFinRoot", "SideFinRoot", "TailRoot"};

static void flatten(const JV& n, float x, float y, float sx, float sy, Part& out, Frame* fr) {
    float px = n["p"][0].f() * sx + x, py = n["p"][1].f() * sy + y;
    float nsx = sx, nsy = sy;
    if (n.has("s")) { nsx *= n["s"][0].f(1); nsy *= n["s"][1].f(1); }
    const std::string& name = n["n"].str();
    if (fr) for (int r = 0; r < 4; r++) if (name == ROOTS[r]) { fr->root[r][0] = px; fr->root[r][1] = py; fr->hasRoot[r] = true; }
    for (auto& c : n["c"].a) {
        if (c["t"].str() != "sr") continue;
        PS p;
        p.sp = gfx::sprite(c["sp"].str());
        if (!p.sp) continue;
        p.x = px; p.y = py; p.sx = nsx; p.sy = nsy; p.rot = n["rz"].f();
        std::string ln = name; std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
        p.mouth = (ln.find("mouthopen") != std::string::npos || ln.find("mouth open") != std::string::npos) ? 2 :
                  (ln.find("mouth closed") != std::string::npos || ln.find("mouthclosed") != std::string::npos) ? 1 : 0;
        if (fr && p.mouth == 2) fr->hasOpen = true;
        p.c = mkcol(c["c"]); p.fx = c["fx"].i() != 0;
        out.s.push_back(p);
        out.ok = true;
    }
    for (auto& k : n["k"].a) flatten(k, px, py, nsx, nsy, out, fr);
}

bool load(const std::string& dd) {
    JV j = jload(dd + "/cfg/fish.json");
    if (!j.isObj()) return false;
    for (auto& s : j["species"].a) {
        Species sp; sp.name = s["name"].str(); sp.type = s["type"].i();
        for (int f = 0; f < 8 && f < (int)s["frames"].size(); f++) {
            const JV& fr = s["frames"][f];
            Frame& F = sp.fr[f];
            if (fr.has("body")) flatten(fr["body"], 0, 0, 1, 1, F.body, &F);
            if (fr.has("top")) flatten(fr["top"], 0, 0, 1, 1, F.top, nullptr);
            if (fr.has("bottom")) flatten(fr["bottom"], 0, 0, 1, 1, F.bottom, nullptr);
            if (fr.has("side")) flatten(fr["side"], 0, 0, 1, 1, F.side, nullptr);
            if (fr.has("tail")) flatten(fr["tail"], 0, 0, 1, 1, F.tail, nullptr);
        }
        // extents from frame 0 body
        float mx = 0, my = 0;
        for (auto& p : sp.fr[0].body.s) { mx = std::max(mx, fabsf(p.x) + p.sp->fw); my = std::max(my, fabsf(p.y) + p.sp->fh); }
        sp.hw = std::max(30.f, mx); sp.hh = std::max(20.f, my);
        g_sp[sp.type] = sp;
    }
    g_finFrames = j["finFrames"];
    return true;
}

std::string speciesName(int t) { auto it = g_sp.find(t); return it == g_sp.end() ? "" : it->second.name; }
void extents(int b, float& hw, float& hh) {
    auto it = g_sp.find(b);
    if (it == g_sp.end()) { hw = 60; hh = 30; return; }
    hw = it->second.hw; hh = it->second.hh;
}

static void drawPart(const Part& p, float rootX, float rootY, float rotDeg, float osx, float osy,
                     float cx, float cy, float k, bool right, Color c, int mouthState, bool hasOpen) {
    float rr = rotDeg * (float)M_PI / 180.f;
    float cs = cosf(rr), sn = sinf(rr);
    for (auto& s : p.s) {
        if (hasOpen && ((s.mouth == 2) != (mouthState == 2))) continue;
        // local offset from root, scaled and rotated (y-up)
        float lx = s.x * osx, ly = s.y * osy;
        float wx = rootX + lx * cs - ly * sn;
        float wy = rootY + lx * sn + ly * cs;
        if (right) wx = -wx;
        float sx = cx + wx * k, sy = cy - wy * k;
        float rot = -(rotDeg + s.rot);   // screen rotation is clockwise
        if (right) rot = -rot;
        gfx::drawSprite(s.sp, sx, sy, s.sx * osx * k * 2.f, s.sy * osy * k * 2.f, rot, c * s.c, right != s.fx);
    }
}

void draw(int bt, int ft, int frame, float x, float y, float k, bool right, float t, Color c, bool mouthOpen) {
    auto ib = g_sp.find(bt), iff = g_sp.find(ft);
    if (ib == g_sp.end()) return;
    if (iff == g_sp.end()) iff = ib;
    frame = std::max(0, std::min(7, frame));
    const Frame& B = ib->second.fr[frame];
    const Frame& F = iff->second.fr[frame];
    // fin edit offsets for (body species, fin species)
    float off[3][5] = {{0, 0, 0, 1, 1}, {0, 0, 0, 1, 1}, {0, 0, 0, 1, 1}};
    const JV* bodyE = g_finFrames.find(ib->second.name.c_str());
    if (bodyE) {
        const JV* fe = bodyE->find(iff->second.name.c_str());
        if (fe && frame < (int)fe->size()) {
            for (int i = 0; i < 3; i++)
                for (int q = 0; q < 5; q++) off[i][q] = (*fe)[frame][i][q].f(q >= 3 ? 1.f : 0.f);
        }
    }
    bool swim = frame == 0;
    float w1 = swim ? sinf(t * 7.0f) : 0, w2 = swim ? sinf(t * 5.0f + 1.3f) : 0, w3 = swim ? sinf(t * 9.0f + 0.4f) : 0;
    int mouth = mouthOpen ? 2 : 1;
    auto rootOf = [&](int r, float& rx, float& ry) { rx = B.root[r][0]; ry = B.root[r][1]; return B.hasRoot[r]; };
    float rx, ry;
    // draw order: top fin, bottom fin, tail, body, side fin
    if (F.top.ok && rootOf(0, rx, ry))
        drawPart(F.top, rx + off[0][0], ry + off[0][1], off[0][2] + w2 * 3.f, off[0][3], off[0][4], x, y, k, right, c, mouth, false);
    if (F.bottom.ok && rootOf(1, rx, ry))
        drawPart(F.bottom, rx + off[1][0], ry + off[1][1], off[1][2] - w2 * 3.f, off[1][3], off[1][4], x, y, k, right, c, mouth, false);
    if (F.tail.ok && rootOf(3, rx, ry))
        drawPart(F.tail, rx, ry, w1 * 6.f, 1, 1, x, y, k, right, c, mouth, false);
    drawPart(B.body, 0, 0, 0, 1, 1, x, y, k, right, c, mouth, B.hasOpen);
    if (F.side.ok && rootOf(2, rx, ry))
        drawPart(F.side, rx + off[2][0], ry + off[2][1], off[2][2] + w3 * 9.f, off[2][3], off[2][4], x, y, k, right, c, mouth, false);
}
}  // namespace fishgfx
