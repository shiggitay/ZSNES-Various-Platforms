/* SDL3-on-SDL-1.2 for the ZSNES frontend; see sdl12compat.h for why and what.
   This is the only file that sees SDL 1.2's own declarations. */
#include <SDL.h>

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/* SDL 1.2's names are in scope here. The header's SDL3 types that clash
   with them come in as zs3_..., and its SDL3 constants that clash as ZS3_...;
   its function-renaming macros are for the frontend only. */
#define SDL12COMPAT_IMPL
#include "sdl12compat.h"

typedef SDL_Surface SDL12_Surface;
typedef SDL_Event SDL12_Event;
typedef SDL_Rect SDL12_Rect;

typedef zs3_Surface ZSurface;
typedef zs3_Event ZEvent;

/* SDL 1.2's SaveBMP is a macro over SDL_SaveBMP_RW. */
#ifndef SDL_SaveBMP
#define SDL_SaveBMP(surface, file) SDL_SaveBMP_RW(surface, SDL_RWFromFile(file, "wb"), 1)
#endif

/* --- init ----------------------------------------------------------------- */

static Uint32 sdl12_flags(Uint32 const f)
{
    Uint32 r = 0;

    if (f & ZS3_INIT_VIDEO)
        r |= SDL_INIT_VIDEO;
    if (f & ZS3_INIT_AUDIO)
        r |= SDL_INIT_AUDIO;
    if (f & (ZS3_INIT_JOYSTICK | ZS3_INIT_GAMEPAD))
        r |= SDL_INIT_JOYSTICK;
    return r;
}

/* SDL 1.2 starts its audio with the driver SDL_AUDIODRIVER names - KDE
   desktops often set it to arts - and fails outright if that one does. Fall
   back to any driver that starts, through SDL_AUDIODRIVER so SDL's own
   bookkeeping stays right. */
static bool init_audio_any(void)
{
    static char const* const drivers[] = { "alsa", "dsp", "dma", "esd", "arts", "nas" };
    size_t i;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
        return true;
    for (i = 0; i < sizeof drivers / sizeof drivers[0]; i++) {
        setenv("SDL_AUDIODRIVER", drivers[i], 1);
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
            fprintf(stderr, "Audio: using SDL's %s driver\n", drivers[i]);
            return true;
        }
    }
    return false;
}

bool zs3_Init(Uint32 const flags)
{
    Uint32 const f = sdl12_flags(flags);

    /* SDL 1.2 reads a grabbed mouse through XFree86-DGA and only switches it
       off on a clean exit: kill the process and the X server's pointer stays
       frozen. Plain X events are enough here. Left alone if the user set it. */
    setenv("SDL_VIDEO_X11_DGAMOUSE", "0", 0);

    /* No parachute: ZSNES installs its own signal handlers (the SDL3 build
       says so with SDL_HINT_NO_SIGNAL_HANDLERS). */
    if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0 && SDL_Init(SDL_INIT_NOPARACHUTE) != 0)
        return false;
    if ((f & ~SDL_INIT_AUDIO) && SDL_InitSubSystem(f & ~SDL_INIT_AUDIO) != 0)
        return false;
    return !(f & SDL_INIT_AUDIO) || init_audio_any();
}

bool zs3_InitSubSystem(Uint32 const flags) { return zs3_Init(flags); }
void zs3_QuitSubSystem(Uint32 const flags) { SDL_QuitSubSystem(sdl12_flags(flags)); }

Uint32 zs3_WasInit(Uint32 const flags)
{
    Uint32 const on = SDL_WasInit(sdl12_flags(flags));
    Uint32 r = 0;

    if (on & SDL_INIT_VIDEO)
        r |= ZS3_INIT_VIDEO;
    if (on & SDL_INIT_AUDIO)
        r |= ZS3_INIT_AUDIO;
    if (on & SDL_INIT_JOYSTICK)
        r |= flags & (ZS3_INIT_JOYSTICK | ZS3_INIT_GAMEPAD);
    return r;
}

void zs3_Quit(void) { SDL_Quit(); }
char const* zs3_GetError(void) { return SDL_GetError(); }
void zs3_free(void* const p) { free(p); }
Uint64 zs3_GetTicks(void) { return SDL_GetTicks(); }

/* Hints: only the audio driver one means anything to SDL 1.2, through its
   environment variable. Others are remembered and answered back. */
static char hint_audio[64];

bool zs3_SetHint(char const* const name, char const* const value)
{
    if (name && value && strcmp(name, SDL_HINT_AUDIO_DRIVER) == 0) {
        snprintf(hint_audio, sizeof hint_audio, "%s", value);
        setenv("SDL_AUDIODRIVER", value, 1);
    }
    return true;
}

