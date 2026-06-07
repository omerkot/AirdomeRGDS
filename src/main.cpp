#include "dyn_sdl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using s16 = int16_t;

#define BIT(n) (1u << (n))
#define RGB15(r,g,b) ((u16)(((r) & 31) | (((g) & 31) << 5) | (((b) & 31) << 10)))

static const u16 KEY_A = 1u << 0;
static const u16 KEY_B = 1u << 1;
static const u16 KEY_SELECT = 1u << 2;
static const u16 KEY_START = 1u << 3;
static const u16 KEY_RIGHT = 1u << 4;
static const u16 KEY_LEFT = 1u << 5;
static const u16 KEY_UP = 1u << 6;
static const u16 KEY_DOWN = 1u << 7;
static const u16 KEY_R = 1u << 8;
static const u16 KEY_L = 1u << 9;
static const u16 KEY_X = 1u << 10;
static const u16 KEY_Y = 1u << 11;
static const u16 KEY_TOUCH = 1u << 12;

struct touchPosition { int px = 0; int py = 0; };

#include "generated_backgrounds_rgds.h"
#include "generated_audio_rgds.h"

static const int RGDS_SCREEN_W = 640;
static const int RGDS_SCREEN_H = 480;
static const int NATIVE_ANALOG_DEADZONE = 16000;

enum NativeBackdrop {
    NATIVE_BG_NONE = 0,
    NATIVE_BG_LEVEL_1,
    NATIVE_BG_LEVEL_2,
    NATIVE_BG_LEVEL_3,
    NATIVE_BG_LEVEL_4,
    NATIVE_BG_LEVEL_5,
    NATIVE_BG_LEVEL_6,
    NATIVE_BG_LEVEL_1_DEATH,
    NATIVE_BG_LEVEL_2_DEATH,
    NATIVE_BG_LEVEL_3_DEATH,
    NATIVE_BG_LEVEL_4_DEATH,
    NATIVE_BG_LEVEL_5_DEATH,
    NATIVE_BG_LEVEL_6_DEATH,
    NATIVE_BG_TITLE_TOP,
    NATIVE_BG_TITLE_BOTTOM,
    NATIVE_BG_HUD_EASY,
    NATIVE_BG_HUD_NORMAL,
    NATIVE_BG_HUD_HARD,
    NATIVE_BG_COUNT
};

static const char *const nativeBackdropFiles[NATIVE_BG_COUNT] = {
    nullptr,
    "assets/rgds/level_1.argb",
    "assets/rgds/level_2.argb",
    "assets/rgds/level_3.argb",
    "assets/rgds/level_4.argb",
    "assets/rgds/level_5.argb",
    "assets/rgds/level_6.argb",
    "assets/rgds/level_1_death.argb",
    "assets/rgds/level_2_death.argb",
    "assets/rgds/level_3_death.argb",
    "assets/rgds/level_4_death.argb",
    "assets/rgds/level_5_death.argb",
    "assets/rgds/level_6_death.argb",
    "assets/rgds/title_top.argb",
    "assets/rgds/title_bottom.argb",
    "assets/rgds/hud_easy.argb",
    "assets/rgds/hud_normal.argb",
    "assets/rgds/hud_hard.argb",
};

struct NativePanel {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *overlayTexture = nullptr;
    SDL_Texture *detailTexture = nullptr;
    SDL_Texture *backdrops[NATIVE_BG_COUNT] = {};
    NativeBackdrop backdrop = NATIVE_BG_NONE;
    u32 overlayPixels[256 * 192];
    u32 *detailPixels = nullptr;
};

static NativePanel nativeTop;
static NativePanel nativeBottom;
static SDL_GameController *nativeController = nullptr;
static bool nativeRunning = true;
static bool nativeDualDisplay = false;
static u16 nativeHeld = 0;
static u16 nativeButtonHeld = 0;
static u16 nativeAxisHeld = 0;
static u16 nativeJsAxisHeld = 0;
static u16 nativeHatHeld = 0;
static u16 nativeTouchHeld = 0;
static u16 nativeEvdevHeld = 0;
static u16 nativePressed = 0;
static u16 nativePrevHeld = 0;
static bool nativeTouchPending = false;
static int nativeTouchX = 0;
static int nativeTouchY = 0;
static bool nativeQuitCombo = false;
static int nativeEvdevFds[2] = { -1, -1 };
static bool nativeEvdevReady = false;
static int nativeJsFd = -1;
static int nativeJsAxisX = 0;
static int nativeJsAxisY = 0;
static SDL_AudioDeviceID nativeAudioDevice = 0;
static int nativeAudioRate = 48000;
static Uint32 nativeLastAudioAttempt = 0;
static bool nativeAudioFailureLogged = false;

struct NativeVoice {
    bool active = false;
    const s16 *data = nullptr;
    u32 count = 0;
    u32 pos = 0;
    u32 step = 0;
    u8 volume = 0;
    bool loop = false;
    int framesLeft = 0;
};

static const int NATIVE_MAX_VOICES = 12;
static const int NATIVE_DS_SOUND_RATE = 32768;
static const int NATIVE_WAV_SOUND_RATE = 11025;
static NativeVoice nativeVoices[NATIVE_MAX_VOICES];
static NativeVoice nativeBombVoice;
static NativeVoice nativeMissileVoice;
static NativeVoice nativeUfoDroneVoice;
static NativeVoice nativeUfoShotVoice;
static u16 nativeLastBombTimer = 0;
static u16 nativeLastBombVolume = 0;
static u8 nativeMissileNoteWait = 0;
static u8 nativeMissileNoteFrames = 0;
static u16 nativeLastUfoShotSeq = 0;

static u32 rgb15ToArgb(u16 c) {
    u8 r = (u8)((c & 31) * 255 / 31);
    u8 g = (u8)(((c >> 5) & 31) * 255 / 31);
    u8 b = (u8)(((c >> 10) & 31) * 255 / 31);
    return 0xFF000000u | ((u32)r << 16) | ((u32)g << 8) | b;
}

static void convertFrame(const u16 *src, const u16 *transparentBase, u32 *dst) {
    for (int i = 0; i < 256 * 192; ++i) {
        dst[i] = (transparentBase && src[i] == transparentBase[i]) ? 0x00000000u : rgb15ToArgb(src[i]);
    }
}

static void nativePresentPanel(NativePanel &panel, const u16 *src, const u16 *transparentBase) {
    if (!panel.renderer || !panel.overlayTexture) {
        fprintf(stderr, "present skipped: renderer=%p texture=%p\n", (void*)panel.renderer, (void*)panel.overlayTexture);
        return;
    }
    SDL_Texture *backdrop = panel.backdrops[panel.backdrop];
    convertFrame(src, backdrop ? transparentBase : nullptr, panel.overlayPixels);
    if (SDL_UpdateTexture(panel.overlayTexture, nullptr, panel.overlayPixels, 256 * (int)sizeof(u32)) != 0) {
        fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return;
    }
    SDL_RenderClear(panel.renderer);
    if (backdrop && SDL_RenderCopy(panel.renderer, backdrop, nullptr, nullptr) != 0) {
        fprintf(stderr, "SDL_RenderCopy backdrop failed: %s\n", SDL_GetError());
        return;
    }
    if (panel.detailTexture && panel.detailPixels) {
        if (SDL_UpdateTexture(panel.detailTexture, nullptr, panel.detailPixels, RGDS_SCREEN_W * (int)sizeof(u32)) != 0) {
            fprintf(stderr, "SDL_UpdateTexture detail failed: %s\n", SDL_GetError());
            return;
        }
        if (SDL_RenderCopy(panel.renderer, panel.detailTexture, nullptr, nullptr) != 0) {
            fprintf(stderr, "SDL_RenderCopy detail failed: %s\n", SDL_GetError());
            return;
        }
    }
    SDL_Rect dst = { 0, 0, RGDS_SCREEN_W, RGDS_SCREEN_H };
    if (SDL_RenderCopy(panel.renderer, panel.overlayTexture, nullptr, &dst) != 0) {
        fprintf(stderr, "SDL_RenderCopy overlay failed: %s\n", SDL_GetError());
        return;
    }
    SDL_RenderPresent(panel.renderer);
}

static SDL_Texture *nativeLoadBackdrop(SDL_Renderer *renderer, const char *path) {
    if (!path) return nullptr;
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "RGDS backdrop missing: %s\n", path);
        return nullptr;
    }
    const size_t byteCount = (size_t)RGDS_SCREEN_W * RGDS_SCREEN_H * sizeof(u32);
    u32 *pixels = (u32 *)malloc(byteCount);
    if (!pixels) {
        fclose(f);
        fprintf(stderr, "RGDS backdrop alloc failed: %s\n", path);
        return nullptr;
    }
    size_t got = fread(pixels, 1, byteCount, f);
    fclose(f);
    if (got != byteCount) {
        fprintf(stderr, "RGDS backdrop short read: %s (%zu/%zu)\n", path, got, byteCount);
        free(pixels);
        return nullptr;
    }
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, RGDS_SCREEN_W, RGDS_SCREEN_H);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture backdrop failed for %s: %s\n", path, SDL_GetError());
        free(pixels);
        return nullptr;
    }
    if (SDL_UpdateTexture(texture, nullptr, pixels, RGDS_SCREEN_W * (int)sizeof(u32)) != 0) {
        fprintf(stderr, "SDL_UpdateTexture backdrop failed for %s: %s\n", path, SDL_GetError());
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    free(pixels);
    return texture;
}

static void nativeLoadBackdrops(NativePanel &panel) {
    for (int i = 1; i < NATIVE_BG_COUNT; ++i) {
        panel.backdrops[i] = nativeLoadBackdrop(panel.renderer, nativeBackdropFiles[i]);
    }
}

