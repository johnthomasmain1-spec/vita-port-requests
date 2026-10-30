// Minimal JSON reader/writer (no external deps).
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <cstdio>

struct JV {
    enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL;
    double n = 0;
    bool b = false;
    std::string s;
    std::vector<JV> a;
    std::vector<std::pair<std::string, JV>> o;
    std::unique_ptr<std::unordered_map<std::string, size_t>> idx;  // built lazily for big objects

    JV() {}
    JV(const JV& x) : t(x.t), n(x.n), b(x.b), s(x.s), a(x.a), o(x.o) {}
    JV(JV&&) = default;
    JV& operator=(const JV& x) { t = x.t; n = x.n; b = x.b; s = x.s; a = x.a; o = x.o; idx.reset(); return *this; }
    JV& operator=(JV&&) = default;

    bool isNull() const { return t == NUL; }
    bool isObj() const { return t == OBJ; }
    bool isArr() const { return t == ARR; }
    bool isStr() const { return t == STR; }
    bool isNum() const { return t == NUM; }
    size_t size() const { return t == ARR ? a.size() : (t == OBJ ? o.size() : 0); }
    const JV& operator[](size_t i) const { static JV nil; return (t == ARR && i < a.size()) ? a[i] : nil; }
    const JV& operator[](int i) const { return (*this)[(size_t)i]; }
    const JV* find(const char* k) const {
        if (t != OBJ) return nullptr;
        if (o.size() > 24) {
            if (!idx) {
                auto* self = const_cast<JV*>(this);
                self->idx.reset(new std::unordered_map<std::string, size_t>());
                for (size_t i = 0; i < o.size(); i++) (*self->idx)[o[i].first] = i;
            }
            auto it = idx->find(k);
            return it == idx->end() ? nullptr : &o[it->second].second;
        }
        for (auto& p : o) if (p.first == k) return &p.second;
        return nullptr;
    }
    const JV& operator[](const char* k) const { static JV nil; const JV* r = find(k); return r ? *r : nil; }
    const JV& operator[](const std::string& k) const { return (*this)[k.c_str()]; }
    bool has(const char* k) const { return find(k) != nullptr; }
    double num(double d = 0) const { return t == NUM ? n : (t == BOOL ? (b ? 1 : 0) : d); }
    int i(int d = 0) const { return t == NUM ? (int)n : (t == BOOL ? (b ? 1 : 0) : d); }
    float f(float d = 0) const { return t == NUM ? (float)n : d; }
    const std::string& str() const { static std::string e; return t == STR ? s : e; }

    // builders
    static JV mkobj() { JV v; v.t = OBJ; return v; }
    static JV mkarr() { JV v; v.t = ARR; return v; }
    static JV mknum(double d) { JV v; v.t = NUM; v.n = d; return v; }
    static JV mkstr(const std::string& x) { JV v; v.t = STR; v.s = x; return v; }
    static JV mkbool(bool x) { JV v; v.t = BOOL; v.b = x; return v; }
    void set(const std::string& k, JV v) {
        idx.reset();
        for (auto& p : o) if (p.first == k) { p.second = std::move(v); return; }
        o.emplace_back(k, std::move(v));
    }
    void push(JV v) { a.push_back(std::move(v)); }
};

struct JParser {
    const char* p; const char* e;
    void ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++; }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    std::string pstr() {
        std::string r; p++;
        while (p < e && *p != '"') {
            if (*p == '\\') {
                p++;
                char c = *p++;
                switch (c) {
                    case 'n': r += '\n'; break; case 't': r += '\t'; break; case 'r': r += '\r'; break;
                    case 'b': r += '\b'; break; case 'f': r += '\f'; break;
                    case 'u': {
                        unsigned cp = strtoul(std::string(p, 4).c_str(), nullptr, 16); p += 4;
                        if (cp >= 0xD800 && cp < 0xDC00 && p + 6 <= e && p[0] == '\\' && p[1] == 'u') {
                            unsigned lo = strtoul(std::string(p + 2, 4).c_str(), nullptr, 16); p += 6;
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        utf8(r, cp); break;
                    }
                    default: r += c;
                }
            } else r += *p++;
        }
        p++;
        return r;
    }
    JV val() {
        ws(); JV v;
        if (p >= e) return v;
        if (*p == '{') {
            v.t = JV::OBJ; p++; ws();
            if (*p == '}') { p++; return v; }
            while (p < e) {
                ws(); std::string k = pstr(); ws(); p++;  // ':'
                v.o.emplace_back(std::move(k), val()); ws();
                if (*p == ',') { p++; continue; }
                p++; break;
            }
        } else if (*p == '[') {
            v.t = JV::ARR; p++; ws();
            if (*p == ']') { p++; return v; }
            while (p < e) {
                v.a.push_back(val()); ws();
                if (*p == ',') { p++; continue; }
                p++; break;
            }
        } else if (*p == '"') { v.t = JV::STR; v.s = pstr(); }
        else if (!strncmp(p, "true", 4)) { v.t = JV::BOOL; v.b = true; p += 4; }
        else if (!strncmp(p, "false", 5)) { v.t = JV::BOOL; v.b = false; p += 5; }
        else if (!strncmp(p, "null", 4)) { p += 4; }
        else if (!strncmp(p, "NaN", 3)) { v.t = JV::NUM; v.n = 0; p += 3; }
        else { v.t = JV::NUM; char* end; v.n = strtod(p, &end); p = end; }
        return v;
    }
};

inline JV jparse(const std::string& s) { JParser jp{s.data(), s.data() + s.size()}; return jp.val(); }

inline bool readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize(n);
    size_t r = n ? fread(&out[0], 1, n, f) : 0;
    fclose(f);
    return r == (size_t)n;
}
inline JV jload(const std::string& path) { std::string s; if (!readFile(path, s)) return JV(); return jparse(s); }

inline void jwrite(const JV& v, std::string& out) {
    char buf[64];
    switch (v.t) {
        case JV::NUL: out += "null"; break;
        case JV::BOOL: out += v.b ? "true" : "false"; break;
        case JV::NUM:
            if (v.n == (double)(long long)v.n) snprintf(buf, sizeof buf, "%lld", (long long)v.n);
            else snprintf(buf, sizeof buf, "%.6g", v.n);
            out += buf; break;
        case JV::STR:
            out += '"';
            for (char c : v.s) {
                if (c == '"' || c == '\\') { out += '\\'; out += c; }
                else if (c == '\n') out += "\\n";
                else if ((unsigned char)c < 0x20) { snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                else out += c;
            }
            out += '"'; break;
        case JV::ARR:
            out += '[';
            for (size_t i = 0; i < v.a.size(); i++) { if (i) out += ','; jwrite(v.a[i], out); }
            out += ']'; break;
        case JV::OBJ:
            out += '{';
            for (size_t i = 0; i < v.o.size(); i++) {
                if (i) out += ',';
                jwrite(JV::mkstr(v.o[i].first), out); out += ':'; jwrite(v.o[i].second, out);
            }
            out += '}'; break;
    }
}
