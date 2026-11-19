/* The part of the SDL3 API the ZSNES frontend uses, implemented on SDL 1.2
   (unix/sdl12compat.c) for systems that have nothing newer - Yellow Dog Linux
   and the other PowerPC distributions of its time ship SDL 1.2 only.

   The frontend keeps calling SDL3 names. This header gives them SDL3's types
   and values and points each function at a zs3_ implementation, so nothing
   here collides with libSDL 1.2's own symbols, several of which share a name
   with an SDL3 function of a different signature (SDL_CreateThread,
   SDL_PollEvent, SDL_GetTicks...). Only sdl12compat.c includes SDL 1.2.

   Built with WITH_SDL12=yes, which defines ZSNES_SDL12. What SDL 1.2 cannot do
   is stubbed so the frontend's existing fallbacks run: no gamepad API (every
   pad is a plain joystick), no rumble, one display, software video only. */
#ifndef SDL12COMPAT_H
#define SDL12COMPAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "sdl12compat_keys.h"

/* Names SDL 1.2 also uses, for a different thing: the frontend sees SDL3's,
   sdl12compat.c (which has SDL 1.2's in scope) sees them under zs3_. */
#ifdef SDL12COMPAT_IMPL
#define ZS3T(n) zs3_##n
#else
#define ZS3T(n) SDL_##n

#endif

#ifndef SDL12COMPAT_IMPL /* sdl12compat.c has SDL 1.2's */
typedef uint8_t Uint8;
typedef int8_t Sint8;
typedef uint16_t Uint16;
typedef int16_t Sint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef uint64_t Uint64;
typedef int64_t Sint64;

#endif

/* --- init ----------------------------------------------------------------- */

#define ZS3_INIT_AUDIO 0x00000010u
#define ZS3_INIT_VIDEO 0x00000020u
#define ZS3_INIT_JOYSTICK 0x00000200u
#define ZS3_INIT_GAMEPAD 0x00002000u

#define SDL_HINT_NO_SIGNAL_HANDLERS "SDL_NO_SIGNAL_HANDLERS"
#define SDL_HINT_AUDIO_DRIVER "SDL_AUDIO_DRIVER"

#ifndef SDL12COMPAT_IMPL
#define SDL_Init zs3_Init
#define SDL_InitSubSystem zs3_InitSubSystem
#define SDL_QuitSubSystem zs3_QuitSubSystem
#define SDL_WasInit zs3_WasInit
#define SDL_Quit zs3_Quit
#define SDL_GetError zs3_GetError
#define SDL_SetHint zs3_SetHint
#define SDL_GetHint zs3_GetHint
#define SDL_free zs3_free
#define SDL_GetTicks zs3_GetTicks

#endif

bool zs3_Init(Uint32 flags);
bool zs3_InitSubSystem(Uint32 flags);
void zs3_QuitSubSystem(Uint32 flags);
Uint32 zs3_WasInit(Uint32 flags);
void zs3_Quit(void);
char const* zs3_GetError(void);
bool zs3_SetHint(char const* name, char const* value);
char const* zs3_GetHint(char const* name);
void zs3_free(void* p);
Uint64 zs3_GetTicks(void);

/* --- properties and displays: one display, no properties ------------------ */

typedef Uint32 SDL_PropertiesID;
typedef Uint32 SDL_DisplayID;
#define SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN "SDL.display.HDR_enabled"

typedef struct SDL_DisplayMode {
    SDL_DisplayID displayID;
    Uint32 format;
    int w, h;
    float pixel_density;
    float refresh_rate;
} SDL_DisplayMode;

#ifndef SDL12COMPAT_IMPL
#define SDL_GetDisplays zs3_GetDisplays
#define SDL_GetDisplayName zs3_GetDisplayName
#define SDL_GetDisplayProperties zs3_GetDisplayProperties
#define SDL_GetBooleanProperty zs3_GetBooleanProperty
#define SDL_GetCurrentDisplayMode zs3_GetCurrentDisplayMode
#define SDL_GetDisplayForWindow zs3_GetDisplayForWindow
#define SDL_DisableScreenSaver zs3_DisableScreenSaver

#endif