static bool nativeCreatePanel(NativePanel &panel, const char *title, int displayIndex, bool fullscreen, bool detailOverlay) {
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP;
    if (!fullscreen) flags = SDL_WINDOW_SHOWN;
    panel.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex), SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex), RGDS_SCREEN_W, RGDS_SCREEN_H, flags);
    if (!panel.window) {
        fprintf(stderr, "SDL_CreateWindow failed for %s: %s\n", title, SDL_GetError());
        return false;
    }
    panel.renderer = SDL_CreateRenderer(panel.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!panel.renderer) panel.renderer = SDL_CreateRenderer(panel.window, -1, SDL_RENDERER_SOFTWARE);
    if (!panel.renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed for %s: %s\n", title, SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawColor(panel.renderer, 0, 0, 0, 255);
    SDL_RenderSetLogicalSize(panel.renderer, RGDS_SCREEN_W, RGDS_SCREEN_H);
    panel.overlayTexture = SDL_CreateTexture(panel.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 192);
    if (!panel.overlayTexture) {
        fprintf(stderr, "SDL_CreateTexture failed for %s: %s\n", title, SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(panel.overlayTexture, SDL_BLENDMODE_BLEND);
    if (detailOverlay) {
        panel.detailPixels = (u32*)calloc(RGDS_SCREEN_W * RGDS_SCREEN_H, sizeof(u32));
        panel.detailTexture = SDL_CreateTexture(panel.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RGDS_SCREEN_W, RGDS_SCREEN_H);
        if (!panel.detailPixels || !panel.detailTexture) {
            fprintf(stderr, "SDL_CreateTexture detail failed for %s: %s\n", title, SDL_GetError());
            return false;
        }
        SDL_SetTextureBlendMode(panel.detailTexture, SDL_BLENDMODE_BLEND);
    }
    nativeLoadBackdrops(panel);
    return true;
}

static void nativeDestroyPanel(NativePanel &panel) {
    if (panel.detailTexture) SDL_DestroyTexture(panel.detailTexture);
    if (panel.overlayTexture) SDL_DestroyTexture(panel.overlayTexture);
    for (int i = 1; i < NATIVE_BG_COUNT; ++i) {
        if (panel.backdrops[i]) SDL_DestroyTexture(panel.backdrops[i]);
    }
    if (panel.renderer) SDL_DestroyRenderer(panel.renderer);
    if (panel.window) SDL_DestroyWindow(panel.window);
    free(panel.detailPixels);
    panel = NativePanel{};
}

static void nativeSetMaskKey(u16 &mask, u16 key, bool down) {
    if (down) mask |= key;
    else mask &= (u16)~key;
}

static void nativeSetButtonHeld(u16 key, bool down) {
    nativeSetMaskKey(nativeButtonHeld, key, down);
}

static void nativeSetEvdevHeld(u16 key, bool down) {
    nativeSetMaskKey(nativeEvdevHeld, key, down);
}

static void nativeSetTouchHeld(bool down) {
    nativeTouchHeld = down ? KEY_TOUCH : 0;
}

struct NativeEvdevEvent {
    long tvSec;
    long tvUsec;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

struct NativeJsEvent {
    uint32_t time;
    int16_t value;
    uint8_t type;
    uint8_t number;
};

static void nativeInitEvdev() {
    const char *paths[] = { "/dev/input/event4", "/dev/input/event5" };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        int fd = open(paths[i], O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            nativeEvdevFds[i] = fd;
            nativeEvdevReady = true;
            printf("evdev: opened %s\n", paths[i]);
        }
    }
}

static void nativeInitJoystick() {
    nativeJsFd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (nativeJsFd >= 0) printf("joystick: opened /dev/input/js0\n");
}

static void nativeShutdownEvdev() {
    for (size_t i = 0; i < sizeof(nativeEvdevFds) / sizeof(nativeEvdevFds[0]); ++i) {
        if (nativeEvdevFds[i] >= 0) {
            close(nativeEvdevFds[i]);
            nativeEvdevFds[i] = -1;
        }
    }
    nativeEvdevReady = false;
    nativeEvdevHeld = 0;
}

static void nativeShutdownJoystick() {
    if (nativeJsFd >= 0) {
        close(nativeJsFd);
        nativeJsFd = -1;
    }
    nativeJsAxisHeld = 0;
    nativeJsAxisX = 0;
    nativeJsAxisY = 0;
}

static void nativeApplyEvdevKey(uint16_t code, bool down) {
    switch (code) {
        case 103: nativeSetEvdevHeld(KEY_UP, down); break;
        case 105: nativeSetEvdevHeld(KEY_LEFT, down); break;
        case 106: nativeSetEvdevHeld(KEY_RIGHT, down); break;
        case 108: nativeSetEvdevHeld(KEY_DOWN, down); break;

        case 304: nativeSetEvdevHeld(KEY_A, down); break;
        case 305: nativeSetEvdevHeld(KEY_B, down); break;
        case 306: nativeSetEvdevHeld(KEY_Y, down); break;
        case 307: nativeSetEvdevHeld(KEY_X, down); break;
        case 308: nativeSetEvdevHeld(KEY_L, down); break;
        case 309: nativeSetEvdevHeld(KEY_R, down); break;

        case 310: nativeSetEvdevHeld(KEY_R, down); break;
        case 311: nativeSetEvdevHeld(KEY_START, down); break;
        case 312: nativeSetEvdevHeld(KEY_SELECT, down); break;

        case 314: nativeSetEvdevHeld(KEY_SELECT, down); break;
        case 315: nativeSetEvdevHeld(KEY_START, down); break;
        case 316: nativeSetEvdevHeld(KEY_R, down); break;
        case 354: nativeSetEvdevHeld(KEY_R, down); break;
        default: break;
    }
}

static void nativeUpdateJoystickAxisHeld() {
    nativeJsAxisHeld = 0;
    if (nativeJsAxisX < -NATIVE_ANALOG_DEADZONE) nativeJsAxisHeld |= KEY_LEFT;
    if (nativeJsAxisX > NATIVE_ANALOG_DEADZONE) nativeJsAxisHeld |= KEY_RIGHT;
    if (nativeJsAxisY < -NATIVE_ANALOG_DEADZONE) nativeJsAxisHeld |= KEY_UP;
    if (nativeJsAxisY > NATIVE_ANALOG_DEADZONE) nativeJsAxisHeld |= KEY_DOWN;
}

static void nativePollJoystick() {
    if (nativeJsFd < 0) return;
    NativeJsEvent events[16];
    for (;;) {
        ssize_t got = read(nativeJsFd, events, sizeof(events));
        if (got < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close(nativeJsFd);
            nativeJsFd = -1;
            nativeJsAxisHeld = 0;
            break;
        }
        if (got == 0) break;
        int count = (int)(got / (ssize_t)sizeof(NativeJsEvent));
        for (int ev = 0; ev < count; ++ev) {
            uint8_t type = events[ev].type & 0x7f;
            if (type != 0x02) continue;
            if (events[ev].number == 0) nativeJsAxisX = events[ev].value;
            else if (events[ev].number == 1) nativeJsAxisY = events[ev].value;
        }
        nativeUpdateJoystickAxisHeld();
    }
}

static void nativePollEvdev() {
    NativeEvdevEvent events[16];
    for (size_t i = 0; i < sizeof(nativeEvdevFds) / sizeof(nativeEvdevFds[0]); ++i) {
        int fd = nativeEvdevFds[i];
        if (fd < 0) continue;
        for (;;) {
            ssize_t got = read(fd, events, sizeof(events));
            if (got < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                close(fd);
                nativeEvdevFds[i] = -1;
                break;
            }
            if (got == 0) break;
            int count = (int)(got / (ssize_t)sizeof(NativeEvdevEvent));
            for (int ev = 0; ev < count; ++ev) {
                if (events[ev].type != 1) continue;
                if (events[ev].value == 0) nativeApplyEvdevKey(events[ev].code, false);
                else if (events[ev].value == 1 || events[ev].value == 2) nativeApplyEvdevKey(events[ev].code, true);
            }
        }
    }
}

static int nativeClamp16(int v) {
    if (v < -32768) return -32768;
    if (v > 32767) return 32767;
    return v;
}

static u32 nativeStepForRate(int sampleRate) {
    if (sampleRate < 1) sampleRate = 1;
    return (u32)(((uint64_t)sampleRate << 16) / (u32)nativeAudioRate);
}

static int nativeSampleRateFromDsTimer(u16 timer) {
    int divisor = 65536 - (int)timer;
    if (divisor <= 0) return NATIVE_DS_SOUND_RATE;
    return (int)(16777216 / divisor);
}

static void nativeStartVoice(NativeVoice &voice, const s16 *data, u32 count, int sampleRate, u8 volume, bool loop, int framesLeft) {
    voice.active = data && count > 0 && volume > 0;
    voice.data = data;
    voice.count = count;
    voice.pos = 0;
    voice.step = nativeStepForRate(sampleRate);
    if (voice.step == 0) voice.step = 1;
    voice.volume = volume > 127 ? 127 : volume;
    voice.loop = loop;
    voice.framesLeft = framesLeft;
}

static int nativeVoiceSample(NativeVoice &voice) {
    if (!voice.active || !voice.data || voice.count == 0) return 0;
    if (voice.framesLeft == 0) {
        voice.active = false;
        return 0;
    }
    u32 idx = voice.pos >> 16;
    if (idx >= voice.count) {
        if (!voice.loop) {
            voice.active = false;
            return 0;
        }
        idx %= voice.count;
        voice.pos = idx << 16;
    }
    int sample = voice.data[idx] * (int)voice.volume / 127;
    voice.pos += voice.step;
    if (voice.loop && (voice.pos >> 16) >= voice.count) {
        voice.pos %= (voice.count << 16);
    }
    if (voice.framesLeft > 0) voice.framesLeft--;
    return sample;
}

static void nativeAudioCallback(void *, Uint8 *stream, int len) {
    s16 *out = (s16 *)stream;
    int frames = len / ((int)sizeof(s16) * 2);
    for (int i = 0; i < frames; ++i) {
        int mixed = 0;
        for (int v = 0; v < NATIVE_MAX_VOICES; ++v) {
            mixed += nativeVoiceSample(nativeVoices[v]);
        }
        mixed += nativeVoiceSample(nativeBombVoice);
        mixed += nativeVoiceSample(nativeMissileVoice);
        mixed += nativeVoiceSample(nativeUfoDroneVoice);
        mixed += nativeVoiceSample(nativeUfoShotVoice);
        mixed = nativeClamp16(mixed);
        out[i * 2] = (s16)mixed;
        out[i * 2 + 1] = (s16)mixed;
    }
}

static void nativeQueueSound(u8 command) {
    if (!nativeAudioDevice || command == 0) return;
    const s16 *data = nullptr;
    u32 count = 0;
    int sampleRate = NATIVE_DS_SOUND_RATE;
    u8 volume = 0;
    switch (command) {
        case 1:
            data = native_audio_shoot_sample;
            count = native_audio_shoot_sample_count;
            volume = native_audio_shoot_sample_volume;
            break;
        case 2:
            data = native_audio_boom_sample;
            count = native_audio_boom_sample_count;
            volume = native_audio_boom_sample_volume;
            break;
        case 3:
            data = audio_warp_sample;
            count = audio_warp_sample_count;
            sampleRate = NATIVE_WAV_SOUND_RATE;
            volume = 118;
            break;
        case 4:
            data = native_audio_life_sample;
            count = native_audio_life_sample_count;
            volume = native_audio_life_sample_volume;
            break;
        case 5:
            data = native_audio_spinner_sample;
            count = native_audio_spinner_sample_count;
            volume = native_audio_spinner_sample_volume;
            break;
        case 6:
            data = audio_cannon_boom_sample;
            count = audio_cannon_boom_sample_count;
            sampleRate = NATIVE_WAV_SOUND_RATE;
            volume = 127;
            break;
        case 7:
            data = audio_intercept_sample;
            count = audio_intercept_sample_count;
            sampleRate = NATIVE_WAV_SOUND_RATE;
            volume = 80;
            break;
        default:
            return;
    }

    SDL_LockAudioDevice(nativeAudioDevice);
    int slot = 0;
    u32 oldest = 0;
    for (int i = 0; i < NATIVE_MAX_VOICES; ++i) {
        if (!nativeVoices[i].active) {
            slot = i;
            oldest = 0;
            break;
        }
        if (nativeVoices[i].pos > oldest) {
            oldest = nativeVoices[i].pos;
            slot = i;
        }
    }
    nativeStartVoice(nativeVoices[slot], data, count, sampleRate, volume, false, -1);
    SDL_UnlockAudioDevice(nativeAudioDevice);
}

static void nativeStopVoice(NativeVoice &voice) {
    voice.active = false;
    voice.framesLeft = 0;
}

static void nativeUpdateContinuousAudio(u16 bombTimer, u16 bombVolume, bool missileActive, bool ufoActive, u16 ufoShotSeq) {
    if (!nativeAudioDevice) return;
    SDL_LockAudioDevice(nativeAudioDevice);

    if (missileActive) {
        if (nativeLastBombTimer != 0 || nativeLastBombVolume != 0) nativeStopVoice(nativeBombVoice);
        nativeLastBombTimer = 0;
        nativeLastBombVolume = 0;
        if (nativeMissileNoteWait == 0) {
            nativeStartVoice(nativeMissileVoice, native_audio_missile_note_sample, native_audio_missile_note_sample_count, nativeSampleRateFromDsTimer(65369), 44, true, nativeAudioRate / 2);
            nativeMissileNoteFrames = 30;
            nativeMissileNoteWait = 59;
        } else {
            nativeMissileNoteWait--;
        }
        if (nativeMissileNoteFrames > 0) nativeMissileNoteFrames--;
    } else {
        nativeMissileNoteWait = 0;
        nativeMissileNoteFrames = 0;
        nativeStopVoice(nativeMissileVoice);
        if (bombTimer == 0 || bombVolume == 0) {
            if (nativeLastBombTimer != 0 || nativeLastBombVolume != 0) nativeStopVoice(nativeBombVoice);
            nativeLastBombTimer = 0;
            nativeLastBombVolume = 0;
        } else if (bombTimer != nativeLastBombTimer || bombVolume != nativeLastBombVolume) {
            nativeStartVoice(nativeBombVoice, native_audio_bomb_tone_sample, native_audio_bomb_tone_sample_count, nativeSampleRateFromDsTimer(bombTimer), bombVolume > 127 ? 127 : (u8)bombVolume, true, -1);
            nativeLastBombTimer = bombTimer;
            nativeLastBombVolume = bombVolume;
        }
    }

    if (ufoActive) {
        if (!nativeUfoDroneVoice.active) {
            nativeStartVoice(nativeUfoDroneVoice, native_audio_ufo_note_sample, native_audio_ufo_note_sample_count, nativeSampleRateFromDsTimer(58395), 34, true, -1);
        }
    } else {
        nativeStopVoice(nativeUfoDroneVoice);
    }

    if (ufoShotSeq != 0 && ufoShotSeq != nativeLastUfoShotSeq) {
        nativeStartVoice(nativeUfoShotVoice, native_audio_ufo_note_sample, native_audio_ufo_note_sample_count, nativeSampleRateFromDsTimer(63751), 56, true, nativeAudioRate / 2);
        nativeLastUfoShotSeq = ufoShotSeq;
    }

    SDL_UnlockAudioDevice(nativeAudioDevice);
}

static void nativeInitAudio() {
    if (nativeAudioDevice) return;
    nativeLastAudioAttempt = SDL_GetTicks();
    SDL_AudioSpec want;
    SDL_AudioSpec got;
    memset(&want, 0, sizeof(want));
    memset(&got, 0, sizeof(got));
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = nativeAudioCallback;

    nativeAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &got, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!nativeAudioDevice) {
        if (!nativeAudioFailureLogged) {
            fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
            nativeAudioFailureLogged = true;
        }
        return;
    }
    nativeAudioFailureLogged = false;
    nativeAudioRate = got.freq > 0 ? got.freq : want.freq;
    printf("audio: %dHz %dch\n", got.freq, got.channels);
    SDL_PauseAudioDevice(nativeAudioDevice, 0);
}

static void nativeRetryAudio() {
    if (nativeAudioDevice) return;
    Uint32 now = SDL_GetTicks();
    if (now - nativeLastAudioAttempt >= 3000) nativeInitAudio();
}

static void nativeShutdownAudio() {
    if (nativeAudioDevice) {
        SDL_CloseAudioDevice(nativeAudioDevice);
        nativeAudioDevice = 0;
    }
}

static int clampIntNative(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void nativeRecordTouch(float fx, float fy) {
    // RG DS Linux currently exposes only the lower digitizer, horizontally packed into the right half.
    float panelX = (fx - 0.5f) / 0.5f;
    nativeTouchX = clampIntNative((int)(panelX * 256.0f + 0.5f), 0, 255);
    nativeTouchY = clampIntNative((int)(fy * 192.0f + 0.5f), 0, 191);
    nativeTouchPending = true;
    nativeSetTouchHeld(true);
}

static void nativePollEvents() {
    nativePollEvdev();
    nativePollJoystick();

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                nativeRunning = false;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                bool down = ev.type == SDL_KEYDOWN;
                if (ev.key.repeat) break;
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE: if (down) nativeRunning = false; break;
                    case SDLK_LEFT: nativeSetButtonHeld(KEY_LEFT, down); break;
                    case SDLK_RIGHT: nativeSetButtonHeld(KEY_RIGHT, down); break;
                    case SDLK_UP: nativeSetButtonHeld(KEY_UP, down); break;
                    case SDLK_DOWN: nativeSetButtonHeld(KEY_DOWN, down); break;
                    case SDLK_SPACE: case SDLK_z: nativeSetButtonHeld(KEY_A, down); break;
                    case SDLK_LCTRL: case SDLK_x: nativeSetButtonHeld(KEY_B, down); break;
                    case SDLK_RETURN: nativeSetButtonHeld(KEY_START, down); break;
                    case SDLK_BACKSPACE: nativeSetButtonHeld(KEY_SELECT, down); break;
                    case SDLK_a: nativeSetButtonHeld(KEY_X, down); break;
                    case SDLK_s: nativeSetButtonHeld(KEY_Y, down); break;
                    case SDLK_q: nativeSetButtonHeld(KEY_L, down); break;
                    case SDLK_w: nativeSetButtonHeld(KEY_R, down); break;
                    default: break;
                }
                break;
            }
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                if (nativeEvdevReady) break;
                bool down = ev.type == SDL_CONTROLLERBUTTONDOWN;
                switch ((SDL_GameControllerButton)ev.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: nativeSetButtonHeld(KEY_LEFT, down); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: nativeSetButtonHeld(KEY_RIGHT, down); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: nativeSetButtonHeld(KEY_UP, down); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: nativeSetButtonHeld(KEY_DOWN, down); break;
                    case SDL_CONTROLLER_BUTTON_A: nativeSetButtonHeld(KEY_A, down); break;
                    case SDL_CONTROLLER_BUTTON_B: nativeSetButtonHeld(KEY_B, down); break;
                    case SDL_CONTROLLER_BUTTON_X: nativeSetButtonHeld(KEY_X, down); break;
                    case SDL_CONTROLLER_BUTTON_Y: nativeSetButtonHeld(KEY_Y, down); break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: nativeSetButtonHeld(KEY_L, down); break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: nativeSetButtonHeld(KEY_R, down); break;
                    case SDL_CONTROLLER_BUTTON_BACK: nativeSetButtonHeld(KEY_SELECT, down); break;
                    case SDL_CONTROLLER_BUTTON_START: nativeSetButtonHeld(KEY_START, down); break;
                    case SDL_CONTROLLER_BUTTON_GUIDE: if (down && (nativeHeld & KEY_START)) nativeQuitCombo = true; break;
                    default: break;
                }
                break;
            }
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP: {
                if (nativeEvdevReady) break;
                bool down = ev.type == SDL_JOYBUTTONDOWN;
                switch (ev.jbutton.button) {
                    case 0: nativeSetButtonHeld(KEY_A, down); break;
                    case 1: nativeSetButtonHeld(KEY_B, down); break;
                    case 2: nativeSetButtonHeld(KEY_Y, down); break;
                    case 3: nativeSetButtonHeld(KEY_X, down); break;
                    case 4: nativeSetButtonHeld(KEY_L, down); break;
                    case 5: nativeSetButtonHeld(KEY_R, down); break;
                    case 6: nativeSetButtonHeld(KEY_R, down); break;
                    case 7: nativeSetButtonHeld(KEY_START, down); break;
                    case 8: nativeSetButtonHeld(KEY_SELECT, down); break;
                    default: break;
                }
                break;
            }
            case SDL_JOYHATMOTION:
                nativeHatHeld = 0;
                if (ev.jhat.value & SDL_HAT_LEFT) nativeHatHeld |= KEY_LEFT;
                if (ev.jhat.value & SDL_HAT_RIGHT) nativeHatHeld |= KEY_RIGHT;
                if (ev.jhat.value & SDL_HAT_UP) nativeHatHeld |= KEY_UP;
                if (ev.jhat.value & SDL_HAT_DOWN) nativeHatHeld |= KEY_DOWN;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (!nativeController && SDL_IsGameController(ev.cdevice.which)) nativeController = SDL_GameControllerOpen(ev.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                if (nativeController && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(nativeController))) {
                    SDL_GameControllerClose(nativeController);
                    nativeController = nullptr;
                    nativeAxisHeld = 0;
                    nativeHatHeld = 0;
                }
                break;
            case SDL_FINGERDOWN:
            case SDL_FINGERMOTION:
                nativeRecordTouch(ev.tfinger.x, ev.tfinger.y);
                break;
            case SDL_FINGERUP:
                nativeSetTouchHeld(false);
                break;
            case SDL_MOUSEBUTTONDOWN:
                nativeTouchX = clampIntNative(ev.button.x * 256 / 640, 0, 255);
                nativeTouchY = clampIntNative(ev.button.y * 192 / 480, 0, 191);
                nativeTouchPending = true;
                nativeSetTouchHeld(true);
                break;
            case SDL_MOUSEBUTTONUP:
                nativeSetTouchHeld(false);
                break;
            default:
                break;
        }
    }

    if (nativeController) {
        Sint16 lx = SDL_GameControllerGetAxis(nativeController, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ly = SDL_GameControllerGetAxis(nativeController, SDL_CONTROLLER_AXIS_LEFTY);
        nativeAxisHeld = 0;
        if (lx < -NATIVE_ANALOG_DEADZONE) nativeAxisHeld |= KEY_LEFT;
        if (lx > NATIVE_ANALOG_DEADZONE) nativeAxisHeld |= KEY_RIGHT;
        if (ly < -NATIVE_ANALOG_DEADZONE) nativeAxisHeld |= KEY_UP;
        if (ly > NATIVE_ANALOG_DEADZONE) nativeAxisHeld |= KEY_DOWN;
    }

    nativeHeld = nativeButtonHeld | nativeAxisHeld | nativeJsAxisHeld | nativeHatHeld | nativeTouchHeld | nativeEvdevHeld;
    if ((nativeHeld & KEY_START) && (nativeHeld & KEY_SELECT)) nativeQuitCombo = true;
    if (nativeQuitCombo) nativeRunning = false;
    nativePressed = nativeHeld & (u16)~nativePrevHeld;
    nativePrevHeld = nativeHeld;
}

