#include "audio.h"
#include "json.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <unordered_map>

namespace audio {
static std::string g_dir;
static std::unordered_map<std::string, std::string> g_ids;   // id -> file base name
static std::unordered_map<std::string, Mix_Chunk*> g_chunks;
static Mix_Music* g_music = nullptr;
static bool g_ok = false;
static float g_mv = 0.6f, g_sv = 0.8f;

bool init(const std::string& dataDir) {
    g_dir = dataDir;
    if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048) < 0) { SDL_Log("Mix_OpenAudio: %s", Mix_GetError()); return false; }
    Mix_Init(MIX_INIT_OGG);
    Mix_AllocateChannels(16);
    g_ok = true;
    JV d = jload(dataDir + "/cfg/Audio.txt");
    for (auto& e : d["def"].a) {
        std::string fp = e["FilePath"].str();
        size_t s = fp.rfind('/');
        g_ids[e["id"].str()] = s == std::string::npos ? fp : fp.substr(s + 1);
    }
    return true;
}
void shutdown() {
    if (!g_ok) return;
    for (auto& c : g_chunks) if (c.second) Mix_FreeChunk(c.second);
    if (g_music) Mix_FreeMusic(g_music);
    Mix_CloseAudio();
}
void play(const std::string& id) {
    if (!g_ok || g_sv <= 0) return;
    auto it = g_ids.find(id);
    std::string base = it == g_ids.end() ? id : it->second;
    auto c = g_chunks.find(base);
    Mix_Chunk* ch;
    if (c == g_chunks.end()) {
        std::string p = g_dir + "/audio/" + base + ".wav";
        ch = Mix_LoadWAV(p.c_str());
        g_chunks[base] = ch;
    } else ch = c->second;
    if (!ch) return;
    Mix_VolumeChunk(ch, (int)(g_sv * MIX_MAX_VOLUME));
    Mix_PlayChannel(-1, ch, 0);
}
void music(bool on) {
    if (!g_ok) return;
    if (on) {
        if (!g_music) {
            std::string p = g_dir + "/audio/FT2Music.ogg";
            g_music = Mix_LoadMUS(p.c_str());
            if (!g_music) { p = g_dir + "/audio/FT2Music.wav"; g_music = Mix_LoadMUS(p.c_str()); }
        }
        if (g_music && !Mix_PlayingMusic()) Mix_PlayMusic(g_music, -1);
        Mix_VolumeMusic((int)(g_mv * MIX_MAX_VOLUME));
    } else Mix_HaltMusic();
}
void setVolumes(float m, float s) { g_mv = m; g_sv = s; if (g_ok) Mix_VolumeMusic((int)(g_mv * MIX_MAX_VOLUME)); }
float musicVolume() { return g_mv; }
float sfxVolume() { return g_sv; }
}