SDL_DisplayID* zs3_GetDisplays(int* count);
char const* zs3_GetDisplayName(SDL_DisplayID id);
SDL_PropertiesID zs3_GetDisplayProperties(SDL_DisplayID id);
bool zs3_GetBooleanProperty(SDL_PropertiesID props, char const* name, bool def);
bool zs3_DisableScreenSaver(void);

/* --- surfaces ------------------------------------------------------------- */

typedef Uint32 ZS3T(PixelFormat);
#define SDL_PIXELFORMAT_RGB565 0x15151002u

typedef enum SDL_ScaleMode {
    SDL_SCALEMODE_NEAREST,
    SDL_SCALEMODE_LINEAR
} SDL_ScaleMode;

typedef struct ZS3T(Rect) {
    int x, y, w, h;
} ZS3T(Rect);

/* The fields the frontend reads, kept current by the shim, and the SDL 1.2
   surface behind them. */
typedef struct ZS3T(Surface) {
    Uint32 flags;
    ZS3T(PixelFormat)
    format;
    int w, h;
    int pitch;
    void* pixels;
    void* sdl12; /* SDL 1.2's own surface */
} ZS3T(Surface);

#ifndef SDL12COMPAT_IMPL
#define SDL_MUSTLOCK(s) zs3_MustLock(s)
#define SDL_CreateSurface zs3_CreateSurface
#define SDL_DestroySurface zs3_DestroySurface
#define SDL_LockSurface zs3_LockSurface
#define SDL_UnlockSurface zs3_UnlockSurface
#define SDL_BlitSurface zs3_BlitSurface
#define SDL_BlitSurfaceScaled zs3_BlitSurfaceScaled
#define SDL_SaveBMP zs3_SaveBMP

#endif

bool zs3_MustLock(ZS3T(Surface) const* s);
ZS3T(Surface) * zs3_CreateSurface(int w, int h, ZS3T(PixelFormat) format);
void zs3_DestroySurface(ZS3T(Surface) * s);
bool zs3_LockSurface(ZS3T(Surface) * s);
void zs3_UnlockSurface(ZS3T(Surface) * s);
bool zs3_BlitSurface(ZS3T(Surface) * src, ZS3T(Rect) const* srcrect, ZS3T(Surface) * dst, ZS3T(Rect) const* dstrect);
bool zs3_BlitSurfaceScaled(ZS3T(Surface) * src, ZS3T(Rect) const* srcrect, ZS3T(Surface) * dst, ZS3T(Rect) const* dstrect, SDL_ScaleMode mode);
bool zs3_SaveBMP(ZS3T(Surface) * s, char const* file);

/* --- the window: SDL 1.2's single video surface --------------------------- */

typedef struct SDL_Window SDL_Window;
typedef Uint64 SDL_WindowFlags;
typedef void* SDL_GLContext;
typedef struct SDL_Renderer SDL_Renderer;

#define SDL_WINDOW_FULLSCREEN 0x0000000000000001u
#define SDL_WINDOW_OPENGL 0x0000000000000002u
#define SDL_WINDOW_RESIZABLE 0x0000000000000020u
#define SDL_WINDOWPOS_CENTERED_DISPLAY(X) (0x2FFF0000u | (X))

#ifndef SDL12COMPAT_IMPL
#define SDL_CreateWindow zs3_CreateWindow
#define SDL_DestroyWindow zs3_DestroyWindow
#define SDL_SetWindowSize zs3_SetWindowSize
#define SDL_SetWindowFullscreen zs3_SetWindowFullscreen
#define SDL_SetWindowPosition zs3_SetWindowPosition
#define SDL_SetWindowMouseGrab zs3_SetWindowMouseGrab
#define SDL_SyncWindow zs3_SyncWindow
#define SDL_GetWindowFlags zs3_GetWindowFlags
#define SDL_GetWindowSizeInPixels zs3_GetWindowSizeInPixels
#define SDL_GetWindowSurface zs3_GetWindowSurface
#define SDL_UpdateWindowSurface zs3_UpdateWindowSurface
#define SDL_StartTextInput zs3_StartTextInput
#define SDL_HideCursor zs3_HideCursor
#define SDL_GetRelativeMouseState zs3_GetRelativeMouseState

#endif

/* OpenGL: not with SDL 1.2; the Mac OS 9 build has it through AGL. SDL 1.2
   has GL names of its own, so sdl12compat.c does not see these. */
