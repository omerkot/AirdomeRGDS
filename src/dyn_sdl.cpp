#include "dyn_sdl.h"

#ifdef AIRDOME_DYNAMIC_SDL

#undef SDL_CreateRenderer
#undef SDL_CreateTexture
#undef SDL_CreateWindow
#undef SDL_CloseAudioDevice
#undef SDL_Delay
#undef SDL_DestroyRenderer
#undef SDL_DestroyTexture
#undef SDL_DestroyWindow
#undef SDL_GameControllerClose
#undef SDL_GameControllerGetAxis
#undef SDL_GameControllerGetJoystick
#undef SDL_GameControllerName
#undef SDL_GameControllerOpen
#undef SDL_GetCurrentDisplayMode
#undef SDL_GetError
#undef SDL_GetNumVideoDisplays
#undef SDL_GetRendererOutputSize
#undef SDL_GetTicks
#undef SDL_GetWindowID
#undef SDL_Init
#undef SDL_IsGameController
#undef SDL_JoystickInstanceID
#undef SDL_LockAudioDevice
#undef SDL_NumJoysticks
#undef SDL_OpenAudioDevice
#undef SDL_PauseAudioDevice
#undef SDL_PollEvent
#undef SDL_Quit
#undef SDL_RenderClear
#undef SDL_RenderCopy
#undef SDL_RenderDrawLines
#undef SDL_RenderDrawPoint
#undef SDL_RenderDrawRect
#undef SDL_RenderFillRect
#undef SDL_RenderPresent
#undef SDL_RenderSetLogicalSize
#undef SDL_SetHint
#undef SDL_SetRenderDrawColor
#undef SDL_SetTextureBlendMode
#undef SDL_UnlockAudioDevice
#undef SDL_UpdateTexture

#include <dlfcn.h>
#include <stdio.h>

DynamicSdl sdl;

template <typename Fn>
bool loadSymbol(void *handle, const char *name, Fn &slot) {
    slot = reinterpret_cast<Fn>(dlsym(handle, name));
    if (!slot) {
        fprintf(stderr, "Missing SDL2 symbol: %s\n", name);
        return false;
    }
    return true;
}

bool loadDynamicSdl() {
    const char *candidates[] = {
        "libSDL2-2.0.so.0",
        "libSDL2.so",
        "/usr/lib/libSDL2-2.0.so.0",
        "/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0",
        nullptr
    };

    for (int i = 0; candidates[i]; i++) {
        sdl.handle = dlopen(candidates[i], RTLD_NOW | RTLD_LOCAL);
        if (sdl.handle) {
            printf("Loaded SDL2: %s\n", candidates[i]);
            break;
        }
    }

    if (!sdl.handle) {
        fprintf(stderr, "Could not load SDL2. Last dlopen error: %s\n", dlerror());
        return false;
    }

#define LOAD_SDL(name) if (!loadSymbol(sdl.handle, #name, sdl.name)) return false
    LOAD_SDL(SDL_CreateRenderer);
    LOAD_SDL(SDL_CreateTexture);
    LOAD_SDL(SDL_CreateWindow);
    LOAD_SDL(SDL_CloseAudioDevice);
    LOAD_SDL(SDL_Delay);
    LOAD_SDL(SDL_DestroyRenderer);
    LOAD_SDL(SDL_DestroyTexture);
    LOAD_SDL(SDL_DestroyWindow);
    LOAD_SDL(SDL_GameControllerClose);
    LOAD_SDL(SDL_GameControllerGetAxis);
    LOAD_SDL(SDL_GameControllerGetJoystick);
    LOAD_SDL(SDL_GameControllerName);
    LOAD_SDL(SDL_GameControllerOpen);
    LOAD_SDL(SDL_GetCurrentDisplayMode);
    LOAD_SDL(SDL_GetError);
    LOAD_SDL(SDL_GetNumVideoDisplays);
    LOAD_SDL(SDL_GetRendererOutputSize);
    LOAD_SDL(SDL_GetTicks);
    LOAD_SDL(SDL_GetWindowID);
    LOAD_SDL(SDL_Init);
    LOAD_SDL(SDL_IsGameController);
    LOAD_SDL(SDL_JoystickInstanceID);
    LOAD_SDL(SDL_LockAudioDevice);
    LOAD_SDL(SDL_NumJoysticks);
    LOAD_SDL(SDL_OpenAudioDevice);
    LOAD_SDL(SDL_PauseAudioDevice);
    LOAD_SDL(SDL_PollEvent);
    LOAD_SDL(SDL_Quit);
    LOAD_SDL(SDL_RenderClear);
    LOAD_SDL(SDL_RenderCopy);
    LOAD_SDL(SDL_RenderDrawLines);
    LOAD_SDL(SDL_RenderDrawPoint);
    LOAD_SDL(SDL_RenderDrawRect);
    LOAD_SDL(SDL_RenderFillRect);
    LOAD_SDL(SDL_RenderPresent);
    LOAD_SDL(SDL_RenderSetLogicalSize);
    LOAD_SDL(SDL_SetHint);
    LOAD_SDL(SDL_SetRenderDrawColor);
    LOAD_SDL(SDL_SetTextureBlendMode);
    LOAD_SDL(SDL_UnlockAudioDevice);
    LOAD_SDL(SDL_UpdateTexture);
#undef LOAD_SDL

    return true;
}

void unloadDynamicSdl() {
    if (sdl.handle) {
        dlclose(sdl.handle);
        sdl = DynamicSdl {};
    }
}

#endif