char const* zs3_GetHint(char const* const name)
{
    if (name && strcmp(name, SDL_HINT_AUDIO_DRIVER) == 0 && hint_audio[0])
        return hint_audio;
    return NULL;
}

/* --- displays ------------------------------------------------------------- */

SDL_DisplayID* zs3_GetDisplays(int* const count)
{
    SDL_DisplayID* const ids = (SDL_DisplayID*)calloc(2, sizeof *ids);

    if (ids)
        ids[0] = 1;
    if (count)
        *count = ids ? 1 : 0;
    return ids;
}

char const* zs3_GetDisplayName(SDL_DisplayID const id) { return id == 1 ? "Display" : NULL; }
SDL_PropertiesID zs3_GetDisplayProperties(SDL_DisplayID const id) { return 0; }
bool zs3_GetBooleanProperty(SDL_PropertiesID const p, char const* const n, bool const def) { return def; }
bool zs3_DisableScreenSaver(void) { return true; }

SDL_DisplayMode const* zs3_GetCurrentDisplayMode(SDL_DisplayID const id)
{
    /* SDL 1.2 does not know the refresh rate; zero makes the frontend keep
       its own timer rather than pace on vsync. */
    /* No size either: SDL_VideoInfo only gained current_w/current_h in SDL
       1.2.10, and Yellow Dog Linux 3 has 1.2.5. The frontend only reads the
       refresh rate. */
    static SDL_DisplayMode m;

    m.displayID = 1;
    m.pixel_density = 1.0f;
    m.refresh_rate = 0.0f;
    return &m;
}

SDL_DisplayID zs3_GetDisplayForWindow(SDL_Window* const w) { return 1; }

/* --- surfaces ------------------------------------------------------------- */

static ZSurface* wrap(SDL12_Surface* const s, ZSurface* z)
{
    if (!s)
        return NULL;
    if (!z && !(z = (ZSurface*)calloc(1, sizeof *z)))
        return NULL;
    z->sdl12 = s;
    z->w = s->w;
    z->h = s->h;
    z->pitch = s->pitch;
    z->pixels = s->pixels;
    z->format = s->format->BitsPerPixel == 16 ? SDL_PIXELFORMAT_RGB565 : 0;
    return z;
}

static SDL12_Surface* rgb565(int const w, int const h)
{
    return SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16, 0xF800, 0x07E0, 0x001F, 0);
}

bool zs3_MustLock(ZSurface const* const s)
{
    SDL12_Surface const* const t = (SDL12_Surface const*)s->sdl12;

    return SDL_MUSTLOCK(t);
}

ZSurface* zs3_CreateSurface(int const w, int const h, zs3_PixelFormat const format)
{
    ZSurface* z;

    if (format != SDL_PIXELFORMAT_RGB565)
        return NULL;
    z = wrap(rgb565(w, h), NULL);
    return z;
}

void zs3_DestroySurface(ZSurface* const s)
{
    if (s) {
        SDL_FreeSurface((SDL12_Surface*)s->sdl12);
        free(s);
    }
}

bool zs3_LockSurface(ZSurface* const s)
{
    SDL12_Surface* const t = (SDL12_Surface*)s->sdl12;
    bool const ok = SDL_LockSurface(t) == 0;

    wrap(t, s); /* a locked hardware surface can move */
    return ok;
}

void zs3_UnlockSurface(ZSurface* const s) { SDL_UnlockSurface((SDL12_Surface*)s->sdl12); }

static SDL12_Rect* rect12(zs3_Rect const* const r, SDL12_Rect* const out)
{
    if (!r)
        return NULL;
    out->x = (Sint16)r->x;
    out->y = (Sint16)r->y;
    out->w = (Uint16)r->w;
    out->h = (Uint16)r->h;
    return out;
}

bool zs3_BlitSurface(ZSurface* const src, zs3_Rect const* const sr, ZSurface* const dst, zs3_Rect const* const dr)
{
    SDL12_Rect a, b;

    return SDL_BlitSurface((SDL12_Surface*)src->sdl12, rect12(sr, &a), (SDL12_Surface*)dst->sdl12, rect12(dr, &b)) == 0;
}

/* SDL 1.2 has no scaled blit. Scale the 16-bit picture into a scratch surface
   the size of the destination - nearest neighbour, whatever the mode asked -
   and let SDL convert that to the screen's format. */