#ifndef SDL12COMPAT_IMPL
typedef enum SDL_GLAttr {
    SDL_GL_DOUBLEBUFFER = 5,
    SDL_GL_ACCELERATED_VISUAL = 15
} SDL_GLAttr;

#define SDL_GL_SetAttribute zs3_GL_SetAttribute
#define SDL_GL_SetSwapInterval zs3_GL_SetSwapInterval
#define SDL_GL_CreateContext zs3_GL_CreateContext
#define SDL_GL_DestroyContext zs3_GL_DestroyContext
#define SDL_GL_SwapWindow zs3_GL_SwapWindow

bool zs3_GL_SetAttribute(SDL_GLAttr attr, int value);
bool zs3_GL_SetSwapInterval(int interval);
SDL_GLContext zs3_GL_CreateContext(SDL_Window* win);
bool zs3_GL_DestroyContext(SDL_GLContext ctx);
bool zs3_GL_SwapWindow(SDL_Window* win);
#endif

SDL_Window* zs3_CreateWindow(char const* title, int w, int h, SDL_WindowFlags flags);
void zs3_DestroyWindow(SDL_Window* win);
bool zs3_SetWindowSize(SDL_Window* win, int w, int h);
bool zs3_SetWindowFullscreen(SDL_Window* win, bool fullscreen);
bool zs3_SetWindowPosition(SDL_Window* win, int x, int y);
bool zs3_SetWindowMouseGrab(SDL_Window* win, bool grabbed);
bool zs3_SyncWindow(SDL_Window* win);
SDL_WindowFlags zs3_GetWindowFlags(SDL_Window* win);
bool zs3_GetWindowSizeInPixels(SDL_Window* win, int* w, int* h);
ZS3T(Surface) * zs3_GetWindowSurface(SDL_Window* win);
bool zs3_UpdateWindowSurface(SDL_Window* win);
bool zs3_StartTextInput(SDL_Window* win);
bool zs3_HideCursor(void);
Uint32 zs3_GetRelativeMouseState(float* x, float* y);
SDL_DisplayMode const* zs3_GetCurrentDisplayMode(SDL_DisplayID id);
SDL_DisplayID zs3_GetDisplayForWindow(SDL_Window* win);

/* --- events --------------------------------------------------------------- */

typedef Uint32 SDL_JoystickID;
typedef Uint16 SDL_Keymod;

#define SDL_KMOD_NUM 0x1000u
#define SDL_KMOD_ALT 0x0300u

#define ZS3_BUTTON_LEFT 1
#define ZS3_BUTTON_MIDDLE 2
#define ZS3_BUTTON_RIGHT 3

#define ZS3_HAT_CENTERED 0x00
#define ZS3_HAT_UP 0x01
#define ZS3_HAT_RIGHT 0x02
#define ZS3_HAT_DOWN 0x04
#define ZS3_HAT_LEFT 0x08
#define ZS3_HAT_RIGHTUP (ZS3_HAT_RIGHT | ZS3_HAT_UP)
#define ZS3_HAT_RIGHTDOWN (ZS3_HAT_RIGHT | ZS3_HAT_DOWN)
#define ZS3_HAT_LEFTUP (ZS3_HAT_LEFT | ZS3_HAT_UP)
#define ZS3_HAT_LEFTDOWN (ZS3_HAT_LEFT | ZS3_HAT_DOWN)

typedef enum ZS3T(EventType) {
    SDL_EVENT_FIRST = 0,
    SDL_EVENT_QUIT = 0x100,
    SDL_EVENT_WINDOW_MOUSE_ENTER = 0x20C,
    SDL_EVENT_WINDOW_FOCUS_GAINED = 0x20E,
    SDL_EVENT_WINDOW_FOCUS_LOST = 0x20F,
    SDL_EVENT_WINDOW_RESIZED = 0x206,
    SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED = 0x207,
    SDL_EVENT_KEY_DOWN = 0x300,
    SDL_EVENT_KEY_UP,
    SDL_EVENT_TEXT_INPUT = 0x303,
    SDL_EVENT_MOUSE_MOTION = 0x400,
    SDL_EVENT_MOUSE_BUTTON_DOWN,
    SDL_EVENT_MOUSE_BUTTON_UP,
    SDL_EVENT_MOUSE_WHEEL,
    SDL_EVENT_JOYSTICK_AXIS_MOTION = 0x600,
    SDL_EVENT_JOYSTICK_HAT_MOTION = 0x602,
    SDL_EVENT_JOYSTICK_BUTTON_DOWN,
    SDL_EVENT_JOYSTICK_BUTTON_UP,
    SDL_EVENT_JOYSTICK_ADDED,
    SDL_EVENT_JOYSTICK_REMOVED,
    SDL_EVENT_GAMEPAD_AXIS_MOTION = 0x650,
    SDL_EVENT_GAMEPAD_BUTTON_DOWN,
    SDL_EVENT_GAMEPAD_BUTTON_UP
} ZS3T(EventType);

