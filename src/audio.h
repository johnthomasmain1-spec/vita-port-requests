#pragma once
#include <string>

namespace audio {
bool init(const std::string& dataDir);
void shutdown();
void play(const std::string& id);        // e.g. "SFX_CashRegister" (Audio.txt ids) or a bare file name
void music(bool on);
void setVolumes(float music, float sfx);
float musicVolume();
float sfxVolume();
}
