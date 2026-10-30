#include "ui.h"
#include <cmath>
#include <algorithm>

namespace ui {
UINode* root = nullptr;
const float CANVAS_W = 1920.f, CANVAS_H = 1088.f;
std::function<void()> clickSound;
static JV g_prefabs;
static std::vector<UINode*> g_drawList;  // visible nodes in draw order (for hit testing)
static UINode* g_pressed = nullptr;
static float g_pressX = 0, g_pressY = 0;
static bool g_dragScroll = false;
static UINode* g_scrollNode = nullptr;
static float g_scrollStart[2];

static Color colOr(const JV& v, Color d = Color()) { return v.isArr() ? mkcol(v) : d; }

UINode* build(const JV& j, UINode* parent) {
    UINode* n = new UINode();
    n->name = j["n"].str();
    n->active = !j.has("off");
    n->parent = parent;
    if (j.has("r")) {
        const JV& r = j["r"];
        n->isRect = true;
        n->amin[0] = r[0].f(); n->amin[1] = r[1].f(); n->amax[0] = r[2].f(); n->amax[1] = r[3].f();
        n->apos[0] = r[4].f(); n->apos[1] = r[5].f(); n->size[0] = r[6].f(); n->size[1] = r[7].f();
        n->pivot[0] = r[8].f(); n->pivot[1] = r[9].f();
    }
    if (j.has("p")) { n->lpos[0] = j["p"][0].f(); n->lpos[1] = j["p"][1].f(); }
    if (j.has("s")) { n->scale[0] = j["s"][0].f(); n->scale[1] = j["s"][1].f(); }
    n->rotz = j["rz"].f();
    for (auto& c : j["c"].a) {
        const std::string& t = c["t"].str();
        if (t == "img") {
            n->hasImg = true;
            if (c.has("sp")) n->sp = gfx::sprite(c["sp"].str());
            n->imgColor = colOr(c["c"]);
            n->imgType = c["ty"].i();
            n->fill = c["fa"].f(1); n->fillMethod = c["fm"].i(); n->fillOrigin = c["fo"].i();
            n->preserve = c.has("pa"); n->fillCenter = !c.has("nofill"); n->raycast = c["ray"].i() != 0;
        } else if (t == "txt") {
            n->hasTxt = true;
            n->text = c["tx"].str();
            n->txtColor = colOr(c["c"]);
            n->font = c["f"].str() == "INSANIBU" ? "INSANIBU" : "ARIAL";
            n->fontSize = c["sz"].f(14); n->fontStyle = c["st"].i();
            n->bestFit = c["bf"].i() != 0; n->minSize = c["mn"].f(2); n->maxSize = c["mx"].f(40);
            n->align = c["al"].i(); n->hOverflow = c["ho"].i(); n->vOverflow = c["vo"].i(); n->lineSpacing = c["ls"].f(1);
        } else if (t == "outline") { n->outline = true; n->effColor = colOr(c["c"]); n->effDist[0] = c["d"][0].f(1); n->effDist[1] = c["d"][1].f(-1); }
        else if (t == "shadow") { if (!n->shadow) { n->shadow = true; n->effColor = colOr(c["c"]); n->effDist[0] = c["d"][0].f(1); n->effDist[1] = c["d"][1].f(-1); } }
        else if (t == "btn") { n->button = true; n->interactable = c["i"].i(1) != 0; }
        else if (t == "toggle") { n->button = true; }
        else if (t == "mask") n->mask = true;
        else if (t == "hlg") n->linear = 1;
        else if (t == "vlg") n->linear = 2;
        else if (t == "grid") {
            n->grid = true;
            for (int q = 0; q < 4; q++) n->gPad[q] = c["pad"][q].f();
            n->gCell[0] = c["cell"][0].f(100); n->gCell[1] = c["cell"][1].f(100);
            n->gSpacing[0] = c["sp"][0].f(); n->gSpacing[1] = c["sp"][1].f();
            n->gAlign = c["ca"].i(); n->gConstraint = c["con"].i(); n->gCount = std::max(1, c["cc"].i(1)); n->gAxis = c["ax"].i();
        }
        else if (t == "scroll") { n->scroll = true; n->scrollH = c["h"].i() != 0; n->scrollV = c["v"].i() != 0; }
        else if (t == "cg") n->cgAlpha = c["a"].f(1);
        else n->scripts.push_back(t);
    }
    for (auto& k : j["k"].a) n->kids.push_back(build(k, n));
    return n;
}

bool load(const std::string& path, const std::string& prefabPath) {
    JV j = jload(path);
    if (!j.isObj()) return false;
    root = build(j, nullptr);
    root->isRect = true;
    root->amin[0] = root->amin[1] = 0; root->amax[0] = root->amax[1] = 0;
    root->size[0] = CANVAS_W; root->size[1] = CANVAS_H; root->pivot[0] = root->pivot[1] = 0;
    root->apos[0] = root->apos[1] = 0;
    root->scale[0] = root->scale[1] = 1;
    g_prefabs = jload(prefabPath);
    return true;
}

UINode* instantiate(const std::string& name, UINode* parent) {
    const JV* j = g_prefabs.find(name.c_str());
    if (!j) return nullptr;
    UINode* n = build(*j, parent);
    n->active = true;
    if (parent) parent->kids.push_back(n);
    return n;
}

static UINode* cloneRec(UINode* s, UINode* parent) {
    UINode* n = new UINode(*s);
    n->parent = parent; n->kids.clear();
    for (UINode* k : s->kids) n->kids.push_back(cloneRec(k, n));
    return n;
}
UINode* clone(UINode* src, UINode* parent) {
    UINode* n = cloneRec(src, parent);
    if (parent) parent->kids.push_back(n);
    return n;
}

void destroy(UINode* n) {
    if (!n) return;
    if (n->parent) {
        auto& v = n->parent->kids;
        v.erase(std::remove(v.begin(), v.end(), n), v.end());
    }
    std::function<void(UINode*)> del = [&](UINode* x) { for (auto* k : x->kids) { k->parent = nullptr; del(k); } if (g_pressed == x) g_pressed = nullptr; if (g_scrollNode == x) g_scrollNode = nullptr; delete x; };
    n->parent = nullptr;
    del(n);
}

}  // namespace ui
UINode* UINode::child(const std::string& n) const {
    for (auto* k : kids) if (k->name == n) return k;
    return nullptr;
}
UINode* UINode::findDeep(const std::string& n) {
    for (auto* k : kids) if (k->name == n) return k;
    for (auto* k : kids) { UINode* r = k->findDeep(n); if (r) return r; }
    return nullptr;
}
UINode* UINode::find(const std::string& path) {
    size_t s = path.find('/');
    if (s == std::string::npos) { UINode* c = child(path); return c ? c : findDeep(path); }
    UINode* c = child(path.substr(0, s));
    if (!c) c = findDeep(path.substr(0, s));
    return c ? c->find(path.substr(s + 1)) : nullptr;
}
bool UINode::hasScript(const std::string& s) const { for (auto& x : scripts) if (x == s) return true; return false; }
bool UINode::visibleInTree() const {
    for (const UINode* n = this; n; n = n->parent) if (!n->active) return false;
    return true;
}

