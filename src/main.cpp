#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <vector>
#include "app.h"
#include "gfx.h"

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <psp2/apputil.h>
int _newlib_heap_size_user = 200 * 1024 * 1024;
#define DATA_DIR "ux0:data/ft2vita"
#define SAVE_DIR "ux0:data/ft2vita/save"
#else
#define DATA_DIR "data"
#define SAVE_DIR "save"
#endif

static void fatal(const std::string& msg) {
    SDL_Log("%s", msg.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fish Tycoon 2", msg.c_str(), gfx::W);
}

int main(int argc, char** argv) {
#ifdef __vita__
    scePowerSetArmClockFrequency(444);
    scePowerSetGpuClockFrequency(222);
#endif
    std::string dataDir = DATA_DIR, saveDir = SAVE_DIR;
    std::string shot; std::vector<std::string> cmds; int frames = 90;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) dataDir = argv[++i];
        else if (a == "--save" && i + 1 < argc) saveDir = argv[++i];
        else if (a == "--shot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--frames" && i + 1 < argc) frames = atoi(argv[++i]);
        else if (a == "--cmd" && i + 1 < argc) cmds.push_back(argv[++i]);
    }
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) { SDL_Log("SDL_Init: %s", SDL_GetError()); return 1; }
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    gfx::W = SDL_CreateWindow("Fish Tycoon 2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    gfx::R = SDL_CreateRenderer(gfx::W, -1, shot.empty() ? (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : SDL_RENDERER_SOFTWARE);
    if (!gfx::R) gfx::R = SDL_CreateRenderer(gfx::W, -1, 0);
    SDL_RenderSetLogicalSize(gfx::R, SCREEN_W, SCREEN_H);
    SDL_SetRenderDrawBlendMode(gfx::R, SDL_BLENDMODE_BLEND);
    if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

    if (!app::init(dataDir, saveDir)) {
        fatal("Game data not found.\n\nRun the ft2extract tool on your own Fish Tycoon 2 APK/XAPK and copy the result to\n" + dataDir);
        SDL_Quit();
        return 1;
    }

    if (!shot.empty()) {   // headless test: run N frames with commands, then save a screenshot
        size_t ci = 0;
        for (int f = 0; f < frames; f++) {
            SDL_Event e; while (SDL_PollEvent(&e)) {}
            if (ci < cmds.size() && f % 10 == 5) app::debugCommand(cmds[ci++]);
            app::update(1.f / 30.f);
            app::render();
        }
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
        app::render();
        SDL_RenderReadPixels(gfx::R, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
        IMG_SavePNG(s, shot.c_str());
        SDL_FreeSurface(s);
        app::shutdown();
        SDL_Quit();
        return 0;
    }

    Uint64 last = SDL_GetPerformanceCounter();
    while (!app::wantsQuit()) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) app::event(e);
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        app::update(dt);
        app::render();
    }
    app::shutdown();
    SDL_Quit();
#ifdef __vita__
    sceKernelExitProcess(0);
#endif
    return 0;
}