bool zs3_BlitSurfaceScaled(ZSurface* const src, zs3_Rect const* const sr, ZSurface* const dst,
    zs3_Rect const* const dr, SDL_ScaleMode const mode)
{
    static SDL12_Surface* scratch;
    SDL12_Surface* const s = (SDL12_Surface*)src->sdl12;
    SDL12_Surface* const d = (SDL12_Surface*)dst->sdl12;
    int const dw = dr ? dr->w : d->w;
    int const dh = dr ? dr->h : d->h;
    int const sx0 = sr ? sr->x : 0, sy0 = sr ? sr->y : 0;
    int const sw = sr ? sr->w : s->w, sh = sr ? sr->h : s->h;
    SDL12_Rect b;
    int x, y;

    (void)mode;
    if (s->format->BitsPerPixel != 16 || dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0)
        return false;
    if (!scratch || scratch->w != dw || scratch->h != dh) {
        if (scratch)
            SDL_FreeSurface(scratch);
        if (!(scratch = rgb565(dw, dh)))
            return false;
    }
    if (SDL_MUSTLOCK(s))
        SDL_LockSurface(s);
    for (y = 0; y < dh; y++) {
        Uint16 const* const in = (Uint16 const*)((Uint8 const*)s->pixels + (size_t)(sy0 + y * sh / dh) * s->pitch) + sx0;
        Uint16* const out = (Uint16*)((Uint8*)scratch->pixels + (size_t)y * scratch->pitch);
        for (x = 0; x < dw; x++)
            out[x] = in[x * sw / dw];
    }
    if (SDL_MUSTLOCK(s))
        SDL_UnlockSurface(s);
    return SDL_BlitSurface(scratch, NULL, d, rect12(dr, &b)) == 0;
}

bool zs3_SaveBMP(ZSurface* const s, char const* const file)
{
    return SDL_SaveBMP((SDL12_Surface*)s->sdl12, file) == 0;
}

/* --- the window ----------------------------------------------------------- */

struct SDL_Window {
    int w, h;
    Uint32 flags; /* SDL 1.2 video flags */
    ZSurface surface;
};

static struct SDL_Window the_window;
static int window_open;

static bool set_mode(struct SDL_Window* const win)
{
    SDL12_Surface* s;

    if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        return false;
    s = SDL_SetVideoMode(win->w, win->h, 0, win->flags);
    if (!s)
        return false;
    wrap(s, &win->surface);
    return true;
}

SDL_Window* zs3_CreateWindow(char const* const title, int const w, int const h, SDL_WindowFlags const flags)
{
    struct SDL_Window* const win = &the_window;

    setenv("SDL_VIDEO_CENTERED", "1", 0);
    win->w = w;
    win->h = h;
    win->flags = SDL_SWSURFACE | SDL_ANYFORMAT;
    if (flags & SDL_WINDOW_FULLSCREEN)
        win->flags |= SDL_FULLSCREEN;
    if (flags & SDL_WINDOW_RESIZABLE)
        win->flags |= SDL_RESIZABLE;
    if (!set_mode(win))
        return NULL;
    SDL_WM_SetCaption(title, title);
    SDL_EnableUNICODE(1);
    window_open = 1;
    return win;
}