namespace ui {
struct XF { float sx, sy, tx, ty; };

static void layoutRec(UINode* n, float px0, float py0, float pw, float ph, const XF& pxf) {
    float w, h, pvx, pvy;
    if (n->isRect) {
        float ax0 = px0 + pw * n->amin[0], ay0 = py0 + ph * n->amin[1];
        float ax1 = px0 + pw * n->amax[0], ay1 = py0 + ph * n->amax[1];
        w = (ax1 - ax0) + n->size[0]; h = (ay1 - ay0) + n->size[1];
        pvx = ax0 + (ax1 - ax0) * n->pivot[0] + n->apos[0];
        pvy = ay0 + (ay1 - ay0) * n->pivot[1] + n->apos[1];
    } else {
        w = h = 0;
        pvx = n->lpos[0]; pvy = n->lpos[1];
    }
    XF xf{pxf.sx * n->scale[0], pxf.sy * n->scale[1], pxf.sx * pvx + pxf.tx, pxf.sy * pvy + pxf.ty};
    float lx0 = -n->pivot[0] * w, ly0 = -n->pivot[1] * h;
    if (!n->isRect) { lx0 = ly0 = 0; }
    float X0 = xf.tx + xf.sx * lx0, X1 = xf.tx + xf.sx * (lx0 + w);
    float Y0 = xf.ty + xf.sy * ly0, Y1 = xf.ty + xf.sy * (ly0 + h);
    n->rect = {std::min(X0, X1), std::min(Y0, Y1), fabsf(X1 - X0), fabsf(Y1 - Y0)};
    n->wsx = fabsf(xf.sx); n->wsy = fabsf(xf.sy);
    for (UINode* k : n->kids) layoutRec(k, lx0, ly0, w, h, xf);
}

void layout() {
    if (!root) return;
    XF xf{SCREEN_W / CANVAS_W, -(float)SCREEN_H / CANVAS_H, 0, (float)SCREEN_H};
    layoutRec(root, 0, 0, CANVAS_W, CANVAS_H, xf);
}

static void drawNode(UINode* n, float alpha) {
    if (!n->active) return;
    if (n->cgAlpha >= 0) alpha *= n->cgAlpha;
    alpha *= n->alphaMul;
    if (alpha <= 0.003f) return;
    n->drawAlpha = alpha;
    g_drawList.push_back(n);
    FRect r = n->rect;
    if (!n->customOnly) {
        if (n->hasImg && r.w > 0 && r.h > 0) {
            Color c = n->imgColor * n->tintMul; c.a *= alpha;
            if (n->sp) {
                float bs = 2.f * n->wsx / std::max(0.01f, n->sp->ppu);
                if (n->imgType == 1 || n->imgType == 2) gfx::drawSliced(n->sp, r, bs, c, n->fillCenter);
                else if (n->imgType == 3) gfx::drawFilled(n->sp, r, n->fillMethod, n->fillOrigin, n->fill, c);
                else if (n->preserve) {
                    float k = std::min(r.w / std::max(1.f, n->sp->fw), r.h / std::max(1.f, n->sp->fh));
                    FRect d{r.x + (r.w - n->sp->fw * k) * .5f, r.y + (r.h - n->sp->fh * k) * .5f, n->sp->fw * k, n->sp->fh * k};
                    gfx::drawSpriteRect(n->sp, d, c);
                } else gfx::drawSpriteRect(n->sp, r, c);
            } else if (c.a > 0.01f) gfx::fillRect(r, c);
        }
        if (n->hasTxt && !n->text.empty()) {
            gfx::TextStyle st;
            st.font = n->font;
            float k = n->wsy;
            st.size = n->fontSize * k;
            st.color = n->txtColor * n->tintMul; st.color.a *= alpha;
            st.align = n->align;
            st.wrap = n->hOverflow == 0;
            st.bestFit = n->bestFit; st.minSize = n->minSize * k; st.maxSize = n->maxSize * k;
            st.lineSpacing = n->lineSpacing;
            st.style = n->fontStyle;
            if (n->outline) { st.outline = true; st.outlineColor = n->effColor; st.outlineDist = std::max(0.75f, fabsf(n->effDist[0]) * k * 1.2f); }
            if (n->shadow) { st.shadow = true; st.shadowColor = n->effColor; st.shadowDx = n->effDist[0] * k; st.shadowDy = -n->effDist[1] * k; }
            gfx::drawText(n->text, r, st);
        }
    }
    if (n->customDraw) n->customDraw(n, r);
    if (n->mask) gfx::pushClip(r);
    for (UINode* k : n->kids) drawNode(k, alpha);
    if (n->mask) gfx::popClip();
}

void draw(UINode* n) {
    if (!n) { g_drawList.clear(); n = root; }
    if (!n) return;
    float a = 1;
    drawNode(n, a);
}

static bool clipped(UINode* n, float x, float y) {
    for (UINode* p = n->parent; p; p = p->parent)
        if (p->mask && !p->rect.contains(x, y)) return true;
    return false;
}
UINode* hitNode(float x, float y) {
    for (int i = (int)g_drawList.size() - 1; i >= 0; i--) {
        UINode* n = g_drawList[i];
        if (!(n->raycast || n->button || n->onClick || n->onPress || n->onScroll) || !n->visibleInTree()) continue;
        if (n->drawAlpha < 0.05f) continue;
        if (n->rect.contains(x, y) && !clipped(n, x, y)) return n;
    }
    return nullptr;
}
bool hitAny(float x, float y) { return hitNode(x, y) != nullptr; }

static UINode* clickable(UINode* n) {
    for (UINode* p = n; p; p = p->parent) if ((p->button && p->interactable) || p->onClick || p->onPress) return p;
    return nullptr;
}
static UINode* scrollAncestor(UINode* n) {
    for (UINode* p = n; p; p = p->parent) if (p->onScroll) return p;
    return nullptr;
}
UINode* pressed() { return g_pressed; }

bool pointerDown(float x, float y) {
    UINode* h = hitNode(x, y);
    if (!h) return false;
    g_pressed = clickable(h);
    g_pressX = x; g_pressY = y;
    g_scrollNode = scrollAncestor(h);
    g_dragScroll = false;
    if (g_pressed && g_pressed->onPress) g_pressed->onPress(g_pressed, x, y);
    return true;
}
bool pointerMove(float x, float y) {
    if (g_scrollNode) {
        float dx = x - g_pressX, dy = y - g_pressY;
        if (!g_dragScroll && (fabsf(dx) > 10 || fabsf(dy) > 10)) { g_dragScroll = true; g_scrollNode->onScroll(0, 0, 0); }
        if (g_dragScroll) { g_scrollNode->onScroll(dx, dy, 1); return true; }
    }
    return g_pressed != nullptr;
}
bool pointerUp(float x, float y) {
    UINode* p = g_pressed;
    bool wasScroll = g_dragScroll;
    if (wasScroll && g_scrollNode) g_scrollNode->onScroll(x - g_pressX, y - g_pressY, 2);
    g_pressed = nullptr; g_scrollNode = nullptr; g_dragScroll = false;
    if (!p) return hitAny(x, y) || wasScroll;
    if (!wasScroll && p->rect.contains(x, y) && p->visibleInTree()) {
        if (p->onClick) { if (clickSound) clickSound(); p->onClick(); }
    }
    return true;
}

void setRect(UINode* n, float ax0, float ay0, float ax1, float ay1, float px, float py, float w, float h, float pivx, float pivy) {
    n->isRect = true;
    n->amin[0] = ax0; n->amin[1] = ay0; n->amax[0] = ax1; n->amax[1] = ay1;
    n->apos[0] = px; n->apos[1] = py; n->size[0] = w; n->size[1] = h; n->pivot[0] = pivx; n->pivot[1] = pivy;
}

void linearArrange(UINode* g) {
    std::vector<UINode*> kids;
    for (auto* k : g->kids) if (k->active) kids.push_back(k);
    if (kids.empty()) return;
    bool h = g->linear == 1;
    float W = g->rect.w / std::max(.01f, g->wsx), H = g->rect.h / std::max(.01f, g->wsy);
    if (W <= 0) W = g->size[0];
    if (H <= 0) H = g->size[1];
    float total = h ? W : H;
    float each = total / kids.size();
    float used = 0;
    for (auto* k : kids) { float s = h ? k->size[0] : k->size[1]; used += s > 1 ? s : each; }
    float pos = (total - used) * .5f;
    for (auto* k : kids) {
        float s = h ? k->size[0] : k->size[1];
        if (s <= 1) s = each;
        float o = h ? k->size[1] : k->size[0];
        if (o <= 1) o = h ? H : W;
        if (h) setRect(k, 0, .5f, 0, .5f, pos + s * k->pivot[0], 0, s, o, k->pivot[0], k->pivot[1]);
        else setRect(k, .5f, 1, .5f, 1, 0, -(pos + s * (1 - k->pivot[1])), o, s, k->pivot[0], k->pivot[1]);
        pos += s;
    }
}

void gridArrange(UINode* g, float* cw, float* ch) {
    std::vector<UINode*> kids;
    for (auto* k : g->kids) if (k->active) kids.push_back(k);
    int n = (int)kids.size();
    int cols, rows;
    if (g->gConstraint == 1) { cols = g->gCount; rows = (n + cols - 1) / std::max(1, cols); }
    else if (g->gConstraint == 2) { rows = g->gCount; cols = (n + rows - 1) / std::max(1, rows); }
    else {
        float avail = g->size[0] + 0;  // flexible: fit width
        cols = std::max(1, (int)((avail - g->gPad[0] - g->gPad[1] + g->gSpacing[0]) / (g->gCell[0] + g->gSpacing[0])));
        rows = (n + cols - 1) / cols;
    }
    cols = std::max(cols, 1); rows = std::max(rows, 1);
    float W = g->gPad[0] + g->gPad[1] + cols * g->gCell[0] + (cols - 1) * g->gSpacing[0];
    float H = g->gPad[2] + g->gPad[3] + rows * g->gCell[1] + (rows - 1) * g->gSpacing[1];
    if (cw) *cw = W;
    if (ch) *ch = H;
    for (int i = 0; i < n; i++) {
        int c, r;
        if (g->gAxis == 0) { c = i % cols; r = i / cols; } else { r = i % rows; c = i / rows; }
        float x = g->gPad[0] + c * (g->gCell[0] + g->gSpacing[0]);
        float y = g->gPad[2] + r * (g->gCell[1] + g->gSpacing[1]);
        UINode* k = kids[i];
        setRect(k, 0, 1, 0, 1, x + g->gCell[0] * k->pivot[0], -(y + g->gCell[1] * (1 - k->pivot[1])), g->gCell[0], g->gCell[1], k->pivot[0], k->pivot[1]);
    }
}
}  // namespace ui
