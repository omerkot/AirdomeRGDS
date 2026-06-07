#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#elif __has_include(<SDL.h>)
#include <SDL.h>
#else
#error "SDL2 headers were not found"
#endif

#ifdef AIRDOME_DYNAMIC_SDL

struct DynamicSdl {
    void *handle = nullptr;

    decltype(&::SDL_CreateRenderer) SDL_CreateRenderer = nullptr;
    decltype(&::SDL_CreateTexture) SDL_CreateTexture = nullptr;
    decltype(&::SDL_CreateWindow) SDL_CreateWindow = nullptr;
    decltype(&::SDL_CloseAudioDevice) SDL_CloseAudioDevice = nullptr;
    decltype(&::SDL_Delay) SDL_Delay = nullptr;
    decltype(&::SDL_DestroyRenderer) SDL_DestroyRenderer = nullptr;
    decltype(&::SDL_DestroyTexture) SDL_DestroyTexture = nullptr;
    decltype(&::SDL_DestroyWindow) SDL_DestroyWindow = nullptr;
    decltype(&::SDL_GameControllerClose) SDL_GameControllerClose = nullptr;
    decltype(&::SDL_GameControllerGetAxis) SDL_GameControllerGetAxis = nullptr;
    decltype(&::SDL_GameControllerGetJoystick) SDL_GameControllerGetJoystick = nullptr;
    decltype(&::SDL_GameControllerName) SDL_GameControllerName = nullptr;
    decltype(&::SDL_GameControllerOpen) SDL_GameControllerOpen = nullptr;
    decltype(&::SDL_GetCurrentDisplayMode) SDL_GetCurrentDisplayMode = nullptr;
    decltype(&::SDL_GetError) SDL_GetError = nullptr;
    decltype(&::SDL_GetNumVideoDisplays) SDL_GetNumVideoDisplays = nullptr;
    decltype(&::SDL_GetRendererOutputSize) SDL_GetRendererOutputSize = nullptr;
    decltype(&::SDL_GetTicks) SDL_GetTicks = nullptr;
    decltype(&::SDL_GetWindowID) SDL_GetWindowID = nullptr;
    decltype(&::SDL_Init) SDL_Init = nullptr;
    decltype(&::SDL_IsGameController) SDL_IsGameController = nullptr;
    decltype(&::SDL_JoystickInstanceID) SDL_JoystickInstanceID = nullptr;
    decltype(&::SDL_LockAudioDevice) SDL_LockAudioDevice = nullptr;
    decltype(&::SDL_NumJoysticks) SDL_NumJoysticks = nullptr;
    decltype(&::SDL_OpenAudioDevice) SDL_OpenAudioDevice = nullptr;
    decltype(&::SDL_PauseAudioDevice) SDL_PauseAudioDevice = nullptr;
    decltype(&::SDL_PollEvent) SDL_PollEvent = nullptr;
    decltype(&::SDL_Quit) SDL_Quit = nullptr;
    decltype(&::SDL_RenderClear) SDL_RenderClear = nullptr;
    decltype(&::SDL_RenderCopy) SDL_RenderCopy = nullptr;
    decltype(&::SDL_RenderDrawLines) SDL_RenderDrawLines = nullptr;
    decltype(&::SDL_RenderDrawPoint) SDL_RenderDrawPoint = nullptr;
    decltype(&::SDL_RenderDrawRect) SDL_RenderDrawRect = nullptr;
    decltype(&::SDL_RenderFillRect) SDL_RenderFillRect = nullptr;
    decltype(&::SDL_RenderPresent) SDL_RenderPresent = nullptr;
    decltype(&::SDL_RenderSetLogicalSize) SDL_RenderSetLogicalSize = nullptr;
    decltype(&::SDL_SetHint) SDL_SetHint = nullptr;
    decltype(&::SDL_SetRenderDrawColor) SDL_SetRenderDrawColor = nullptr;
    decltype(&::SDL_SetTextureBlendMode) SDL_SetTextureBlendMode = nullptr;
    decltype(&::SDL_UnlockAudioDevice) SDL_UnlockAudioDevice = nullptr;
    decltype(&::SDL_UpdateTexture) SDL_UpdateTexture = nullptr;
};

extern DynamicSdl sdl;

bool loadDynamicSdl();
void unloadDynamicSdl();

#define SDL_CreateRenderer sdl.SDL_CreateRenderer
#define SDL_CreateTexture sdl.SDL_CreateTexture
#define SDL_CreateWindow sdl.SDL_CreateWindow
#define SDL_CloseAudioDevice sdl.SDL_CloseAudioDevice
#define SDL_Delay sdl.SDL_Delay
#define SDL_DestroyRenderer sdl.SDL_DestroyRenderer
#define SDL_DestroyTexture sdl.SDL_DestroyTexture
#define SDL_DestroyWindow sdl.SDL_DestroyWindow
#define SDL_GameControllerClose sdl.SDL_GameControllerClose
#define SDL_GameControllerGetAxis sdl.SDL_GameControllerGetAxis
#define SDL_GameControllerGetJoystick sdl.SDL_GameControllerGetJoystick
#define SDL_GameControllerName sdl.SDL_GameControllerName
#define SDL_GameControllerOpen sdl.SDL_GameControllerOpen
#define SDL_GetCurrentDisplayMode sdl.SDL_GetCurrentDisplayMode
#define SDL_GetError sdl.SDL_GetError
#define SDL_GetNumVideoDisplays sdl.SDL_GetNumVideoDisplays
#define SDL_GetRendererOutputSize sdl.SDL_GetRendererOutputSize
#define SDL_GetTicks sdl.SDL_GetTicks
#define SDL_GetWindowID sdl.SDL_GetWindowID
#define SDL_Init sdl.SDL_Init
#define SDL_IsGameController sdl.SDL_IsGameController
#define SDL_JoystickInstanceID sdl.SDL_JoystickInstanceID
#define SDL_LockAudioDevice sdl.SDL_LockAudioDevice
#define SDL_NumJoysticks sdl.SDL_NumJoysticks
#define SDL_OpenAudioDevice sdl.SDL_OpenAudioDevice
#define SDL_PauseAudioDevice sdl.SDL_PauseAudioDevice
#define SDL_PollEvent sdl.SDL_PollEvent
#define SDL_Quit sdl.SDL_Quit
#define SDL_RenderClear sdl.SDL_RenderClear
#define SDL_RenderCopy sdl.SDL_RenderCopy
#define SDL_RenderDrawLines sdl.SDL_RenderDrawLines
#define SDL_RenderDrawPoint sdl.SDL_RenderDrawPoint
#define SDL_RenderDrawRect sdl.SDL_RenderDrawRect
#define SDL_RenderFillRect sdl.SDL_RenderFillRect
#define SDL_RenderPresent sdl.SDL_RenderPresent
#define SDL_RenderSetLogicalSize sdl.SDL_RenderSetLogicalSize
#define SDL_SetHint sdl.SDL_SetHint
#define SDL_SetRenderDrawColor sdl.SDL_SetRenderDrawColor
#define SDL_SetTextureBlendMode sdl.SDL_SetTextureBlendMode
#define SDL_UnlockAudioDevice sdl.SDL_UnlockAudioDevice
#define SDL_UpdateTexture sdl.SDL_UpdateTexture

#endif