void zs3_DestroyWindow(SDL_Window* const win)
{
    /* SDL 1.2 cannot close its window without stopping video; the next
       CreateWindow starts it again. */
    if (window_open) {
        window_open = 0;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

bool zs3_SetWindowSize(SDL_Window* const win, int const w, int const h)
{
    if (!win)
        return false;
    if (win->w == w && win->h == h && SDL_GetVideoSurface())
        return true;
    win->w = w;
    win->h = h;
    return set_mode(win);
}

bool zs3_SetWindowFullscreen(SDL_Window* const win, bool const fs)
{
    if (!win)
        return false;
    if (!!(win->flags & SDL_FULLSCREEN) == fs)
        return true;
    win->flags ^= SDL_FULLSCREEN;
    return set_mode(win);
}

bool zs3_SetWindowPosition(SDL_Window* const win, int const x, int const y) { return true; }

bool zs3_SetWindowMouseGrab(SDL_Window* const win, bool const grab)
{
    SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
    return true;
}

bool zs3_SyncWindow(SDL_Window* const win) { return true; }

SDL_WindowFlags zs3_GetWindowFlags(SDL_Window* const win)
{
    return win && (win->flags & SDL_FULLSCREEN) ? SDL_WINDOW_FULLSCREEN : 0;
}

bool zs3_GetWindowSizeInPixels(SDL_Window* const win, int* const w, int* const h)
{
    SDL12_Surface const* const s = SDL_GetVideoSurface();

    if (w)
        *w = s ? s->w : (win ? win->w : 0);
    if (h)
        *h = s ? s->h : (win ? win->h : 0);
    return s != NULL;
}

ZSurface* zs3_GetWindowSurface(SDL_Window* const win)
{
    SDL12_Surface* const s = SDL_GetVideoSurface();

    if (!win || !s)
        return NULL;
    return wrap(s, &win->surface);
}

bool zs3_UpdateWindowSurface(SDL_Window* const win)
{
    SDL12_Surface* const s = SDL_GetVideoSurface();

    if (!s)
        return false;
    SDL_UpdateRect(s, 0, 0, 0, 0);
    return true;
}

bool zs3_StartTextInput(SDL_Window* const win)
{
    SDL_EnableUNICODE(1);
    return true;
}

bool zs3_HideCursor(void)
{
    SDL_ShowCursor(SDL_DISABLE);
    return true;
}

Uint32 zs3_GetRelativeMouseState(float* const x, float* const y)
{
    int ix = 0, iy = 0;
    Uint32 const b = SDL_GetRelativeMouseState(&ix, &iy);

    if (x)
        *x = (float)ix;
    if (y)
        *y = (float)iy;
    return b;
}

/* --- keyboard: SDL 1.2 keysyms to SDL3 scancodes and keycodes ------------- */

static SDL_Scancode scancode_of(SDLKey const k)
{
    if (k >= SDLK_a && k <= SDLK_z)
        return (SDL_Scancode)(SDL_SCANCODE_A + (k - SDLK_a));
    if (k >= SDLK_1 && k <= SDLK_9)
        return (SDL_Scancode)(SDL_SCANCODE_1 + (k - SDLK_1));
    if (k >= SDLK_F1 && k <= SDLK_F12)
        return (SDL_Scancode)(SDL_SCANCODE_F1 + (k - SDLK_F1));
    if (k >= SDLK_KP1 && k <= SDLK_KP9)
        return (SDL_Scancode)(SDL_SCANCODE_KP_1 + (k - SDLK_KP1));
    switch (k) {
    case SDLK_0:
        return SDL_SCANCODE_0;
    case SDLK_RETURN:
        return SDL_SCANCODE_RETURN;
    case SDLK_ESCAPE:
        return SDL_SCANCODE_ESCAPE;
    case SDLK_BACKSPACE:
        return SDL_SCANCODE_BACKSPACE;
    case SDLK_TAB:
        return SDL_SCANCODE_TAB;
    case SDLK_SPACE:
        return SDL_SCANCODE_SPACE;
    case SDLK_MINUS:
        return SDL_SCANCODE_MINUS;
    case SDLK_EQUALS:
        return SDL_SCANCODE_EQUALS;
    case SDLK_LEFTBRACKET:
        return SDL_SCANCODE_LEFTBRACKET;
    case SDLK_RIGHTBRACKET:
        return SDL_SCANCODE_RIGHTBRACKET;
    case SDLK_BACKSLASH:
        return SDL_SCANCODE_BACKSLASH;
    case SDLK_HASH:
        return SDL_SCANCODE_NONUSHASH;
    case SDLK_SEMICOLON:
        return SDL_SCANCODE_SEMICOLON;
    case SDLK_QUOTE:
        return SDL_SCANCODE_APOSTROPHE;
    case SDLK_BACKQUOTE:
        return SDL_SCANCODE_GRAVE;
    case SDLK_COMMA:
        return SDL_SCANCODE_COMMA;
    case SDLK_PERIOD:
        return SDL_SCANCODE_PERIOD;
    case SDLK_SLASH:
        return SDL_SCANCODE_SLASH;
    case SDLK_LESS:
        return SDL_SCANCODE_NONUSBACKSLASH;
    case SDLK_CAPSLOCK:
        return SDL_SCANCODE_CAPSLOCK;
    case SDLK_PRINT:
        return SDL_SCANCODE_PRINTSCREEN;
    case SDLK_SCROLLOCK:
        return SDL_SCANCODE_SCROLLLOCK;
    case SDLK_PAUSE:
        return SDL_SCANCODE_PAUSE;
    case SDLK_INSERT:
        return SDL_SCANCODE_INSERT;
    case SDLK_HOME:
        return SDL_SCANCODE_HOME;
    case SDLK_PAGEUP:
        return SDL_SCANCODE_PAGEUP;
    case SDLK_DELETE:
        return SDL_SCANCODE_DELETE;
    case SDLK_END:
        return SDL_SCANCODE_END;
    case SDLK_PAGEDOWN:
        return SDL_SCANCODE_PAGEDOWN;
    case SDLK_RIGHT:
        return SDL_SCANCODE_RIGHT;
    case SDLK_LEFT:
        return SDL_SCANCODE_LEFT;
    case SDLK_DOWN:
        return SDL_SCANCODE_DOWN;
    case SDLK_UP:
        return SDL_SCANCODE_UP;
    case SDLK_NUMLOCK:
        return SDL_SCANCODE_NUMLOCKCLEAR;
    case SDLK_KP_DIVIDE:
        return SDL_SCANCODE_KP_DIVIDE;
    case SDLK_KP_MULTIPLY:
        return SDL_SCANCODE_KP_MULTIPLY;
    case SDLK_KP_MINUS:
        return SDL_SCANCODE_KP_MINUS;
    case SDLK_KP_PLUS:
        return SDL_SCANCODE_KP_PLUS;
    case SDLK_KP_ENTER:
        return SDL_SCANCODE_KP_ENTER;
    case SDLK_KP0:
        return SDL_SCANCODE_KP_0;
    case SDLK_KP_PERIOD:
        return SDL_SCANCODE_KP_PERIOD;
    case SDLK_KP_EQUALS:
        return SDL_SCANCODE_KP_EQUALS;
    case SDLK_MENU:
        return SDL_SCANCODE_MENU;
    case SDLK_LCTRL:
        return SDL_SCANCODE_LCTRL;
    case SDLK_LSHIFT:
        return SDL_SCANCODE_LSHIFT;
    case SDLK_LALT:
        return SDL_SCANCODE_LALT;
    case SDLK_LMETA:
    case SDLK_LSUPER:
        return SDL_SCANCODE_LGUI;
    case SDLK_RCTRL:
        return SDL_SCANCODE_RCTRL;
    case SDLK_RSHIFT:
        return SDL_SCANCODE_RSHIFT;
    case SDLK_RALT:
        return SDL_SCANCODE_RALT;
    case SDLK_RMETA:
    case SDLK_RSUPER:
        return SDL_SCANCODE_RGUI;
    case SDLK_MODE:
        return SDL_SCANCODE_MODE;
    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

/* SDL3 gives the keys that type a character that character's code (lower
   case for letters, as SDL 1.2's keysyms already are) and every other key
   its scancode with bit 30 set. */
static SDL_Keycode keycode_of(SDLKey const k, SDL_Scancode const sc)
{
    if ((unsigned)k < 128)
        return (SDL_Keycode)k;
    return sc == SDL_SCANCODE_UNKNOWN ? 0 : ((SDL_Keycode)sc | 0x40000000u);
}

/* --- events --------------------------------------------------------------- */

static ZEvent pending; /* the text event that follows a key press */
static int has_pending;

static void utf8(Uint16 const c, char* const out)
{
    if (c < 0x80) {
        out[0] = (char)c;
        out[1] = 0;
    } else if (c < 0x800) {
        out[0] = (char)(0xC0 | c >> 6);
        out[1] = (char)(0x80 | (c & 0x3F));
        out[2] = 0;
    } else {
        out[0] = (char)(0xE0 | c >> 12);
        out[1] = (char)(0x80 | (c >> 6 & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
        out[3] = 0;
    }
}

/* One SDL 1.2 event as the SDL3 one the frontend expects; 0 to skip it. */
static int translate(SDL12_Event const* const in, ZEvent* const e)
{
    memset(e, 0, sizeof *e);
    switch (in->type) {
    case SDL_QUIT:
        e->type = SDL_EVENT_QUIT;
        return 1;
    case SDL_ACTIVEEVENT:
        if (in->active.state & SDL_APPINPUTFOCUS) {
            e->type = in->active.gain ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
            return 1;
        }
        if ((in->active.state & SDL_APPMOUSEFOCUS) && in->active.gain) {
            e->type = SDL_EVENT_WINDOW_MOUSE_ENTER;
            return 1;
        }
        return 0;
    case SDL_VIDEORESIZE:
        e->type = SDL_EVENT_WINDOW_RESIZED;
        e->window.data1 = in->resize.w;
        e->window.data2 = in->resize.h;
        return 1;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        SDLKey const k = in->key.keysym.sym;

        e->type = in->type == SDL_KEYDOWN ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e->key.scancode = scancode_of(k);
        e->key.key = keycode_of(k, e->key.scancode);
        /* SDL 1.2's KMOD_NUM and KMOD_ALT have SDL3's values. */
        e->key.mod = (SDL_Keymod)in->key.keysym.mod;
        e->key.down = in->type == SDL_KEYDOWN;
        e->key.repeat = false;
        if (in->type == SDL_KEYDOWN) {
            Uint16 const u = in->key.keysym.unicode;

            if (u >= 0x20 && u != 0x7F) {
                memset(&pending, 0, sizeof pending);
                pending.type = SDL_EVENT_TEXT_INPUT;
                utf8(u, pending.text.text);
                has_pending = 1;
            }
        }
        return 1;
    }
    case SDL_MOUSEMOTION:
        e->type = SDL_EVENT_MOUSE_MOTION;
        e->motion.x = in->motion.x;
        e->motion.y = in->motion.y;
        e->motion.xrel = in->motion.xrel;
        e->motion.yrel = in->motion.yrel;
        return 1;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (in->button.button == SDL_BUTTON_WHEELUP || in->button.button == SDL_BUTTON_WHEELDOWN) {
            if (in->type != SDL_MOUSEBUTTONDOWN)
                return 0;
            e->type = SDL_EVENT_MOUSE_WHEEL;
            e->wheel.y = in->button.button == SDL_BUTTON_WHEELUP ? 1.0f : -1.0f;
            return 1;
        }
        e->type = in->type == SDL_MOUSEBUTTONDOWN ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e->button.button = in->button.button; /* 1, 2, 3 in both */
        return 1;
    case SDL_JOYAXISMOTION:
        e->type = SDL_EVENT_JOYSTICK_AXIS_MOTION;
        e->jaxis.which = (SDL_JoystickID)in->jaxis.which + 1;
        e->jaxis.axis = in->jaxis.axis;
        e->jaxis.value = in->jaxis.value;
        return 1;
    case SDL_JOYHATMOTION:
        e->type = SDL_EVENT_JOYSTICK_HAT_MOTION;
        e->jhat.which = (SDL_JoystickID)in->jhat.which + 1;
        e->jhat.hat = in->jhat.hat;
        e->jhat.value = in->jhat.value; /* the same bits in both */
        return 1;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        e->type = in->type == SDL_JOYBUTTONDOWN ? SDL_EVENT_JOYSTICK_BUTTON_DOWN : SDL_EVENT_JOYSTICK_BUTTON_UP;
        e->jbutton.which = (SDL_JoystickID)in->jbutton.which + 1;
        e->jbutton.button = in->jbutton.button;
        return 1;
    default:
        return 0;
    }
}

bool zs3_PollEvent(ZEvent* const e)
{
    SDL12_Event in;

    if (has_pending) {
        has_pending = 0;
        if (e)
            *e = pending;
        return true;
    }
    while (SDL_PollEvent(&in)) {
        ZEvent out;

        if (translate(&in, &out)) {
            if (e)
                *e = out;
            return true;
        }
    }
    return false;
}

void zs3_PumpEvents(void) { SDL_PumpEvents(); }

/* --- joysticks: id = device index + 1, SDL3 keeping 0 for "none" ---------- */

SDL_JoystickID* zs3_GetJoysticks(int* const count)
{
    int const n = SDL_NumJoysticks();
    SDL_JoystickID* const ids = (SDL_JoystickID*)calloc((size_t)(n > 0 ? n : 0) + 1, sizeof *ids);
    int i;

    if (ids)
        for (i = 0; i < n; i++)
            ids[i] = (SDL_JoystickID)i + 1;
    if (count)
        *count = ids ? (n > 0 ? n : 0) : 0;
    return ids;
}

zs3_Joystick* zs3_OpenJoystick(SDL_JoystickID const id)
{
    return id ? (zs3_Joystick*)SDL_JoystickOpen((int)id - 1) : NULL;
}

void zs3_CloseJoystick(zs3_Joystick* const j)
{
    if (j)
        SDL_JoystickClose((SDL_Joystick*)j);
}

char const* zs3_GetJoystickName(zs3_Joystick* const j)
{
    return j ? SDL_JoystickName(SDL_JoystickIndex((SDL_Joystick*)j)) : NULL;
}

int zs3_GetNumJoystickAxes(zs3_Joystick* const j) { return j ? SDL_JoystickNumAxes((SDL_Joystick*)j) : -1; }
int zs3_GetNumJoystickButtons(zs3_Joystick* const j) { return j ? SDL_JoystickNumButtons((SDL_Joystick*)j) : -1; }
int zs3_GetNumJoystickHats(zs3_Joystick* const j) { return j ? SDL_JoystickNumHats((SDL_Joystick*)j) : -1; }
void zs3_SetJoystickEventsEnabled(bool const on) { SDL_JoystickEventState(on ? SDL_ENABLE : SDL_IGNORE); }
bool zs3_RumbleJoystick(zs3_Joystick* const j, Uint16 const lo, Uint16 const hi, Uint32 const ms) { return false; }

/* No gamepad database in SDL 1.2: every pad is a joystick, which the frontend
   already handles. */
bool zs3_IsGamepad(SDL_JoystickID const id) { return false; }
SDL_Gamepad* zs3_OpenGamepad(SDL_JoystickID const id) { return NULL; }
void zs3_CloseGamepad(SDL_Gamepad* const g) { }
char const* zs3_GetGamepadName(SDL_Gamepad* const g) { return NULL; }
bool zs3_GamepadConnected(SDL_Gamepad* const g) { return false; }
bool zs3_GetGamepadButton(SDL_Gamepad* const g, SDL_GamepadButton const b) { return false; }
Sint16 zs3_GetGamepadAxis(SDL_Gamepad* const g, SDL_GamepadAxis const a) { return 0; }
void zs3_SetGamepadEventsEnabled(bool const on) { }
bool zs3_RumbleGamepad(SDL_Gamepad* const g, Uint16 const lo, Uint16 const hi, Uint32 const ms) { return false; }

/* --- threads and semaphores ----------------------------------------------- */

/* The frontend's one thread of its own is a frame timer that only signals
   the semaphore below, and it runs as well without it: the waits then simply
   time out, as on Mac OS 9, which has no threads at all. Yellow Dog Linux 3
   on a 2.4 kernel - LinuxThreads, one process per thread - hung at the first
   frame with it running, so it starts only on request (ZSNES_SDL12_THREADS=1).
   SDL 1.2's audio thread is its own business and unaffected. */
zs3_Thread* zs3_CreateThread(zs3_ThreadFunction const fn, char const* const name, void* const data)
{
    char const* const e = getenv("ZSNES_SDL12_THREADS");

    (void)name;
    if (!e || *e != '1')
        return NULL;
    return (zs3_Thread*)SDL_CreateThread(fn, data);
}

void zs3_WaitThread(zs3_Thread* const t, int* const status) { SDL_WaitThread((SDL_Thread*)t, status); }
SDL_Semaphore* zs3_CreateSemaphore(Uint32 const n) { return (SDL_Semaphore*)SDL_CreateSemaphore(n); }
void zs3_DestroySemaphore(SDL_Semaphore* const s) { SDL_DestroySemaphore((SDL_sem*)s); }
void zs3_SignalSemaphore(SDL_Semaphore* const s) { SDL_SemPost((SDL_sem*)s); }

static double now_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

/* SDL 1.2's timed wait polls with 1 ms sleeps, which a 2.4 kernel (HZ=100)
   rounds up to 10 ms or more - most of a frame. Sleep whole ticks while more
   than one is left, then yield until the time is up. */
bool zs3_WaitSemaphoreTimeout(SDL_Semaphore* const s, Sint32 const ms)
{
    double end;

    if (ms < 0)
        return SDL_SemWait((SDL_sem*)s) == 0;
    end = now_ms() + ms;
    for (;;) {
        double left;

        if (SDL_SemTryWait((SDL_sem*)s) == 0)
            return true;
        left = end - now_ms();
        if (left <= 0.0)
            return false;
        if (left > 12.0)
            SDL_Delay((Uint32)(left - 11.0));
        else
            sched_yield();
    }
}

/* --- audio ---------------------------------------------------------------- */

/* A byte ring between the frontend, which pushes a frame's samples at a
   time, and SDL 1.2's callback, which pulls a device period at a time. */
#define RING_BYTES (1 << 17)

struct SDL_AudioStream {
    Uint8 ring[RING_BYTES];
    unsigned head, tail; /* free-running; the difference is what is queued */
    int freq, channels, samples;
};

static struct SDL_AudioStream* the_stream;

static void audio_callback(void* const userdata, Uint8* const out, int const len)
{
    struct SDL_AudioStream* const s = (struct SDL_AudioStream*)userdata;
    unsigned const have = s->head - s->tail;
    unsigned const n = have < (unsigned)len ? have : (unsigned)len;
    unsigned i;

    for (i = 0; i < n; i++)
        out[i] = s->ring[(s->tail + i) % RING_BYTES];
    s->tail += n;
    if (n < (unsigned)len)
        memset(out + n, 0, (size_t)len - n);
}

/* SDL 1.2 settles on one audio driver at start-up - often aRts or ESD, whose
   sound server need not be running - and gives up if that one cannot open.
   Try the rest before going without sound: ALSA and OSS (/dev/dsp) talk to
   the hardware directly and are on every Linux of the era. */
static int open_any_driver(SDL_AudioSpec* const want)
{
    static char const* const drivers[] = { "alsa", "dsp", "dma", "esd", "arts", "nas" };
    char tried[32] = "";
    size_t i;

    SDL_AudioDriverName(tried, (int)sizeof tried);
    for (i = 0; i < sizeof drivers / sizeof drivers[0]; i++) {
        if (strcmp(tried, drivers[i]) == 0)
            continue;
        SDL_AudioQuit();
        if (SDL_AudioInit(drivers[i]) == 0 && SDL_OpenAudio(want, NULL) == 0) {
            fprintf(stderr, "Audio: using SDL's %s driver\n", drivers[i]);
            return 1;
        }
    }
    return 0;
}

SDL_AudioStream* zs3_OpenAudioDeviceStream(SDL_AudioDeviceID const dev, zs3_AudioSpec const* const spec,
    SDL_AudioStreamCallback const cb, void* const userdata)
{
    struct SDL_AudioStream* s;
    SDL_AudioSpec want;

    if (!spec || spec->format != SDL_AUDIO_S16 || the_stream)
        return NULL;
    if (!(s = (struct SDL_AudioStream*)calloc(1, sizeof *s)))
        return NULL;
    memset(&want, 0, sizeof want);
    want.freq = spec->freq;
    want.format = AUDIO_S16SYS; /* host order, as SDL3's SDL_AUDIO_S16 */
    want.channels = (Uint8)spec->channels;
    want.samples = 1024;
    want.callback = audio_callback;
    want.userdata = s;
    /* No obtained spec: SDL 1.2 then converts to the device itself, so the
       format asked for is the format the callback fills. */
    if (SDL_OpenAudio(&want, NULL) != 0 && !open_any_driver(&want)) {
        free(s);
        return NULL;
    }
    s->freq = spec->freq;
    s->channels = spec->channels;
    s->samples = want.samples;
    the_stream = s;
    return s;
}

void zs3_DestroyAudioStream(SDL_AudioStream* const s)
{
    if (s && s == the_stream) {
        SDL_CloseAudio();
        the_stream = NULL;
        free(s);
    }
}

bool zs3_ResumeAudioStreamDevice(SDL_AudioStream* const s)
{
    SDL_PauseAudio(0);
    return s != NULL;
}

bool zs3_PutAudioStreamData(SDL_AudioStream* const s, void const* const buf, int const len)
{
    Uint8 const* const in = (Uint8 const*)buf;
    unsigned room, n, i;

    if (!s || len < 0)
        return false;
    SDL_LockAudio();
    room = RING_BYTES - (s->head - s->tail);
    n = (unsigned)len < room ? (unsigned)len : room; /* a full ring drops the rest */
    for (i = 0; i < n; i++)
        s->ring[(s->head + i) % RING_BYTES] = in[i];
    s->head += n;
    SDL_UnlockAudio();
    return true;
}

int zs3_GetAudioStreamQueued(SDL_AudioStream* const s)
{
    int n;

    if (!s)
        return -1;
    SDL_LockAudio();
    n = (int)(s->head - s->tail);
    SDL_UnlockAudio();
    return n;
}

/* No resampler here; the frontend's drift control asks for ratios a fraction
   of a percent off 1, which the ring absorbs. */
bool zs3_SetAudioStreamFrequencyRatio(SDL_AudioStream* const s, float const ratio) { return s != NULL; }

SDL_AudioDeviceID zs3_GetAudioStreamDevice(SDL_AudioStream* const s) { return s ? 1 : 0; }

bool zs3_GetAudioDeviceFormat(SDL_AudioDeviceID const dev, zs3_AudioSpec* const spec, int* const frames)
{
    if (!the_stream)
        return false;
    if (spec) {
        spec->format = SDL_AUDIO_S16;
        spec->channels = the_stream->channels;
        spec->freq = the_stream->freq;
    }
    if (frames)
        *frames = the_stream->samples;
    return true;
}

char const* zs3_GetCurrentAudioDriver(void)
{
    static char name[32];

    return SDL_AudioDriverName(name, (int)sizeof name) ? name : NULL;
}

/* SDL 1.2 cannot list its drivers; SDL_AUDIODRIVER still picks one. */
int zs3_GetNumAudioDrivers(void) { return 0; }
char const* zs3_GetAudioDriver(int const index) { return NULL; }

/* The accelerated renderer (unix/sdl_render.c) is not built with SDL 1.2;
   this is the one thing the frontend asks of it outside the OpenGL paths. */
int sr_vsync_on(void);
int sr_vsync_on(void) { return 0; }