// Main-screen and sub-screen dimensions for the Nintendo DS bitmap backgrounds.
// Gameplay positions use 8-bit fixed point: the high bits are pixels, the low
// FP bits are sub-pixel precision. This keeps motion smooth without floats.
static const int SCREEN_W = 256;
static const int SCREEN_H = 192;
static const int FP = 8;
static const int ONE = 1 << FP;

// Pool sizes are fixed because DS homebrew should avoid heap allocation during
// gameplay. Every active falling object, bullet, and explosion particle lives in
// one of these arrays.
static const int MAX_OBJECTS = 34;
static const int MAX_BULLETS = 8;
static const int MAX_PARTICLES = 48;

// Ground/cannon geometry. GROUND_Y is both the visible impact line and the
// collision/landing line for meteors, bombs, missiles, and UFO projectiles.
static const int GROUND_Y = 171;
static const int PLAYER_Y = 170;
static const int PLAYER_HALF_W = 5;
static const int PLAYER_HIT_TOP = PLAYER_Y - 17;
static const int PLAYER_HIT_BOTTOM = PLAYER_Y + 1;
static const int DEATH_PAUSE_FRAMES = 120;
static const int FALLING_SPAWN_MARGIN = 26;
static const int BOMB_SPAWN_MARGIN = 38;

// Original-scale level thresholds. Earlier builds used compressed values for
// debugging, but these are the normal progression breakpoints.
static const int LEVEL2_SCORE = 1000;
static const int LEVEL3_SCORE = 5000;
static const int LEVEL4_SCORE = 20000;
static const int LEVEL5_SCORE = 50000;
static const int LEVEL6_SCORE = 100000;

// Extra difficulty tiers based on the best score in the current run. They are
// separate from level so long runs can keep getting faster even after level 6.
static const int SPEED_TIER1_SCORE = 40000;
static const int SPEED_TIER2_SCORE = 100000;
static const int SPEED_TIER3_SCORE = 200000;

static const int TARGET_GAME_FPS = 60;
static const int TIMER_TICKS_PER_SECOND = 32728;
static const int TIMER_TICKS_PER_GAME_UPDATE = (TIMER_TICKS_PER_SECOND + TARGET_GAME_FPS / 2) / TARGET_GAME_FPS;

// Any of these keys can dismiss the title screen. During gameplay they keep
// their normal meaning below.
static const u32 START_BUTTON_MASK =
    KEY_A | KEY_B | KEY_SELECT | KEY_START | KEY_RIGHT | KEY_LEFT | KEY_UP | KEY_DOWN |
    KEY_R | KEY_L | KEY_X | KEY_Y;

// User-selectable speed modes. These are gameplay speed percentages, not frame
// rates: the renderer still targets VBlank while movement/timers scale.
static const int GAME_SPEED_STEPS[] = { 50, 70, 90 };
static const int GAME_SPEED_STEP_COUNT = sizeof(GAME_SPEED_STEPS) / sizeof(GAME_SPEED_STEPS[0]);
static const int DEFAULT_GAME_SPEED_STEP = 1;

// Bomb drop pitch ladder. The ARM7 sound core consumes these raw sound timer
// values to step the "falling note" downward while a bomb is on screen.
static const int BOMB_NOTE_STEPS = 40;
static const int UFO_SHOT_INTERVAL_UPDATES = 30;
static const u16 BOMB_NOTE_TIMERS[BOMB_NOTE_STEPS] = {
    64534, 64474, 64411, 64344, 64274, 64199, 64119, 64035, 63945, 63851,
    63751, 63645, 63532, 63413, 63287, 63153, 63011, 62861, 62702, 62533,
    62355, 62166, 61965, 61753, 61528, 61290, 61037, 60770, 60486, 60186,
    59868, 59531, 59174, 58795, 58395, 57970, 57520, 57043, 56539, 56003
};

static inline u16 C(int r, int g, int b) { return RGB15(r, g, b) | BIT(15); }

static const u16 COL_BLACK = C(0, 0, 0);
static const u16 COL_WHITE = C(31, 31, 31);
static const u16 COL_PANEL = C(4, 6, 9);
static const u16 COL_PANEL_2 = C(7, 10, 14);
static const u16 COL_CYAN = C(0, 25, 31);
static const u16 COL_MINT = C(5, 31, 20);
static const u16 COL_RED = C(31, 4, 3);
static const u16 COL_ORANGE = C(31, 17, 4);
static const u16 COL_YELLOW = C(31, 28, 6);
static const u16 COL_PURPLE = C(22, 6, 31);
static const u16 COL_GRAY = C(18, 20, 22);
static const u16 COL_DIM = C(8, 10, 12);
static const u16 COL_HUD_RUST = C(23, 7, 5);

// ObjectType drives both behavior and drawing. Everything that can damage the
// player or be shot is represented as an Object with one of these types.
enum ObjectType {
    OBJ_NONE,
    OBJ_METEOR_BIG,
    OBJ_METEOR_SMALL,
    OBJ_SPINNER_BIG,
    OBJ_SPINNER_SMALL,
    OBJ_MISSILE,
    OBJ_UFO,
    OBJ_SALVO
};

// Generic falling/hostile object. x/y/vx/vy are fixed point values except when
// spawnObject receives pixel inputs and converts them with << FP.
struct Object {
    bool active;
    ObjectType type;
    int x, y;
    int prevX, prevY;
    int vx, vy;
    int radius;
    int seed;
    int fireTimer;
    int soundOrder;
    int soundAge;
    int soundTotalUpdates;
    bool grounded;
};

// Player shots are simple vertical projectiles; all collision is done in
// updateBullets() against the active Object pool.
struct Bullet {
    bool active;
    int x, y;
};

// Short-lived pixels used for explosions, warp flashes, and the cannon death
// animation. They are intentionally cheap to draw.
struct Particle {
    bool active;
    int x, y;
    int vx, vy;
    int life;
    u16 color;
};

// Legacy star data. The current generated bitmap backgrounds provide most of
// the sky, but this remains available for lightweight effects.
struct Star {
    int x, y;
    u8 speed;
    u8 phase;
};

// Back buffers live in main RAM. Each frame is drawn here first, then streamed
// through SDL as a transparent overlay over the RGDS-resolution backdrops.
static u16 backMain[SCREEN_W * SCREEN_H];
static u16 backSub[SCREEN_W * SCREEN_H];
static u16 nativeMainBackdropCompare[SCREEN_W * SCREEN_H];
static const u16 *nativeTopTransparentBase = nullptr;
static const u16 *nativeBottomTransparentBase = nullptr;
static u16 *objectDrawTarget = backMain;

static Object objects[MAX_OBJECTS];
static Bullet bullets[MAX_BULLETS];
static Particle particles[MAX_PARTICLES];
static Star stars[58];
static int drawAlpha = 255;
static u16 objectSpriteScratch[SCREEN_W * SCREEN_H];

// Core run state. Timers are counted in gameplay update ticks, then converted
// with timerByGameSpeed() when the selected speed mode changes duration.
static int playerX = SCREEN_W / 2;
static int playerMoveAccumulator = 0;
static int score = 0;
static int peakScore = 0;
static int lives = 4;
static int nextBonus = 1000;
static int frameCounter = 0;
static int spawnTimer = 20;
static int ufoTimer = 600;
static int fireTimer = 0;
static int hyperTimer = 0;
static int invulnTimer = 0;
static int deathTimer = 0;
static bool deathWillGameOver = false;
static bool autoFire = false;
static bool paused = false;
static bool startScreen = true;
static bool gameOver = false;
static bool lastTouchHeld = false;
static bool lastAutoToggleHeld = false;
static bool lastStartButtonHeld = false;
static bool ignoreTitleStartTouchUntilRelease = false;
static int startScreenFrames = 0;
static int measuredFramesPerSecond = TARGET_GAME_FPS;
static int measuredUpdatesPerSecond = TARGET_GAME_FPS;
static int gameSpeedStep = DEFAULT_GAME_SPEED_STEP;
static u16 currentHeldKeys = 0;
static int nextBombSoundOrder = 1;
static u16 ufoShotSequence = 0;
static int lastLevelForUfoTimer = 1;
static u32 rngState = 0x7A5EED5Du;

static bool objectTouchesPlayer(const Object &o);
static bool objectHasSmoothDraw(const Object &o);

// Reset all real-time pacing diagnostics and accumulator state. This is called
// when a run starts so stale title-screen time does not create a burst of ticks.
static void resetGameplayClock() {
    nativePresentPanel(nativeTop, backMain, nativeTopTransparentBase);
    nativePresentPanel(nativeBottom, backSub, nativeBottomTransparentBase);
}