typedef struct ZS3T(KeyboardEvent) {
    Uint32 type;
    SDL_Scancode scancode;
    SDL_Keycode key;
    SDL_Keymod mod;
    bool down;
    bool repeat;
} ZS3T(KeyboardEvent);

typedef struct SDL_TextInputEvent {
    Uint32 type;
    char text[8]; /* one character, UTF-8 */
} SDL_TextInputEvent;

typedef struct ZS3T(MouseMotionEvent) {
    Uint32 type;
    float x, y, xrel, yrel;
} ZS3T(MouseMotionEvent);

typedef struct ZS3T(MouseButtonEvent) {
    Uint32 type;
    Uint8 button;
} ZS3T(MouseButtonEvent);

typedef struct SDL_MouseWheelEvent {
    Uint32 type;
    float x, y;
} SDL_MouseWheelEvent;

typedef struct SDL_WindowEvent {
    Uint32 type;
    Sint32 data1, data2;
} SDL_WindowEvent;

typedef struct ZS3T(JoyAxisEvent) {
    Uint32 type;
    SDL_JoystickID which;
    Uint8 axis;
    Sint16 value;
} ZS3T(JoyAxisEvent);

typedef struct ZS3T(JoyHatEvent) {
    Uint32 type;
    SDL_JoystickID which;
    Uint8 hat;
    Uint8 value;
} ZS3T(JoyHatEvent);

typedef struct ZS3T(JoyButtonEvent) {
    Uint32 type;
    SDL_JoystickID which;
    Uint8 button;
} ZS3T(JoyButtonEvent);

typedef struct SDL_JoyDeviceEvent {
    Uint32 type;
    SDL_JoystickID which;
} SDL_JoyDeviceEvent;

typedef ZS3T(JoyAxisEvent) SDL_GamepadAxisEvent;
typedef ZS3T(JoyButtonEvent) SDL_GamepadButtonEvent;

typedef union ZS3T(Event) {
    Uint32 type;
    ZS3T(KeyboardEvent)
    key;
    SDL_TextInputEvent text;
    ZS3T(MouseMotionEvent)
    motion;
    ZS3T(MouseButtonEvent)
    button;
    SDL_MouseWheelEvent wheel;
    SDL_WindowEvent window;
    ZS3T(JoyAxisEvent)
    jaxis;
    ZS3T(JoyHatEvent)
    jhat;
    ZS3T(JoyButtonEvent)
    jbutton;
    SDL_JoyDeviceEvent jdevice;
    SDL_GamepadAxisEvent gaxis;
    SDL_GamepadButtonEvent gbutton;
} ZS3T(Event);

#ifndef SDL12COMPAT_IMPL
#define SDL_PollEvent zs3_PollEvent
#define SDL_PumpEvents zs3_PumpEvents

#endif

bool zs3_PollEvent(ZS3T(Event) * e);
void zs3_PumpEvents(void);

/* --- joysticks; no gamepad mapping in SDL 1.2 ----------------------------- */

typedef struct ZS3T(Joystick) ZS3T(Joystick);
typedef struct SDL_Gamepad SDL_Gamepad;

typedef enum SDL_GamepadButton {
    SDL_GAMEPAD_BUTTON_SOUTH,
    SDL_GAMEPAD_BUTTON_EAST,
    SDL_GAMEPAD_BUTTON_WEST,
    SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_BACK,
    SDL_GAMEPAD_BUTTON_GUIDE,
    SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_LEFT_STICK,
    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_DPAD_UP,
    SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    SDL_GAMEPAD_BUTTON_DPAD_RIGHT
} SDL_GamepadButton;

