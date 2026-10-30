#pragma once
#include <SDL2/SDL.h>
#include <string>

namespace app {
bool init(const std::string& dataDir, const std::string& saveDir);
void event(const SDL_Event& e);
void update(float dt);
void render();
void shutdown();
bool wantsQuit();
// headless testing helpers
std::string stateSummary();
bool quitViaSelect();
void debugCommand(const std::string& cmd);
}