// Track actual frame and gameplay update rates for the diagnostic counters on
// the lower screen. This is also useful for comparing emulator behavior.
static void sampleGameplayClock(u16 elapsedTicks, int updatesRan) {
    (void)elapsedTicks;
    (void)updatesRan;
    measuredFramesPerSecond = TARGET_GAME_FPS;
    measuredUpdatesPerSecond = TARGET_GAME_FPS;
}


// The custom ARM7 sound core polls shared memory at 0x027FF120. Incrementing
// a per-sound counter is safer than sending one IPC value because multiple
// sound events can happen in the same ARM9 frame.
static void sendArm7SoundCommand(u8 command) {
    nativeQueueSound(command);
}


static void playShoot() { sendArm7SoundCommand(1); }
static void playBoom() { sendArm7SoundCommand(2); }
static void playWarp() { sendArm7SoundCommand(3); }
static void playLife() { sendArm7SoundCommand(4); }
static void playSpinner() { sendArm7SoundCommand(5); }
static void playCannonBoom() { sendArm7SoundCommand(6); }
static void playIntercept() { sendArm7SoundCommand(7); }

// Publish continuous sound state to the ARM7 sound core. Bombs use a timer
// ladder, missiles suppress bomb tones with a warning beep, and UFO state
// drives its drone/shot sounds.
static void publishBombDropSound() {
    if (startScreen || paused || gameOver || deathTimer > 0) {
        nativeUpdateContinuousAudio(0, 0, false, false, ufoShotSequence);
        return;
    }

    Object *trackedBomb = 0;
    Object *trackedMissile = 0;
    bool hasUfo = false;
    int bestOrder = 0x7FFFFFFF;
    int bestMissileOrder = 0x7FFFFFFF;
    for (int i = 0; i < MAX_OBJECTS; i++) {
        Object &o = objects[i];
        if (!o.active) continue;
        if (o.type == OBJ_UFO) hasUfo = true;
        if (o.type == OBJ_MISSILE && o.soundOrder > 0 && o.soundOrder < bestMissileOrder) {
            trackedMissile = &o;
            bestMissileOrder = o.soundOrder;
        }
        if (o.type != OBJ_SPINNER_BIG && o.type != OBJ_SPINNER_SMALL) continue;
        if (o.soundOrder > 0 && o.soundOrder < bestOrder) {
            trackedBomb = &o;
            bestOrder = o.soundOrder;
        }
    }

    if (trackedMissile) {
        nativeUpdateContinuousAudio(0, 0, true, hasUfo, ufoShotSequence);
        return;
    }

    if (!trackedBomb) {
        nativeUpdateContinuousAudio(0, 0, false, hasUfo, ufoShotSequence);
        return;
    }

    int total = trackedBomb->soundTotalUpdates;
    if (total < BOMB_NOTE_STEPS) total = BOMB_NOTE_STEPS;
    int step = (trackedBomb->soundAge * (BOMB_NOTE_STEPS - 1)) / total;
    if (step < 0) step = 0;
    if (step >= BOMB_NOTE_STEPS) step = BOMB_NOTE_STEPS - 1;

    nativeUpdateContinuousAudio(BOMB_NOTE_TIMERS[step], 32, false, hasUfo, ufoShotSequence);
}


// Tiny PCM arrays retained as embedded project audio data. They are marked
// unused because the native SDL path plays generated_audio_rgds.h samples.
static s16 sfxShoot[] __attribute__((aligned(4), unused)) = {
    0, 6200, 11200, 9800, 3400, -4800, -11000, -9200, -2700, 3800, 6200, 3300,
    -1300, -5200, -4100, -600, 2800, 3500, 1200, -1600, -2400, -900, 700, 1200,
    650, -220, -700, -520, -100, 280, 310, 100, -80, -110, -40, 0
};

static s16 sfxBoom[] __attribute__((aligned(4), unused)) = {
    9000, -11800, 14200, -15200, 11600, -8100, 7400, -9100, 12600, -13400, 9200, -4800,
    2600, -6200, 8800, -7600, 4200, -2400, 1600, -3600, 4700, -3800, 2100, -900,
    500, -1900, 2300, -1600, 700, -300, 100, -800, 900, -520, 230, -80,
    20, -140, 120, -60, 20, 0, 0, 0
};

static s16 sfxWarp[] __attribute__((aligned(4), unused)) = {
    -3000, -2100, -1200, -300, 700, 1600, 2600, 3700, 4800, 6100, 7300, 8500,
    6800, 5200, 3600, 2000, 400, -1000, -2400, -3600, -4800, -6100, -7400, -8700,
    -6200, -3900, -1700, 400, 2200, 3900, 5200, 6100, 4700, 3000, 1400, 300,
    -500, -900, -700, -300, 0, 0, 0, 0
};

static s16 sfxLife[] __attribute__((aligned(4), unused)) = {
    0, 2600, 5200, 7800, 10400, 7800, 5200, 2600, 0, 3600, 7200, 10800,
    14400, 10800, 7200, 3600, 0, 4200, 8400, 12600, 16800, 12600, 8400, 4200,
    0, 3000, 6000, 9000, 12000, 9000, 6000, 3000, 0, 1600, 3200, 4800,
    6400, 4800, 3200, 1600, 0, 0, 0, 0
};

static s16 sfxSpinner[] __attribute__((aligned(4), unused)) = {
    8200, 6900, 4200, 800, -3100, -6200, -7600, -6500, -3400, 700, 4300, 6500,
    6600, 4500, 1100, -2400, -5000, -5900, -4800, -2200, 900, 3500, 4800, 4500,
    2700, 200, -2200, -3700, -3900, -2700, -700, 1400, 2800, 3200, 2500, 1000,
    -600, -1800, -2200, -1800, -700, 400, 1100, 1200, 700, 100, -200, 0
};

// Xorshift PRNG. It is deterministic and tiny, which is good enough for object
// placement, sprite jitter, and explosion particles.
static u32 nextRandom() {
    u32 x = rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rngState = x ? x : 0x7A5EED5Du;
    return rngState;
}

static int randomInt(int maxValue) {
    return maxValue > 0 ? (int)(nextRandom() % (u32)maxValue) : 0;
}

static int absInt(int v) { return v < 0 ? -v : v; }

static int floorDivInt(int n, int d) {
    return n >= 0 ? n / d : -((-n + d - 1) / d);
}

// Translate score to the visible game level. Level controls background,
// difficulty scaling, score multiplier, and some enemy availability.
static int levelForScore(int value) {
    if (value >= LEVEL6_SCORE) return 6;
    if (value >= LEVEL5_SCORE) return 5;
    if (value >= LEVEL4_SCORE) return 4;
    if (value >= LEVEL3_SCORE) return 3;
    if (value >= LEVEL2_SCORE) return 2;
    return 1;
}

// Long-run speed tier based on peak score. This is independent of the visible
// level and adds pressure later in a run.
static int speedTier() {
    if (peakScore >= SPEED_TIER3_SCORE) return 3;
    if (peakScore >= SPEED_TIER2_SCORE) return 2;
    if (peakScore >= SPEED_TIER1_SCORE) return 1;
    return 0;
}

// Generic +10% per level scaling used for falling object counts and speeds.
static int levelPercent(int level) {
    return 100 + (level - 1) * 10;
}

static int scaleByLevel(int value, int level) {
    return (value * levelPercent(level) + 50) / 100;
}

// UFOs get special level 5/6 bumps that are separate from normal falling object
// level scaling, matching the requested tuning.
static int ufoLevelPercent(int level) {
    if (level >= 6) return 144;
    if (level >= 5) return 120;
    return 100;
}

static int scaleByUfoLevel(int value, int level) {
    return (value * ufoLevelPercent(level) + 50) / 100;
}

static int gameSpeedPercent() {
    return GAME_SPEED_STEPS[gameSpeedStep];
}

// Scale a movement delta by the selected Easy/Normal/Hard speed. The non-zero
// guard prevents very small velocities from rounding to zero.
static int scaleByGameSpeed(int value) {
    int speed = gameSpeedPercent();
    int scaled = value >= 0 ? (value * speed + 50) / 100 : -((-value * speed + 50) / 100);
    if (value != 0 && scaled == 0) return value > 0 ? 1 : -1;
    return scaled;
}

// Convert a duration expressed at 100% speed into the current speed mode.
// Slower modes lengthen cooldowns; faster modes shorten them.
static int timerByGameSpeed(int frames) {
    int speed = gameSpeedPercent();
    int scaled = (frames * 100 + speed / 2) / speed;
    return scaled > 0 ? scaled : 1;
}

// Cannon fire rate improves with level advancement, then respects the global
// speed mode just like other gameplay timers.
static int cannonFireCooldown(int baseFrames) {
    int level = levelForScore(score);
    int rate = levelPercent(level);
    int frames = (baseFrames * 100 + rate / 2) / rate;
    return timerByGameSpeed(frames);
}

static int nextUfoDelayFrames(int level) {
    int base = 900 - level * 45;
    if (base < 420) base = 420;
    return timerByGameSpeed(base + randomInt(360));
}

// When changing speed modes mid-run, keep existing timers at the same fraction
// of their remaining real-time duration.
static int rescaleTimerForSpeed(int frames, int oldSpeed, int newSpeed) {
    if (frames <= 0) return frames;
    int scaled = (frames * oldSpeed + newSpeed / 2) / newSpeed;
    return scaled > 0 ? scaled : 1;
}

// Low-level software drawing helpers for the 16-bit bitmap back buffers. The DS
// hardware is only used to display the final bitmap; all sprites are drawn here.
static u16 blendColor(u16 src, u16 dst, int alpha) {
    if (alpha >= 255) return src;
    if (alpha <= 0) return dst;
    int sr = src & 31;
    int sg = (src >> 5) & 31;
    int sb = (src >> 10) & 31;
    int dr = dst & 31;
    int dg = (dst >> 5) & 31;
    int db = (dst >> 10) & 31;
    int inv = 255 - alpha;
    int r = (sr * alpha + dr * inv + 127) / 255;
    int g = (sg * alpha + dg * inv + 127) / 255;
    int b = (sb * alpha + db * inv + 127) / 255;
    return C(r, g, b);
}

static void putPixel(u16 *fb, int x, int y, u16 color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    u16 *dst = fb + y * SCREEN_W + x;
    *dst = drawAlpha >= 255 ? color : blendColor(color, *dst, drawAlpha);
}

static int pixelFromFixed(int v) {
    return (v + (ONE / 2)) >> FP;
}

static void fillRect(u16 *fb, int x, int y, int w, int h, u16 color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;
    if (drawAlpha < 255) {
        for (int yy = 0; yy < h; yy++) {
            for (int xx = 0; xx < w; xx++) putPixel(fb, x + xx, y + yy, color);
        }
        return;
    }
    for (int yy = 0; yy < h; yy++) {
        u16 *row = fb + (y + yy) * SCREEN_W + x;
        for (int xx = 0; xx < w; xx++) row[xx] = color;
    }
}