typedef enum SDL_GamepadAxis {
    SDL_GAMEPAD_AXIS_LEFTX,
    SDL_GAMEPAD_AXIS_LEFTY,
    SDL_GAMEPAD_AXIS_RIGHTX,
    SDL_GAMEPAD_AXIS_RIGHTY,
    SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
} SDL_GamepadAxis;

#ifndef SDL12COMPAT_IMPL
#define SDL_GetJoysticks zs3_GetJoysticks
#define SDL_OpenJoystick zs3_OpenJoystick
#define SDL_CloseJoystick zs3_CloseJoystick
#define SDL_GetJoystickName zs3_GetJoystickName
#define SDL_GetNumJoystickAxes zs3_GetNumJoystickAxes
#define SDL_GetNumJoystickButtons zs3_GetNumJoystickButtons
#define SDL_GetNumJoystickHats zs3_GetNumJoystickHats
#define SDL_SetJoystickEventsEnabled zs3_SetJoystickEventsEnabled
#define SDL_RumbleJoystick zs3_RumbleJoystick
#define SDL_IsGamepad zs3_IsGamepad
#define SDL_OpenGamepad zs3_OpenGamepad
#define SDL_CloseGamepad zs3_CloseGamepad
#define SDL_GetGamepadName zs3_GetGamepadName
#define SDL_GamepadConnected zs3_GamepadConnected
#define SDL_GetGamepadButton zs3_GetGamepadButton
#define SDL_GetGamepadAxis zs3_GetGamepadAxis
#define SDL_SetGamepadEventsEnabled zs3_SetGamepadEventsEnabled
#define SDL_RumbleGamepad zs3_RumbleGamepad

#endif

SDL_JoystickID* zs3_GetJoysticks(int* count);
ZS3T(Joystick) * zs3_OpenJoystick(SDL_JoystickID id);
void zs3_CloseJoystick(ZS3T(Joystick) * j);
char const* zs3_GetJoystickName(ZS3T(Joystick) * j);
int zs3_GetNumJoystickAxes(ZS3T(Joystick) * j);
int zs3_GetNumJoystickButtons(ZS3T(Joystick) * j);
int zs3_GetNumJoystickHats(ZS3T(Joystick) * j);
void zs3_SetJoystickEventsEnabled(bool enabled);
bool zs3_RumbleJoystick(ZS3T(Joystick) * j, Uint16 lo, Uint16 hi, Uint32 ms);
bool zs3_IsGamepad(SDL_JoystickID id);
SDL_Gamepad* zs3_OpenGamepad(SDL_JoystickID id);
void zs3_CloseGamepad(SDL_Gamepad* g);
char const* zs3_GetGamepadName(SDL_Gamepad* g);
bool zs3_GamepadConnected(SDL_Gamepad* g);
bool zs3_GetGamepadButton(SDL_Gamepad* g, SDL_GamepadButton b);
Sint16 zs3_GetGamepadAxis(SDL_Gamepad* g, SDL_GamepadAxis a);
void zs3_SetGamepadEventsEnabled(bool enabled);
bool zs3_RumbleGamepad(SDL_Gamepad* g, Uint16 lo, Uint16 hi, Uint32 ms);

/* --- threads and semaphores ----------------------------------------------- */

typedef struct ZS3T(Thread) ZS3T(Thread);
typedef struct SDL_Semaphore SDL_Semaphore;
typedef int (*ZS3T(ThreadFunction))(void* data);

#ifndef SDL12COMPAT_IMPL
#define SDL_CreateThread zs3_CreateThread
#define SDL_WaitThread zs3_WaitThread
#define SDL_CreateSemaphore zs3_CreateSemaphore
#define SDL_DestroySemaphore zs3_DestroySemaphore
#define SDL_SignalSemaphore zs3_SignalSemaphore
#define SDL_WaitSemaphoreTimeout zs3_WaitSemaphoreTimeout

#endif

ZS3T(Thread) * zs3_CreateThread(ZS3T(ThreadFunction) fn, char const* name, void* data);
void zs3_WaitThread(ZS3T(Thread) * t, int* status);
SDL_Semaphore* zs3_CreateSemaphore(Uint32 initial);
void zs3_DestroySemaphore(SDL_Semaphore* s);
void zs3_SignalSemaphore(SDL_Semaphore* s);
bool zs3_WaitSemaphoreTimeout(SDL_Semaphore* s, Sint32 ms);