static void drawLine(u16 *fb, int x0, int y0, int x1, int y1, u16 color) {
    int dx = absInt(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -absInt(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        putPixel(fb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Filled triangle used by the rocky asteroid facets and several simple ship
// shapes. Coordinates are clipped indirectly by fillRect/putPixel.
static void fillTriangle(u16 *fb, int x0, int y0, int x1, int y1, int x2, int y2, u16 color) {
    if (y0 > y1) { int tx = x0; x0 = x1; x1 = tx; int ty = y0; y0 = y1; y1 = ty; }
    if (y1 > y2) { int tx = x1; x1 = x2; x2 = tx; int ty = y1; y1 = y2; y2 = ty; }
    if (y0 > y1) { int tx = x0; x0 = x1; x1 = tx; int ty = y0; y0 = y1; y1 = ty; }

    int totalHeight = y2 - y0;
    if (totalHeight == 0) return;

    for (int i = 0; i <= totalHeight; i++) {
        bool secondHalf = i > y1 - y0 || y1 == y0;
        int segmentHeight = secondHalf ? y2 - y1 : y1 - y0;
        if (segmentHeight == 0) continue;

        int ax = x0 + (x2 - x0) * i / totalHeight;
        int bx = secondHalf
            ? x1 + (x2 - x1) * (i - (y1 - y0)) / segmentHeight
            : x0 + (x1 - x0) * i / segmentHeight;
        if (ax > bx) { int t = ax; ax = bx; bx = t; }
        fillRect(fb, ax, y0 + i, bx - ax + 1, 1, color);
    }
}

// Deterministic per-object jitter. Asteroids use it so their outlines stay
// stable frame to frame but still look irregular.
static int pseudoJitter(int seed, int index, int amount) {
    u32 v = (u32)seed ^ ((u32)index * 0x9E3779B9u);
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    return (int)(v % (u32)(amount * 2 + 1)) - amount;
}

static void fillCircle(u16 *fb, int cx, int cy, int r, u16 color) {
    for (int y = -r; y <= r; y++) {
        int yy = cy + y;
        if (yy < 0 || yy >= SCREEN_H) continue;
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) putPixel(fb, cx + x, yy, color);
        }
    }
}

static void drawCircleOutline(u16 *fb, int cx, int cy, int r, u16 color) {
    int x = r;
    int y = 0;
    int err = 0;
    while (x >= y) {
        putPixel(fb, cx + x, cy + y, color);
        putPixel(fb, cx + y, cy + x, color);
        putPixel(fb, cx - y, cy + x, color);
        putPixel(fb, cx - x, cy + y, color);
        putPixel(fb, cx - x, cy - y, color);
        putPixel(fb, cx - y, cy - x, color);
        putPixel(fb, cx + y, cy - x, color);
        putPixel(fb, cx + x, cy - y, color);
        y++;
        if (err <= 0) err += 2 * y + 1;
        if (err > 0) { x--; err -= 2 * x + 1; }
    }
}

static void drawSoftDot(u16 *fb, int x, int y, u16 core, u16 fringe) {
    putPixel(fb, x, y, core);
    putPixel(fb, x - 1, y, fringe);
    putPixel(fb, x + 1, y, fringe);
    putPixel(fb, x, y - 1, fringe);
    putPixel(fb, x, y + 1, fringe);
}

// Minimal 3x5 bitmap font. It is intentionally primitive and fast, and keeps
// the project independent from font assets or text engines.
static void glyphRows(char ch, u8 rows[5]) {
    for (int i = 0; i < 5; i++) rows[i] = 0;
    switch (ch) {
        case '0': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
        case '1': rows[0]=2; rows[1]=6; rows[2]=2; rows[3]=2; rows[4]=7; break;
        case '2': rows[0]=7; rows[1]=1; rows[2]=7; rows[3]=4; rows[4]=7; break;
        case '3': rows[0]=7; rows[1]=1; rows[2]=3; rows[3]=1; rows[4]=7; break;
        case '4': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=1; rows[4]=1; break;
        case '5': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=1; rows[4]=7; break;
        case '6': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=5; rows[4]=7; break;
        case '7': rows[0]=7; rows[1]=1; rows[2]=2; rows[3]=2; rows[4]=2; break;
        case '8': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=7; break;
        case '9': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=1; rows[4]=7; break;
        case 'A': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; break;
        case 'B': rows[0]=6; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=6; break;
        case 'C': rows[0]=7; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=7; break;
        case 'D': rows[0]=6; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=6; break;
        case 'E': rows[0]=7; rows[1]=4; rows[2]=6; rows[3]=4; rows[4]=7; break;
        case 'F': rows[0]=7; rows[1]=4; rows[2]=6; rows[3]=4; rows[4]=4; break;
        case 'G': rows[0]=7; rows[1]=4; rows[2]=5; rows[3]=5; rows[4]=7; break;
        case 'H': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; break;
        case 'I': rows[0]=7; rows[1]=2; rows[2]=2; rows[3]=2; rows[4]=7; break;
        case 'K': rows[0]=5; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=5; break;
        case 'L': rows[0]=4; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=7; break;
        case 'M': rows[0]=5; rows[1]=7; rows[2]=7; rows[3]=5; rows[4]=5; break;
        case 'N': rows[0]=5; rows[1]=7; rows[2]=7; rows[3]=7; rows[4]=5; break;
        case 'O': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
        case 'P': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=4; rows[4]=4; break;
        case 'R': rows[0]=7; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=5; break;
        case 'S': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=1; rows[4]=7; break;
        case 'T': rows[0]=7; rows[1]=2; rows[2]=2; rows[3]=2; rows[4]=2; break;
        case 'U': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
        case 'V': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=2; break;
        case 'W': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=7; rows[4]=5; break;
        case 'X': rows[0]=5; rows[1]=5; rows[2]=2; rows[3]=5; rows[4]=5; break;
        case 'Y': rows[0]=5; rows[1]=5; rows[2]=2; rows[3]=2; rows[4]=2; break;
        case '-': rows[2]=7; break;
        case ':': rows[1]=2; rows[3]=2; break;
        case '/': rows[0]=1; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=4; break;
        default: break;
    }
}

static void drawChar(u16 *fb, int x, int y, char ch, u16 color, int scale) {
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    u8 rows[5];
    glyphRows(ch, rows);
    for (int yy = 0; yy < 5; yy++) {
        for (int xx = 0; xx < 3; xx++) {
            if (rows[yy] & (1 << (2 - xx))) {
                fillRect(fb, x + xx * scale, y + yy * scale, scale, scale, color);
            }
        }
    }
}

static int textPixelWidth(const char *text, int scale) {
    int chars = 0;
    while (text[chars]) chars++;
    return chars > 0 ? chars * 4 * scale - scale : 0;
}

static void drawText(u16 *fb, int x, int y, const char *text, u16 color, int scale) {
    int cursor = x;
    while (*text) {
        if (*text != ' ') drawChar(fb, cursor, y, *text, color, scale);
        cursor += 4 * scale;
        text++;
    }
}

static void drawTextCentered(u16 *fb, int cx, int y, const char *text, u16 color, int scale) {
    drawText(fb, cx - textPixelWidth(text, scale) / 2, y, text, color, scale);
}

static void intToText(int value, char *buf, int bufSize) {
    int idx = 0;
    if (value < 0 && idx < bufSize - 1) {
        buf[idx++] = '-';
        value = -value;
    }
    int start = idx;
    do {
        if (idx >= bufSize - 1) break;
        buf[idx++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0);
    for (int a = start, b = idx - 1; a < b; a++, b--) {
        char t = buf[a]; buf[a] = buf[b]; buf[b] = t;
    }
    buf[idx] = 0;
}

static void drawTextCenteredInBox(u16 *fb, int left, int right, int y, const char *text, u16 color, int maxScale) {
    int maxWidth = right - left + 1;
    int x = left + (maxWidth - textPixelWidth(text, maxScale)) / 2;
    drawText(fb, x, y, text, color, maxScale);
}

static void drawIntCentered(u16 *fb, int cx, int y, int value, u16 color, int scale) {
    char buf[16];
    intToText(value, buf, sizeof(buf));
    drawTextCentered(fb, cx, y, buf, color, scale);
}

// Center an integer inside a fixed panel box. Used for score so 2-digit and
// 6-digit values share the same visual center.
static void drawIntCenteredInBox(u16 *fb, int left, int right, int y, int value, u16 color, int maxScale) {
    char buf[16];
    intToText(value, buf, sizeof(buf));
    drawTextCenteredInBox(fb, left, right, y, buf, color, maxScale);
}

// Add a single particle to the fixed pool. If the pool is full, the effect is
// simply dropped; gameplay must never block on visual effects.
static void addParticle(int x, int y, int vx, int vy, int life, u16 color) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].x = x;
            particles[i].y = y;
            particles[i].vx = vx;
            particles[i].vy = vy;
            particles[i].life = life;
            particles[i].color = color;
            return;
        }
    }
}

// Generic quick burst used for impacts and successful shots.
static void burst(int x, int y, u16 color, int count) {
    for (int i = 0; i < count; i++) {
        int angle = randomInt(256);
        int vx = ((angle & 31) - 16) * (10 + randomInt(18));
        int vy = (((angle >> 3) & 31) - 18) * (10 + randomInt(18));
        addParticle(x << FP, y << FP, vx, vy, timerByGameSpeed(14 + randomInt(18)), color);
    }
}

// Slower upward/sideways burst used during the two-second cannon death pause.
static void slowDeathBurst(int x, int y) {
    for (int i = 0; i < 4; i++) {
        int vx = (randomInt(41) - 20) * (5 + randomInt(10));
        int vy = -(45 + randomInt(80));
        u16 color = (i == 0) ? COL_WHITE : ((i == 1) ? COL_YELLOW : ((i == 2) ? COL_ORANGE : COL_RED));
        addParticle(x << FP, y << FP, vx, vy, DEATH_PAUSE_FRAMES + 10, color);
    }
}

// Apply score changes with the level multiplier. Bonus lives are awarded from
// peak score so temporary negative score changes do not remove progress.
static void addScore(int deltaBase) {
    int mult = levelForScore(score);
    score += deltaBase * mult;
    if (score > peakScore) {
        peakScore = score;
        while (peakScore >= nextBonus) {
            lives++;
            nextBonus += 1000;
            playLife();
        }
    }
}

// Remove all active threats before a respawn or reset.
static void clearThreats() {
    memset(objects, 0, sizeof(objects));
    memset(bullets, 0, sizeof(bullets));
}

// Enter the death sequence: lose a life, pause gameplay, clear threats, and let
// the ballistic explosion particles play for DEATH_PAUSE_FRAMES.
static void loseLife() {
    if (invulnTimer > 0 || deathTimer > 0 || gameOver) return;
    addScore(-100);
    lives--;
    if (lives < 0) lives = 0;
    deathTimer = DEATH_PAUSE_FRAMES;
    deathWillGameOver = (lives <= 0);
    invulnTimer = 0;
    paused = false;
    clearThreats();
    memset(particles, 0, sizeof(particles));
    slowDeathBurst(playerX, GROUND_Y);
    slowDeathBurst(playerX, GROUND_Y);
    playCannonBoom();
}

// Restore a full new run while leaving the title-screen state to the caller.
static void resetGame() {
    memset(objects, 0, sizeof(objects));
    memset(bullets, 0, sizeof(bullets));
    memset(particles, 0, sizeof(particles));
    gameSpeedStep = DEFAULT_GAME_SPEED_STEP;
    playerX = SCREEN_W / 2;
    playerMoveAccumulator = 0;
    score = 0;
    peakScore = 0;
    lives = 4;
    nextBonus = 1000;
    spawnTimer = timerByGameSpeed(30);
    ufoTimer = timerByGameSpeed(600);
    fireTimer = 0;
    hyperTimer = 0;
    invulnTimer = timerByGameSpeed(90);
    deathTimer = 0;
    deathWillGameOver = false;
    autoFire = false;
    paused = false;
    gameOver = false;
    lastAutoToggleHeld = false;
    lastStartButtonHeld = false;
    nextBombSoundOrder = 1;
    ufoShotSequence = 0;
    lastLevelForUfoTimer = 1;
}

// Title-screen transition into active gameplay.
static void startNewRun() {
    resetGame();
    startScreen = false;
    startScreenFrames = 0;
    resetGameplayClock();
}

// Return the first inactive object slot from the fixed object pool.
static Object *freeObject() {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) return &objects[i];
    }
    return 0;
}

// Initialize any object type, including radius, sound tracking, special speed
// multipliers, and visual sizing. x/y are pixel coordinates; vx/vy are fixed
// point deltas per 100%-speed gameplay tick.
static void spawnObject(ObjectType type, int x, int y, int vx, int vy) {
    Object *o = freeObject();
    if (!o) return;
    o->active = true;
    o->type = type;
    o->x = x << FP;
    o->y = y << FP;
    o->prevX = o->x;
    o->prevY = o->y;
    o->vx = vx;
    o->vy = vy;
    o->seed = (int)nextRandom();
    o->fireTimer = timerByGameSpeed(70 + randomInt(70));
    o->soundOrder = 0;
    o->soundAge = 0;
    o->soundTotalUpdates = BOMB_NOTE_STEPS;
    o->grounded = false;
    switch (type) {
        case OBJ_METEOR_BIG: o->radius = 13 + randomInt(4); break;
        case OBJ_METEOR_SMALL: o->radius = 7 + randomInt(3); break;
        case OBJ_SPINNER_BIG: o->radius = 6; break;
        case OBJ_SPINNER_SMALL: o->radius = 4; break;
        case OBJ_MISSILE: o->radius = 3; break;
        case OBJ_UFO: o->radius = 15; break;
        case OBJ_SALVO: o->radius = 4; o->vx = (o->vx * 21) / 8; o->vy = (o->vy * 21) / 8; break;
        default: o->radius = 8; break;
    }

    if (type == OBJ_UFO) {
        o->fireTimer = timerByGameSpeed(UFO_SHOT_INTERVAL_UPDATES);
    }

    if (type == OBJ_METEOR_BIG || type == OBJ_METEOR_SMALL) {
        o->radius = (o->radius * 21 + 50) / 100;
        if (type == OBJ_METEOR_BIG) o->radius *= 2;
        if (o->radius < 3) o->radius = 3;
    }

    if (type == OBJ_SPINNER_BIG || type == OBJ_SPINNER_SMALL) {
        o->soundOrder = nextBombSoundOrder++;
        if (nextBombSoundOrder < 1) nextBombSoundOrder = 1;
        o->radius = (o->radius * 7 + 5) / 10;
        if (o->radius < 3) o->radius = 3;
        int safeVy = vy > 0 ? vy : 1;
        int fallPixels = GROUND_Y + o->radius + 12 - y;
        if (fallPixels < 1) fallPixels = 1;
        o->soundTotalUpdates = timerByGameSpeed((fallPixels << FP) / safeVy);
        if (o->soundTotalUpdates < BOMB_NOTE_STEPS) o->soundTotalUpdates = BOMB_NOTE_STEPS;
    }

    if (type == OBJ_MISSILE) {
        o->vx *= 2;
        o->vy *= 2;
        o->soundOrder = nextBombSoundOrder++;
        if (nextBombSoundOrder < 1) nextBombSoundOrder = 1;
    }
}

// While a UFO is active, new falling objects are intentionally suppressed.
static bool hasActiveUfo() {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (objects[i].active && objects[i].type == OBJ_UFO) return true;
    }
    return false;
}

static bool isAsteroidOrBomb(ObjectType type) {
    return type == OBJ_METEOR_BIG || type == OBJ_METEOR_SMALL ||
           type == OBJ_SPINNER_BIG || type == OBJ_SPINNER_SMALL;
}

static bool objectExitedPlayfieldSide(const Object &o, int px) {
    if (!isAsteroidOrBomb(o.type)) return false;
    return px + o.radius < 0 || px - o.radius >= SCREEN_W;
}

static int randomDriftWithWideAngles(int normalMax) {
    if (normalMax < 1) return 0;
    if (randomInt(100) < 15) {
        int magnitude = normalMax + 1 + randomInt(normalMax);
        return randomInt(2) ? magnitude : -magnitude;
    }
    return randomInt(normalMax * 2 + 1) - normalMax;
}

// Large asteroids can split into two smaller fragments when destroyed.
static void splitMeteor(const Object &o) {
    int x = o.x >> FP;
    int y = o.y >> FP;
    int level = levelForScore(score);
    int vy = scaleByLevel(128 + level * 18 + speedTier() * 30 + randomInt(105), level);
    spawnObject(OBJ_METEOR_SMALL, x - 5, y, scaleByLevel(-75 - randomInt(70), level), vy + scaleByLevel(randomInt(68), level));
    spawnObject(OBJ_METEOR_SMALL, x + 5, y, scaleByLevel(75 + randomInt(70), level), vy + scaleByLevel(randomInt(68), level));
}

// Base score before the level multiplier is applied.
static int scoreForType(ObjectType type) {
    switch (type) {
        case OBJ_METEOR_BIG: return 10;
        case OBJ_METEOR_SMALL: return 20;
        case OBJ_SPINNER_BIG: return 40;
        case OBJ_SPINNER_SMALL: return 80;
        case OBJ_MISSILE: return 50;
        case OBJ_UFO: return 100;
        default: return 0;
    }
}

// Shared destruction path for bullet hits and other awarded kills.
static void destroyObject(int index, bool award) {
    Object &o = objects[index];
    ObjectType type = o.type;
    int x = o.x >> FP;
    int y = o.y >> FP;
    if (award) addScore(scoreForType(type));
    if (type == OBJ_METEOR_BIG && randomInt(100) < 55) splitMeteor(o);
    burst(x, y, type == OBJ_SPINNER_BIG || type == OBJ_SPINNER_SMALL ? COL_WHITE : COL_ORANGE, 12);
    o.active = false;
    if (award) playIntercept();
    else playBoom();
}

// Spawn a player shot if the cannon cooldown and bullet pool allow it.
static void fireBullet() {
    if (fireTimer > 0 || paused || deathTimer > 0 || gameOver) return;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].x = playerX << FP;
            bullets[i].y = (PLAYER_Y - 15) << FP;
            fireTimer = cannonFireCooldown(autoFire ? 20 : 8);
            playShoot();
            return;
        }
    }
}

// Relocate the cannon randomly and start its cooldown. The visual bursts mark
// the old and new positions.
static void hyperspace() {
    if (hyperTimer > 0 || paused || deathTimer > 0 || gameOver) return;
    burst(playerX, PLAYER_Y - 4, COL_CYAN, 10);
    playerX = 18 + randomInt(SCREEN_W - 36);
    playerMoveAccumulator = 0;
    hyperTimer = timerByGameSpeed(90);
    burst(playerX, PLAYER_Y - 4, COL_MINT, 10);
    playWarp();
}

// Choose the next threat to spawn. Missiles and bombs keep their own fixed
// chances; asteroid density is controlled mostly by the empty-wave branch.
static void spawnWave() {
    int level = levelForScore(score);
    int tier = speedTier();
    int baseVy = 82 + level * 20 + tier * 27;
    int x = FALLING_SPAWN_MARGIN + randomInt(SCREEN_W - FALLING_SPAWN_MARGIN * 2);
    int roll = randomInt(100);
    int missileChance = level >= 2 ? 3 + level / 2 : 0;
    int bombChance = 20;

    if (level >= 4 && ufoTimer <= 0) {
        int fromLeft = randomInt(2);
        int speed = scaleByUfoLevel(203 + randomInt(205), level);
        spawnObject(OBJ_UFO, fromLeft ? -18 : SCREEN_W + 18, 26 + randomInt(24), fromLeft ? speed : -speed, 0);
        ufoTimer = nextUfoDelayFrames(level);
        return;
    }

    if (roll < missileChance) {
        spawnObject(OBJ_MISSILE, x, -8, scaleByLevel(randomInt(31) - 15, level), scaleByLevel(baseVy + 70 + randomInt(95), level));
    } else if (roll < missileChance + bombChance) {
        ObjectType type = randomInt(100) < 35 ? OBJ_SPINNER_SMALL : OBJ_SPINNER_BIG;
        int bombX = BOMB_SPAWN_MARGIN + randomInt(SCREEN_W - BOMB_SPAWN_MARGIN * 2);
        spawnObject(type, bombX, -8, scaleByLevel(randomDriftWithWideAngles(26), level), scaleByLevel(baseVy + 15 + randomInt(135), level));
        playSpinner();
    } else if (roll < 24) {
        return;
    } else {
        ObjectType type = randomInt(100) < 45 ? OBJ_METEOR_SMALL : OBJ_METEOR_BIG;
        int drift = scaleByLevel(randomDriftWithWideAngles(50), level);
        int fall = scaleByLevel(baseVy + randomInt(type == OBJ_METEOR_SMALL ? 158 : 128), level);
        spawnObject(type, x, -10, drift, fall);
    }
}

// Update all active hostile objects. This handles spawn cadence, missile
// steering/ground behavior, UFO firing, movement, ground impacts, and player
// collision checks.
static void updateObjects() {
    int level = levelForScore(score);
    if (level != lastLevelForUfoTimer) {
        if (level >= 4 && lastLevelForUfoTimer < 4) {
            ufoTimer = nextUfoDelayFrames(level);
        }
        lastLevelForUfoTimer = level;
    }
    if (ufoTimer > 0) ufoTimer--;

    if (!hasActiveUfo()) {
        spawnTimer--;
        if (spawnTimer <= 0) {
            spawnWave();
            int delay = 50 - level * 6 - speedTier() * 5;
            delay = (delay * 100 + levelPercent(level) / 2) / levelPercent(level);
            if (delay < 12) delay = 12;
            int jitter = (18 * 100 + levelPercent(level) / 2) / levelPercent(level);
            if (jitter < 6) jitter = 6;
            spawnTimer = timerByGameSpeed(delay + randomInt(jitter));
        }
    }

    for (int i = 0; i < MAX_OBJECTS; i++) {
        Object &o = objects[i];
        if (!o.active) continue;
        o.prevX = o.x;
        o.prevY = o.y;
        if (o.type == OBJ_SPINNER_BIG || o.type == OBJ_SPINNER_SMALL || o.type == OBJ_MISSILE) o.soundAge++;

        if (o.type == OBJ_MISSILE) {
            int target = playerX << FP;
            if (!o.grounded) {
                if (o.x < target) o.vx += scaleByGameSpeed(10);
                if (o.x > target) o.vx -= scaleByGameSpeed(10);
                if (o.vx > 300) o.vx = 300;
                if (o.vx < -300) o.vx = -300;
            }
        }

        if (o.type == OBJ_UFO) {
            o.fireTimer--;
            if (o.fireTimer <= 0) {
                int sx = o.x >> FP;
                int sy = (o.y >> FP) + 8;
                int dx = playerX - sx;
                if (dx < -80) dx = -80;
                if (dx > 80) dx = 80;
                int salvoVx = scaleByUfoLevel(dx * 2 + randomInt(41) - 20, level);
                int salvoVy = scaleByUfoLevel(145 + level * 10 + randomInt(90), level);
                spawnObject(OBJ_SALVO, sx, sy, salvoVx, salvoVy);
                ufoShotSequence++;
                if (ufoShotSequence == 0) ufoShotSequence = 1;
                o.fireTimer = timerByGameSpeed(UFO_SHOT_INTERVAL_UPDATES);
            }
        }

        o.x += scaleByGameSpeed(o.vx);
        o.y += scaleByGameSpeed(o.vy);

        int px = o.x >> FP;
        int py = o.y >> FP;

        if (objectExitedPlayfieldSide(o, px)) {
            o.active = false;
            continue;
        }

        if (invulnTimer == 0 && objectTouchesPlayer(o)) {
            o.active = false;
            loseLife();
            return;
        }

        if (o.type == OBJ_UFO) {
            if (px < -34 || px > SCREEN_W + 34) o.active = false;
            continue;
        }

        if (o.type == OBJ_SALVO && py + o.radius >= GROUND_Y) {
            burst(px, GROUND_Y, COL_WHITE, 8);
            o.active = false;
            playBoom();
            continue;
        }

        if (o.type == OBJ_MISSILE && !o.grounded && py + o.radius >= GROUND_Y) {
            if (randomInt(100) < 65) {
                int horizontalSpeed = absInt(o.vy);
                if (horizontalSpeed < 290) horizontalSpeed = 290;
                o.grounded = true;
                o.y = (GROUND_Y - o.radius) << FP;
                o.vy = 0;
                o.vx = (o.x < (playerX << FP)) ? horizontalSpeed : -horizontalSpeed;
            } else {
                o.active = false;
            }
        } else if (py - o.radius > SCREEN_H || px < -30 || px > SCREEN_W + 30) {
            if (o.type == OBJ_METEOR_BIG) addScore(-5);
            if (o.type == OBJ_METEOR_SMALL) addScore(-10);
            o.active = false;
        } else if ((o.type == OBJ_METEOR_BIG || o.type == OBJ_METEOR_SMALL ||
                    o.type == OBJ_SPINNER_BIG || o.type == OBJ_SPINNER_SMALL) &&
                   py + o.radius >= GROUND_Y) {
            if (o.type == OBJ_METEOR_BIG) addScore(-5);
            if (o.type == OBJ_METEOR_SMALL) addScore(-10);
            if (o.type == OBJ_SPINNER_BIG || o.type == OBJ_SPINNER_SMALL) loseLife();
            burst(px, GROUND_Y, o.type == OBJ_SPINNER_BIG || o.type == OBJ_SPINNER_SMALL ? COL_WHITE : COL_ORANGE, 8);
            o.active = false;
        }
    }
}

// Move cannon shots upward and test them against every active hostile object.
static void updateBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet &b = bullets[i];
        if (!b.active) continue;
        b.y -= scaleByGameSpeed(5 * ONE);
        if ((b.y >> FP) < -8) {
            b.active = false;
            continue;
        }
        int bx = b.x >> FP;
        int by = b.y >> FP;
        for (int j = 0; j < MAX_OBJECTS; j++) {
            Object &o = objects[j];
            if (!o.active) continue;
            int ox = o.x >> FP;
            int oy = o.y >> FP;
            int dx = bx - ox;
            int dy = by - oy;
            int r = o.radius + 3;
            if (dx * dx + dy * dy <= r * r) {
                b.active = false;
                destroyObject(j, true);
                break;
            }
        }
    }
}

// Update explosion particles. Death particles intentionally ignore game speed
// so the death animation lasts exactly two seconds.
static void updateParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle &p = particles[i];
        if (!p.active) continue;
        if (deathTimer > 0) {
            p.x += p.vx;
            p.y += p.vy;
            p.vy += 1;
        } else {
            p.x += scaleByGameSpeed(p.vx);
            p.y += scaleByGameSpeed(p.vy);
            p.vy += scaleByGameSpeed(5);
        }
        p.life--;
        if (p.life <= 0) p.active = false;
    }
}

// Circle-vs-rectangle collision between an object and the visible cannon body.
static bool objectTouchesPlayer(const Object &o) {
    int ox = o.x >> FP;
    int oy = o.y >> FP;
    int closestX = ox;
    int left = playerX - PLAYER_HALF_W;
    int right = playerX + PLAYER_HALF_W;
    if (closestX < left) closestX = left;
    if (closestX > right) closestX = right;

    int closestY = oy;
    if (closestY < PLAYER_HIT_TOP) closestY = PLAYER_HIT_TOP;
    if (closestY > PLAYER_HIT_BOTTOM) closestY = PLAYER_HIT_BOTTOM;

    int dx = ox - closestX;
    int dy = oy - closestY;
    int r = o.radius;
    return dx * dx + dy * dy <= r * r;
}

// Secondary collision sweep after all movement. This catches anything that was
// not handled during the per-object update path.
static void checkPlayerCollisions() {
    if (invulnTimer > 0) return;
    for (int i = 0; i < MAX_OBJECTS; i++) {
        Object &o = objects[i];
        if (!o.active) continue;
        if (objectTouchesPlayer(o)) {
            o.active = false;
            loseLife();
            return;
        }
    }
}

// Held left/right movement for the cannon.
static void updatePlayerMovement() {
    int direction = 0;
    if (currentHeldKeys & KEY_LEFT) direction--;
    if (currentHeldKeys & KEY_RIGHT) direction++;
    if (direction == 0) {
        playerMoveAccumulator = 0;
        return;
    }

    int delta = (4 * ONE * gameSpeedPercent() * 70 + 5000) / 10000;
    if (delta < 1) delta = 1;
    playerMoveAccumulator += direction * delta;
    while (playerMoveAccumulator >= ONE) {
        playerX++;
        playerMoveAccumulator -= ONE;
    }
    while (playerMoveAccumulator <= -ONE) {
        playerX--;
        playerMoveAccumulator += ONE;
    }

    if (playerX < PLAYER_HALF_W) {
        playerX = PLAYER_HALF_W;
        playerMoveAccumulator = 0;
    }
    if (playerX > SCREEN_W - PLAYER_HALF_W) {
        playerX = SCREEN_W - PLAYER_HALF_W;
        playerMoveAccumulator = 0;
    }
}

// Change Easy/Normal/Hard mode and rescale active timers so switching modes
// does not create weird cooldown jumps.
static void setGameSpeedStep(int newStep) {
    while (newStep < 0) newStep += GAME_SPEED_STEP_COUNT;
    newStep %= GAME_SPEED_STEP_COUNT;
    if (newStep == gameSpeedStep) return;

    int oldSpeed = gameSpeedPercent();
    gameSpeedStep = newStep;
    int newSpeed = gameSpeedPercent();
    spawnTimer = rescaleTimerForSpeed(spawnTimer, oldSpeed, newSpeed);
    ufoTimer = rescaleTimerForSpeed(ufoTimer, oldSpeed, newSpeed);
    fireTimer = rescaleTimerForSpeed(fireTimer, oldSpeed, newSpeed);
    hyperTimer = rescaleTimerForSpeed(hyperTimer, oldSpeed, newSpeed);
    invulnTimer = rescaleTimerForSpeed(invulnTimer, oldSpeed, newSpeed);
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        objects[i].fireTimer = rescaleTimerForSpeed(objects[i].fireTimer, oldSpeed, newSpeed);
        objects[i].soundAge = rescaleTimerForSpeed(objects[i].soundAge, oldSpeed, newSpeed);
        objects[i].soundTotalUpdates = rescaleTimerForSpeed(objects[i].soundTotalUpdates, oldSpeed, newSpeed);
    }
}