/* --- audio: an SDL3-style push stream over SDL 1.2's callback ------------- */

typedef Uint16 SDL_AudioFormat;
typedef Uint32 SDL_AudioDeviceID;
typedef struct SDL_AudioStream SDL_AudioStream;

#define SDL_AUDIO_S16 0x8010u /* host byte order, as in SDL3 */
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ((SDL_AudioDeviceID)0xFFFFFFFFu)

typedef struct ZS3T(AudioSpec) {
    SDL_AudioFormat format;
    int channels;
    int freq;
} ZS3T(AudioSpec);

typedef void (*SDL_AudioStreamCallback)(void* userdata, SDL_AudioStream* stream, int additional, int total);

#ifndef SDL12COMPAT_IMPL
#define SDL_OpenAudioDeviceStream zs3_OpenAudioDeviceStream
#define SDL_DestroyAudioStream zs3_DestroyAudioStream
#define SDL_ResumeAudioStreamDevice zs3_ResumeAudioStreamDevice
#define SDL_PutAudioStreamData zs3_PutAudioStreamData
#define SDL_GetAudioStreamQueued zs3_GetAudioStreamQueued
#define SDL_SetAudioStreamFrequencyRatio zs3_SetAudioStreamFrequencyRatio
#define SDL_GetAudioStreamDevice zs3_GetAudioStreamDevice
#define SDL_GetAudioDeviceFormat zs3_GetAudioDeviceFormat
#define SDL_GetCurrentAudioDriver zs3_GetCurrentAudioDriver
#define SDL_GetNumAudioDrivers zs3_GetNumAudioDrivers
#define SDL_GetAudioDriver zs3_GetAudioDriver

#endif

SDL_AudioStream* zs3_OpenAudioDeviceStream(SDL_AudioDeviceID dev, ZS3T(AudioSpec) const* spec, SDL_AudioStreamCallback cb, void* userdata);
void zs3_DestroyAudioStream(SDL_AudioStream* s);
bool zs3_ResumeAudioStreamDevice(SDL_AudioStream* s);
bool zs3_PutAudioStreamData(SDL_AudioStream* s, void const* buf, int len);
int zs3_GetAudioStreamQueued(SDL_AudioStream* s);
bool zs3_SetAudioStreamFrequencyRatio(SDL_AudioStream* s, float ratio);
SDL_AudioDeviceID zs3_GetAudioStreamDevice(SDL_AudioStream* s);
bool zs3_GetAudioDeviceFormat(SDL_AudioDeviceID dev, ZS3T(AudioSpec) * spec, int* sample_frames);
char const* zs3_GetCurrentAudioDriver(void);
int zs3_GetNumAudioDrivers(void);
char const* zs3_GetAudioDriver(int index);

#ifndef SDL12COMPAT_IMPL
#define SDL_INIT_AUDIO ZS3_INIT_AUDIO
#define SDL_INIT_VIDEO ZS3_INIT_VIDEO
#define SDL_INIT_JOYSTICK ZS3_INIT_JOYSTICK
#define SDL_INIT_GAMEPAD ZS3_INIT_GAMEPAD
#define SDL_BUTTON_LEFT ZS3_BUTTON_LEFT
#define SDL_BUTTON_MIDDLE ZS3_BUTTON_MIDDLE
#define SDL_BUTTON_RIGHT ZS3_BUTTON_RIGHT
#define SDL_HAT_CENTERED ZS3_HAT_CENTERED
#define SDL_HAT_UP ZS3_HAT_UP
#define SDL_HAT_RIGHT ZS3_HAT_RIGHT
#define SDL_HAT_DOWN ZS3_HAT_DOWN
#define SDL_HAT_LEFT ZS3_HAT_LEFT
#define SDL_HAT_RIGHTUP ZS3_HAT_RIGHTUP
#define SDL_HAT_RIGHTDOWN ZS3_HAT_RIGHTDOWN
#define SDL_HAT_LEFTUP ZS3_HAT_LEFTUP
#define SDL_HAT_LEFTDOWN ZS3_HAT_LEFTDOWN
#endif

#endif