// One fixed gameplay update. Rendering is deliberately not done here; main()
// may run more than one update before drawing if the real-time clock is behind.
static void updateGame() {
    if (startScreen) return;
    if (paused || gameOver) return;
    if (deathTimer > 0) {
        if ((deathTimer % 3) == 0) slowDeathBurst(playerX, GROUND_Y);
        updateParticles();
        deathTimer--;
        if (deathTimer == 0) {
            clearThreats();
            memset(particles, 0, sizeof(particles));
            playerX = SCREEN_W / 2;
            playerMoveAccumulator = 0;
            spawnTimer = timerByGameSpeed(55);
            fireTimer = 0;
            hyperTimer = 0;
            invulnTimer = timerByGameSpeed(90);
            if (deathWillGameOver) gameOver = true;
            deathWillGameOver = false;
        }
        return;
    }
    if (fireTimer > 0) fireTimer--;
    if (hyperTimer > 0) hyperTimer--;
    if (invulnTimer > 0) invulnTimer--;
    updatePlayerMovement();
    if (autoFire) fireBullet();
    updateObjects();
    updateBullets();
    updateParticles();
    checkPlayerCollisions();
}

// Seed background star positions. Mostly retained for lightweight effects and
// compatibility with earlier generated-background versions.
static void initStars() {
    for (int i = 0; i < 58; i++) {
        stars[i].x = randomInt(SCREEN_W);
        stars[i].y = randomInt(GROUND_Y - 12);
        stars[i].speed = 1 + randomInt(3);
        stars[i].phase = randomInt(64);
    }
}

// Tint the active level background during the death pause without loading a
// separate red asset.
static void applyDeathPalette(u16 *buffer) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        u16 c = buffer[i];
        int r = (c & 31) * 2;
        if (r > 31) r = 31;
        int g = ((c >> 5) & 31) >> 1;
        int b = ((c >> 10) & 31) >> 1;
        buffer[i] = C(r, g, b);
    }
}

// Copy the current level background into the main back buffer and draw the
// double ground line.
static void drawBackground() {
    int idx = levelForScore(score) - 1;
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    memcpy(backMain, levelBackgrounds[idx], SCREEN_W * SCREEN_H * sizeof(u16));
    nativeTop.backdrop = deathTimer > 0
        ? (NativeBackdrop)(NATIVE_BG_LEVEL_1_DEATH + idx)
        : (NativeBackdrop)(NATIVE_BG_LEVEL_1 + idx);
    nativeTopTransparentBase = levelBackgrounds[idx];
    if (deathTimer > 0) {
        applyDeathPalette(backMain);
        memcpy(nativeMainBackdropCompare, levelBackgrounds[idx], SCREEN_W * SCREEN_H * sizeof(u16));
        applyDeathPalette(nativeMainBackdropCompare);
        nativeTopTransparentBase = nativeMainBackdropCompare;
    }
    drawLine(backMain, 0, GROUND_Y, SCREEN_W - 1, GROUND_Y, deathTimer > 0 ? C(31, 6, 4) : C(10, 23, 20));
    drawLine(backMain, 0, GROUND_Y + 1, SCREEN_W - 1, GROUND_Y + 1, deathTimer > 0 ? C(14, 2, 2) : C(4, 12, 9));
}

// Draw a faceted asteroid from deterministic jittered vertices.
static void drawMeteor(const Object &o) {
    int x = pixelFromFixed(o.x);
    int y = pixelFromFixed(o.y);
    int r = o.radius;
    int jag = o.type == OBJ_METEOR_BIG ? 4 : 3;
    u16 shadow = o.type == OBJ_METEOR_BIG ? C(5, 5, 5) : C(7, 7, 7);
    u16 rockDark = o.type == OBJ_METEOR_BIG ? C(10, 8, 7) : C(12, 11, 9);
    u16 rockMid = o.type == OBJ_METEOR_BIG ? C(16, 14, 11) : C(19, 17, 13);
    u16 rockLight = o.type == OBJ_METEOR_BIG ? C(24, 22, 17) : C(27, 25, 18);
    static const int px[10] = { 0, 6, 10, 8, 3, -4, -9, -11, -7, -2 };
    static const int py[10] = { -10, -8, -3, 4, 9, 10, 5, -1, -7, -11 };

    int vx[10];
    int vy[10];
    for (int i = 0; i < 10; i++) {
        vx[i] = x + px[i] * r / 10 + pseudoJitter(o.seed, i, jag);
        vy[i] = y + py[i] * r / 10 + pseudoJitter(o.seed, i + 17, jag);
    }

    for (int i = 0; i < 10; i++) {
        int n = (i + 1) % 10;
        fillTriangle(objectDrawTarget, x + 2, y + 2, vx[i] + 2, vy[i] + 2, vx[n] + 2, vy[n] + 2, shadow);
    }

    for (int i = 0; i < 10; i++) {
        int n = (i + 1) % 10;
        u16 facet = (i < 3) ? rockLight : ((i < 7) ? rockMid : rockDark);
        fillTriangle(objectDrawTarget, x, y, vx[i], vy[i], vx[n], vy[n], facet);
        drawLine(objectDrawTarget, vx[i], vy[i], vx[n], vy[n], C(5, 5, 5));
    }

    drawLine(objectDrawTarget, x - r / 2, y - r / 3, x - r / 8, y - r / 2, C(30, 28, 20));
    drawLine(objectDrawTarget, x - r / 2, y - r / 3, x - r / 3, y + r / 8, C(8, 8, 7));
    fillCircle(objectDrawTarget, x - r / 3, y, r / 4, C(6, 6, 6));
    drawCircleOutline(objectDrawTarget, x - r / 3, y, r / 4, C(14, 13, 11));
    fillCircle(objectDrawTarget, x + r / 4, y + r / 4, r / 5, C(8, 8, 7));
    putPixel(objectDrawTarget, x + r / 3, y - r / 4, rockLight);
    putPixel(objectDrawTarget, x + r / 3 + 1, y - r / 4, rockLight);
}

// Draw the aircraft-style bomb. The 16-step phase table gives a slow spin while
// it falls.
static void drawSpinner(const Object &o) {
    int x = pixelFromFixed(o.x);
    int y = pixelFromFixed(o.y);
    int r = o.radius;
    int phase = ((frameCounter / 10) + o.seed) & 15;
    u16 body = phase & 1 ? C(16, 17, 18) : C(21, 21, 20);
    u16 nose = phase & 1 ? C(25, 24, 21) : C(29, 27, 22);
    u16 shadow = C(3, 4, 5);
    u16 tail = C(9, 9, 9);
    static const int dx[16] = { 0, 2, 3, 4, 4, 4, 3, 2, 0, -2, -3, -4, -4, -4, -3, -2 };
    static const int dy[16] = { -4, -4, -3, -2, 0, 2, 3, 4, 4, 4, 3, 2, 0, -2, -3, -4 };
    int noseX = x + dx[phase] * (r + 2) / 4;
    int noseY = y + dy[phase] * (r + 2) / 4;
    int tailX = x - dx[phase] * (r + 2) / 4;
    int tailY = y - dy[phase] * (r + 2) / 4;
    int sideX = -dy[phase] * (r / 2 + 1) / 4;
    int sideY = dx[phase] * (r / 2 + 1) / 4;

    fillTriangle(objectDrawTarget, noseX + 1, noseY + 1, tailX + sideX + 1, tailY + sideY + 1, tailX - sideX + 1, tailY - sideY + 1, shadow);
    fillTriangle(objectDrawTarget, noseX, noseY, tailX + sideX, tailY + sideY, tailX - sideX, tailY - sideY, body);
    fillTriangle(objectDrawTarget, noseX, noseY, x + sideX / 2, y + sideY / 2, x - sideX / 2, y - sideY / 2, nose);
    fillTriangle(objectDrawTarget, tailX + sideX, tailY + sideY, tailX - sideX, tailY - sideY, tailX - dx[phase] * 2 / 4, tailY - dy[phase] * 2 / 4, tail);
    drawLine(objectDrawTarget, noseX, noseY, tailX + sideX, tailY + sideY, C(28, 28, 27));
    drawSoftDot(objectDrawTarget, x, y, COL_YELLOW, C(18, 13, 5));
}

// Guided missiles are blinking stars, matching the original-game behavior more
// closely than a physical missile shape.
static void drawMissile(const Object &o) {
    int x = pixelFromFixed(o.x);
    int y = pixelFromFixed(o.y);
    bool bright = ((frameCounter + o.seed) / 6) & 1;
    u16 glow = bright ? COL_WHITE : COL_CYAN;
    u16 core = bright ? COL_YELLOW : C(0, 12, 18);
    drawLine(objectDrawTarget, x, y - 5, x, y + 5, C(0, 8, 12));
    drawLine(objectDrawTarget, x - 5, y, x + 5, y, C(0, 8, 12));
    drawLine(objectDrawTarget, x, y - 4, x, y + 4, glow);
    drawLine(objectDrawTarget, x - 4, y, x + 4, y, glow);
    drawLine(objectDrawTarget, x - 3, y - 3, x + 3, y + 3, glow);
    drawLine(objectDrawTarget, x - 3, y + 3, x + 3, y - 3, glow);
    fillCircle(objectDrawTarget, x, y, bright ? 2 : 1, core);
}

// Detailed UFO with flickering lights. Its projectiles are spawned in
// updateObjects() but rendered by drawSalvo().
static void drawUfo(const Object &o) {
    int x = pixelFromFixed(o.x);
    int y = pixelFromFixed(o.y);
    bool blink0 = ((frameCounter + o.seed) / 4) & 1;
    bool blink1 = ((frameCounter + o.seed / 3) / 5) & 1;
    bool blink2 = ((frameCounter + o.seed / 5) / 3) & 1;
    bool blink3 = ((frameCounter + o.seed / 7) / 6) & 1;
    u16 hot = blink0 ? C(31, 8, 3) : C(18, 3, 2);
    u16 cold = blink1 ? C(8, 23, 20) : C(3, 9, 9);
    fillTriangle(objectDrawTarget, x - 16, y + 1, x, y - 7, x + 16, y + 1, C(3, 5, 6));
    fillTriangle(objectDrawTarget, x - 14, y + 3, x, y + 9, x + 14, y + 3, C(8, 9, 9));
    fillRect(objectDrawTarget, x - 13, y - 1, 27, 5, C(15, 14, 12));
    fillRect(objectDrawTarget, x - 8, y - 8, 17, 8, C(5, 11, 11));
    fillRect(objectDrawTarget, x - 5, y - 10, 11, 3, C(10, 16, 15));
    drawLine(objectDrawTarget, x - 16, y + 2, x + 16, y + 2, C(20, 18, 15));
    drawLine(objectDrawTarget, x - 10, y + 6, x + 10, y + 6, C(24, 23, 20));
    drawLine(objectDrawTarget, x - 7, y - 5, x + 5, y - 8, C(18, 19, 17));
    drawLine(objectDrawTarget, x - 12, y + 1, x - 6, y + 4, C(5, 5, 5));
    drawLine(objectDrawTarget, x + 4, y, x + 12, y + 3, C(4, 4, 4));
    drawSoftDot(objectDrawTarget, x - 12, y + 3, hot, blink0 ? C(12, 2, 1) : C(6, 1, 1));
    drawSoftDot(objectDrawTarget, x - 6, y + 5, cold, blink1 ? C(2, 10, 8) : C(1, 5, 4));
    drawSoftDot(objectDrawTarget, x, y + 6, blink2 ? C(31, 25, 8) : C(10, 7, 2), blink2 ? C(12, 7, 2) : C(5, 3, 1));
    drawSoftDot(objectDrawTarget, x + 6, y + 5, blink3 ? C(22, 8, 31) : C(7, 2, 9), blink3 ? C(8, 2, 12) : C(3, 1, 5));
    drawSoftDot(objectDrawTarget, x + 12, y + 3, blink0 ? C(31, 12, 4) : C(13, 3, 1), blink0 ? C(12, 3, 1) : C(6, 1, 1));
    putPixel(objectDrawTarget, x - 11, y + 3, hot);
    putPixel(objectDrawTarget, x - 4, y + 5, cold);
    putPixel(objectDrawTarget, x + 5, y + 5, hot);
    putPixel(objectDrawTarget, x + 12, y + 3, C(22, 18, 8));
}

// UFO projectile: a small grey pulsing orb.
static void drawSalvo(const Object &o) {
    int x = pixelFromFixed(o.x);
    int y = pixelFromFixed(o.y);
    int pulse = ((frameCounter + o.seed) / 5) & 3;
    int r = 3 + (pulse == 1 || pulse == 2 ? 1 : 0);
    fillCircle(objectDrawTarget, x + 1, y + 1, r, C(5, 5, 6));
    fillCircle(objectDrawTarget, x, y, r, C(17, 18, 19));
    fillCircle(objectDrawTarget, x - 1, y - 1, r > 3 ? 2 : 1, C(25, 26, 27));
    drawCircleOutline(objectDrawTarget, x, y, r + 1, C(8, 9, 10));
}

// Skip drawing objects that are fully offscreen. This reduces overdraw in
// DraStic without changing gameplay.
static bool objectVisibleForDraw(const Object &o) {
    int x = o.x >> FP;
    int y = o.y >> FP;
    int pad = o.radius + 26;
    return x + pad >= 0 && x - pad < SCREEN_W && y + pad >= 0 && y - pad < SCREEN_H;
}

static bool objectHasSmoothDraw(const Object &o) {
    return o.type == OBJ_METEOR_BIG || o.type == OBJ_METEOR_SMALL ||
           o.type == OBJ_SPINNER_BIG || o.type == OBJ_SPINNER_SMALL ||
           o.type == OBJ_MISSILE || o.type == OBJ_SALVO;
}

static void drawObjectBody(const Object &o) {
    switch (o.type) {
        case OBJ_METEOR_BIG:
        case OBJ_METEOR_SMALL: drawMeteor(o); break;
        case OBJ_SPINNER_BIG:
        case OBJ_SPINNER_SMALL: drawSpinner(o); break;
        case OBJ_MISSILE: drawMissile(o); break;
        case OBJ_UFO: drawUfo(o); break;
        case OBJ_SALVO: drawSalvo(o); break;
        default: break;
    }
}

static void clearDetailOverlay() {
    if (nativeTop.detailPixels) {
        memset(nativeTop.detailPixels, 0, RGDS_SCREEN_W * RGDS_SCREEN_H * sizeof(u32));
    }
}

static void stampLowPixelToDetail(int sx, int sy, int centerX, int centerY, int centerX2, int centerY2, u16 color) {
    if (!nativeTop.detailPixels || color == 0) return;
    int shiftX2 = centerX2 - centerX * 5;
    int shiftY2 = centerY2 - centerY * 5;
    int left2 = sx * 5 + shiftX2;
    int top2 = sy * 5 + shiftY2;
    int x0 = floorDivInt(left2, 2);
    int y0 = floorDivInt(top2, 2);
    int x1 = floorDivInt(left2 + 4, 2);
    int y1 = floorDivInt(top2 + 4, 2);
    if (x1 < 0 || y1 < 0 || x0 >= RGDS_SCREEN_W || y0 >= RGDS_SCREEN_H) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= RGDS_SCREEN_W) x1 = RGDS_SCREEN_W - 1;
    if (y1 >= RGDS_SCREEN_H) y1 = RGDS_SCREEN_H - 1;

    u32 argb = rgb15ToArgb(color);
    for (int y = y0; y <= y1; y++) {
        u32 *row = nativeTop.detailPixels + y * RGDS_SCREEN_W;
        for (int x = x0; x <= x1; x++) row[x] = argb;
    }
}

static void stampSmoothObjectToDetail(const Object &o) {
    int centerX = pixelFromFixed(o.x);
    int centerY = pixelFromFixed(o.y);
    int pad = o.radius + 26;
    int minX = centerX - pad;
    int maxX = centerX + pad;
    int minY = centerY - pad;
    int maxY = centerY + pad;
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= SCREEN_W) maxX = SCREEN_W - 1;
    if (maxY >= SCREEN_H) maxY = SCREEN_H - 1;

    memset(objectSpriteScratch, 0, sizeof(objectSpriteScratch));
    Object snapped = o;
    snapped.x = centerX << FP;
    snapped.y = centerY << FP;
    u16 *oldTarget = objectDrawTarget;
    int oldAlpha = drawAlpha;
    objectDrawTarget = objectSpriteScratch;
    drawAlpha = 255;
    drawObjectBody(snapped);
    drawAlpha = oldAlpha;
    objectDrawTarget = oldTarget;

    int centerX2 = (o.x * 5 + ONE / 2) / ONE;
    int centerY2 = (o.y * 5 + ONE / 2) / ONE;
    for (int sy = minY; sy <= maxY; sy++) {
        const u16 *row = objectSpriteScratch + sy * SCREEN_W;
        for (int sx = minX; sx <= maxX; sx++) {
            stampLowPixelToDetail(sx, sy, centerX, centerY, centerX2, centerY2, row[sx]);
        }
    }
}

static void drawSmoothObjectsToDetail() {
    clearDetailOverlay();
    if (!nativeTop.detailPixels) return;
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        if (!objectHasSmoothDraw(objects[i])) continue;
        if (!objectVisibleForDraw(objects[i])) continue;
        stampSmoothObjectToDetail(objects[i]);
    }
}

// Dispatch each active object to its type-specific drawing routine.
static void drawObjects() {
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        if (!objectVisibleForDraw(objects[i])) continue;
        if (!objectHasSmoothDraw(objects[i])) drawObjectBody(objects[i]);
    }
}

// Player bullets are thin vertical cyan beams.
static void drawBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        int x = pixelFromFixed(bullets[i].x);
        int y = pixelFromFixed(bullets[i].y);
        if (y < -8 || y >= SCREEN_H + 8) continue;
        drawLine(backMain, x, y + 8, x, y - 7, COL_CYAN);
        putPixel(backMain, x - 1, y, COL_WHITE);
        putPixel(backMain, x + 1, y, COL_WHITE);
    }
}

// Draw particles after objects so explosions appear on top.
static void drawParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) continue;
        int x = pixelFromFixed(particles[i].x);
        int y = pixelFromFixed(particles[i].y);
        putPixel(backMain, x, y, particles[i].color);
        putPixel(backMain, x + 1, y, particles[i].color);
    }
}

// Draw the player cannon unless it is exploding or blinking during respawn
// invulnerability.
static void drawPlayer() {
    if (deathTimer > 0) return;
    if (invulnTimer > 0 && ((frameCounter / 5) & 1)) return;
    int x = playerX;
    int y = PLAYER_Y;
    u16 dark = C(6, 5, 4);
    u16 bronze = C(15, 10, 6);
    u16 silver = C(22, 21, 18);
    u16 highlight = C(29, 28, 24);
    int wheelPhase = (playerX / 3) & 3;

    fillRect(backMain, x - 5, y, 11, 2, dark);
    fillCircle(backMain, x - 3, y - 1, 2, C(8, 7, 6));
    fillCircle(backMain, x + 3, y - 1, 2, C(8, 7, 6));
    putPixel(backMain, x - 3, y - 1, silver);
    putPixel(backMain, x + 3, y - 1, silver);
    if (wheelPhase & 1) {
        putPixel(backMain, x - 3, y - 3, highlight);
        putPixel(backMain, x + 3, y - 3, highlight);
    } else {
        putPixel(backMain, x - 5, y - 1, highlight);
        putPixel(backMain, x + 1, y - 1, highlight);
    }

    fillTriangle(backMain, x, y - 12, x - 5, y - 3, x + 5, y - 3, bronze);
    fillTriangle(backMain, x, y - 12, x - 2, y - 4, x + 2, y - 4, silver);
    fillRect(backMain, x - 2, y - 16, 5, 9, dark);
    fillRect(backMain, x - 1, y - 16, 3, 8, silver);
    fillRect(backMain, x, y - 17, 1, 10, highlight);
    fillRect(backMain, x - 4, y - 5, 9, 4, C(12, 8, 5));
    drawLine(backMain, x - 4, y - 4, x + 4, y - 4, highlight);
    putPixel(backMain, x - 2, y - 7, C(24, 19, 12));
    putPixel(backMain, x + 2, y - 7, C(24, 19, 12));
}

// Lower-screen HUD: uses a generated bitmap template for the neon panels, then
// draws only live values and the hyper fill over it.
static void drawSubScreen() {
    memcpy(backSub, hudTemplates[gameSpeedStep], SCREEN_W * SCREEN_H * sizeof(u16));
    nativeBottom.backdrop = (NativeBackdrop)(NATIVE_BG_HUD_EASY + gameSpeedStep);
    nativeBottomTransparentBase = hudTemplates[gameSpeedStep];

    drawIntCenteredInBox(backSub, 8, 88, 29, score, COL_HUD_RUST, 3);
    drawIntCentered(backSub, 132, 29, levelForScore(score), COL_HUD_RUST, 3);
    drawIntCentered(backSub, 215, 29, lives, COL_HUD_RUST, 3);

    int hyperFull = timerByGameSpeed(90);
    int charge = hyperTimer > 0 ? (hyperFull - hyperTimer) : hyperFull;
    if (charge < 0) charge = 0;
    if (charge > hyperFull) charge = hyperFull;
    int fillW = charge * 151 / hyperFull;
    fillRect(backSub, 14, 93, fillW, 13, COL_HUD_RUST);
}

// Title screen has separate generated top/bottom bitmaps.
static void drawStartScreen() {
    memcpy(backMain, titleTopBg, SCREEN_W * SCREEN_H * sizeof(u16));
    memcpy(backSub, titleBottomBg, SCREEN_W * SCREEN_H * sizeof(u16));
    nativeTop.backdrop = NATIVE_BG_TITLE_TOP;
    nativeBottom.backdrop = NATIVE_BG_TITLE_BOTTOM;
    nativeTopTransparentBase = titleTopBg;
    nativeBottomTransparentBase = titleBottomBg;
}

// Build one complete frame in the software back buffers.
static void drawGame() {
    if (startScreen) {
        clearDetailOverlay();
        drawStartScreen();
        return;
    }
    drawBackground();
    drawObjects();
    drawSmoothObjectsToDetail();
    drawBullets();
    drawParticles();
    drawPlayer();
    if (paused && !gameOver) drawTextCentered(backMain, SCREEN_W / 2, 82, "PAUSED", COL_WHITE, 3);
    if (gameOver) drawTextCentered(backMain, SCREEN_W / 2, 82, "GAME OVER", COL_WHITE, 3);
    drawSubScreen();
}

// Copy software back buffers to VRAM. Both screens use 16-bit bitmap BGs.
static void presentFrame() {
    nativePresentPanel(nativeTop, backMain, nativeTopTransparentBase);
    nativePresentPanel(nativeBottom, backSub, nativeBottomTransparentBase);
}

// Touch regions follow the visible lower-screen panels.
static void handleTouch(int x, int y) {
    if (x >= 171 && x < 252 && y >= 64 && y < 127) {
        setGameSpeedStep((gameSpeedStep + 1) % GAME_SPEED_STEP_COUNT);
        return;
    }
    if (y >= 128 && y < 185) {
        if (x >= 4 && x < 85) {
            autoFire = !autoFire;
        } else if (x >= 88 && x < 166) {
            hyperspace();
        } else if (x >= 171 && x < 252) {
            if (gameOver) resetGame();
            else if (deathTimer == 0) paused = !paused;
        }
    }
}

// Read keypad/touch input, including the ARM7 mirror for touch/X/Y on emulators
// that do not report them correctly through the normal ARM9 path.
static void handleInput() {
    nativePollEvents();
    int down = nativePressed;
    int held = nativeHeld;
    touchPosition touch;
    touch.px = nativeTouchX;
    touch.py = nativeTouchY;
    currentHeldKeys = held;

    bool touchHeld = (held & KEY_TOUCH) != 0;
    bool autoToggleHeld = (held & (KEY_Y | KEY_X)) != 0;
    bool startButtonHeld = (held & START_BUTTON_MASK) != 0;
    if (startScreen) {
        if (startScreenFrames < 45) {
            startScreenFrames++;
            lastTouchHeld = touchHeld;
            lastAutoToggleHeld = autoToggleHeld;
            lastStartButtonHeld = startButtonHeld;
            return;
        }
        if ((down & START_BUTTON_MASK) ||
            (startButtonHeld && !lastStartButtonHeld) ||
            (touchHeld && !lastTouchHeld)) {
            if (touchHeld && !lastTouchHeld) ignoreTitleStartTouchUntilRelease = true;
            nativeTouchPending = false;
            startNewRun();
        }
        lastTouchHeld = touchHeld;
        lastAutoToggleHeld = autoToggleHeld;
        lastStartButtonHeld = startButtonHeld;
        return;
    }

    if (!touchHeld) {
        ignoreTitleStartTouchUntilRelease = false;
    }
    if (ignoreTitleStartTouchUntilRelease && touchHeld) {
        nativeTouchPending = false;
    } else if ((touchHeld && !lastTouchHeld) || nativeTouchPending) {
        handleTouch(touch.px, touch.py);
        nativeTouchPending = false;
    }
    lastTouchHeld = touchHeld;

    if (down & KEY_START) {
        if (gameOver) resetGame();
        else if (deathTimer == 0) paused = !paused;
    }
    if (autoToggleHeld && !lastAutoToggleHeld) autoFire = !autoFire;
    lastAutoToggleHeld = autoToggleHeld;
    if (down & KEY_L) setGameSpeedStep(gameSpeedStep - 1);
    if (down & (KEY_R | KEY_SELECT)) setGameSpeedStep(gameSpeedStep + 1);
    if (down & KEY_B) hyperspace();
    if (down & KEY_A) fireBullet();
}


// Hardware setup and main loop. The loop reads elapsed real time, runs however
// many fixed gameplay updates are due, publishes audio state, draws one frame,
// then waits for VBlank before presenting diagnostics.

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#ifdef AIRDOME_DYNAMIC_SDL
    if (!loadDynamicSdl()) return 1;
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    nativeInitAudio();
    nativeInitEvdev();
    nativeInitJoystick();

    int displays = SDL_GetNumVideoDisplays();
    printf("SDL displays: %d\n", displays);
    for (int i = 0; i < displays; ++i) {
        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(i, &mode) == 0) printf("display %d: %dx%d @ %dHz\n", i, mode.w, mode.h, mode.refresh_rate);
    }
    nativeDualDisplay = displays >= 2;
    if (!nativeCreatePanel(nativeTop, "AirdomeRGDS Top", 0, nativeDualDisplay, true)) return 1;
    if (!nativeCreatePanel(nativeBottom, "AirdomeRGDS Bottom", nativeDualDisplay ? 1 : 0, nativeDualDisplay, false)) return 1;

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            nativeController = SDL_GameControllerOpen(i);
            if (nativeController) {
                printf("controller: %s\n", SDL_GameControllerName(nativeController));
                break;
            }
        }
    }

    rngState ^= (u32)time(nullptr);
    rngState ^= 0x51F15EEDu;
    initStars();
    resetGame();
    startScreen = true;
    startScreenFrames = 0;
    resetGameplayClock();
    drawGame();
    presentFrame();

    const Uint32 frameMs = 1000 / TARGET_GAME_FPS;
    while (nativeRunning) {
        Uint32 start = SDL_GetTicks();
        nativeRetryAudio();
        handleInput();
        int updatesRan = 0;
        if (!startScreen) {
            updateGame();
            if (!paused) updatesRan = 1;
            frameCounter++;
        } else {
            frameCounter++;
        }
        publishBombDropSound();
        drawGame();
        presentFrame();
        sampleGameplayClock(TIMER_TICKS_PER_GAME_UPDATE, updatesRan);
        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < frameMs) SDL_Delay(frameMs - elapsed);
    }

    if (nativeController) SDL_GameControllerClose(nativeController);
    nativeShutdownJoystick();
    nativeShutdownEvdev();
    nativeShutdownAudio();
    nativeDestroyPanel(nativeBottom);
    nativeDestroyPanel(nativeTop);
    SDL_Quit();
#ifdef AIRDOME_DYNAMIC_SDL
    unloadDynamicSdl();
#endif
    return 0;
}
