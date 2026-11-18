/* The part of SDL3 the ZSNES frontend uses (see unix/sdl12compat.h, whose
   declarations this file implements), written on the classic Mac OS Toolbox
   for PowerPC Macs running Mac OS 8.6 to 9.2.2.

   - Video: one window (or a screen-covering one for full screen) and a
     16-bit offscreen GWorld. The frontend draws RGB565; QuickDraw's 16-bit
     pixels are xRGB1555, so each frame is converted on the way into the
     GWorld, and CopyBits takes it to the screen at whatever depth that is.
   - Keyboard: GetKeys, polled, for presses and releases of every key
     (modifiers included, which the event queue does not report); key-down
     events for typed characters and for Command-key shortcuts.
   - Sound: a Sound Manager channel fed 16-bit stereo through two buffers that
     its callBackCmd refills, at interrupt time, from a ring the emulator
     writes.
   - No threads: the frontend's frame-timer thread is not started and its
     waits become timed spins, which is how it already runs without one.
   - No joysticks yet (InputSprocket would be the way). */
#include <AppleEvents.h>
#include <Devices.h>
#include <Dialogs.h>
#include <Events.h>
#include <Fonts.h>
#include <Gestalt.h>
#include <LowMem.h>
#include <MacMemory.h>
#include <Windows.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <Sound.h>
#include <TextEdit.h>
#include <ToolUtils.h>

#include <stdio.h>
#include <string.h>

#include "../unix/sdl12compat.h"
#include "macextra.h"
#include "macos9.h"

/* --- the Toolbox ---------------------------------------------------------- */

static int toolbox_ready;
static int quit_requested;
static MenuHandle apple_menu, file_menu;

enum { MENU_APPLE = 128, MENU_FILE = 129 };

static pascal OSErr ae_quit(AppleEvent const* const e, AppleEvent* const reply, long const refcon)
{
    (void)e;
    (void)reply;
    (void)refcon;
    quit_requested = 1;
    return noErr;
}

/* The Finder hands over the files dropped on the application, or
   double-clicked with it, as an Open Documents event; the first is the ROM.
   Kept as a Unix-style path for the emulator's command line. */
static char opened_path[1024];

static int spec_to_path(FSSpec const* const spec, char* const out, size_t const size);

static pascal OSErr ae_open(AppleEvent const* const e, AppleEvent* const reply, long const refcon)
{
    AEDescList docs;
    long n;

    (void)reply;
    (void)refcon;
    if (AEGetParamDesc(e, keyDirectObject, typeAEList, &docs) != noErr)
        return noErr;
    if (AECountItems(&docs, &n) == noErr && n >= 1) {
        FSSpec spec;
        AEKeyword kw;
        DescType type;
        Size got;

        if (AEGetNthPtr(&docs, 1, typeFSS, &kw, &type, &spec, sizeof spec, &got) == noErr)
            spec_to_path(&spec, opened_path, sizeof opened_path);
    }
    AEDisposeDesc(&docs);
    return noErr;
}

static pascal OSErr ae_ignore(AppleEvent const* const e, AppleEvent* const reply, long const refcon)
{
    (void)e;
    (void)reply;
    (void)refcon;
    return noErr;
}

static void pstr(Str255 out, char const* const s)
{
    size_t const n = strlen(s) > 255 ? 255 : strlen(s);

    out[0] = (unsigned char)n;
    memcpy(out + 1, s, n);
}

void macos9_init(void)
{
    Str255 s;

    if (toolbox_ready)
        return;
    toolbox_ready = 1;
    MaxApplZone();
    MoreMasters();
    MoreMasters();
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);

    AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, NewAEEventHandlerUPP(ae_quit), 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, NewAEEventHandlerUPP(ae_open), 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEOpenApplication, NewAEEventHandlerUPP(ae_ignore), 0, false);
    AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments, NewAEEventHandlerUPP(ae_ignore), 0, false);

    pstr(s, "\x14");
    apple_menu = NewMenu(MENU_APPLE, s);
    pstr(s, "About ZSNES\xc9;(-");
    AppendMenu(apple_menu, s);
    AppendResMenu(apple_menu, 'DRVR');
    InsertMenu(apple_menu, 0);
    pstr(s, "File");
    file_menu = NewMenu(MENU_FILE, s);
    pstr(s, "Quit/Q");
    AppendMenu(file_menu, s);
    InsertMenu(file_menu, 0);
    DrawMenuBar();
}

static void about_box(void)
{
    Str255 a, b;

    pstr(a, "ZSNES 2 for Mac OS 8 and 9 (PowerPC)");
    pstr(b, "GPL 2. The emulator's own menu is on Escape; Command-Q quits.");
    ParamText(a, b, (ConstStr255Param) "\0", (ConstStr255Param) "\0");
    /* No ALRT resource of our own: the standard note alert with the text. */
    {
        AlertStdAlertParamRec p;
        short hit;

        memset(&p, 0, sizeof p);
        p.movable = true;
        p.defaultText = (ConstStringPtr)-1L; /* "OK" */
        p.defaultButton = kAlertStdAlertOKButton;
        p.position = kWindowDefaultPosition;
        StandardAlert(kAlertNoteAlert, a, b, &p, &hit);
    }
}

/* --- time ----------------------------------------------------------------- */

static uint64_t t0;

Uint64 zs3_GetTicks(void)
{
    if (!t0)
        t0 = macos9_microseconds();
    return (macos9_microseconds() - t0) / 1000u;
}

/* --- the event queue the frontend reads ----------------------------------- */

#define QLEN 128
static SDL_Event queue[QLEN];
static unsigned qhead, qtail;

static void push(SDL_Event const* const e)
{
    if (qhead - qtail < QLEN)
        queue[qhead++ % QLEN] = *e;
}

static void push_type(Uint32 const type)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type = type;
    push(&e);
}

/* --- init ----------------------------------------------------------------- */

static Uint32 inited;

bool zs3_Init(Uint32 const flags)
{
    macos9_init();
    inited |= flags;
    return true;
}

bool zs3_InitSubSystem(Uint32 const flags) { return zs3_Init(flags); }
void zs3_QuitSubSystem(Uint32 const flags) { inited &= ~flags; }
Uint32 zs3_WasInit(Uint32 const flags) { return inited & flags; }
void zs3_Quit(void) { macos9_shutdown(); }

static char error_text[128];
char const* zs3_GetError(void) { return error_text; }

static void set_error(char const* const s) { snprintf(error_text, sizeof error_text, "%s", s); }

bool zs3_SetHint(char const* const name, char const* const value) { return true; }
char const* zs3_GetHint(char const* const name) { return NULL; }
void zs3_free(void* const p) { free(p); }

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

char const* zs3_GetDisplayName(SDL_DisplayID const id) { return id == 1 ? "Main screen" : NULL; }
SDL_PropertiesID zs3_GetDisplayProperties(SDL_DisplayID const id) { return 0; }
bool zs3_GetBooleanProperty(SDL_PropertiesID const p, char const* const n, bool const def) { return def; }
bool zs3_DisableScreenSaver(void) { return true; }

SDL_DisplayMode const* zs3_GetCurrentDisplayMode(SDL_DisplayID const id)
{
    static SDL_DisplayMode m;
    Rect const r = (**GetMainDevice()).gdRect;

    m.displayID = 1;
    m.w = r.right - r.left;
    m.h = r.bottom - r.top;
    m.pixel_density = 1.0f;
    m.refresh_rate = 0.0f; /* unknown: the frontend keeps its own timer */
    return &m;
}

SDL_DisplayID zs3_GetDisplayForWindow(SDL_Window* const w) { return 1; }

/* --- surfaces: RGB565 in ordinary memory ---------------------------------- */

bool zs3_MustLock(SDL_Surface const* const s) { return false; }

SDL_Surface* zs3_CreateSurface(int const w, int const h, SDL_PixelFormat const format)
{
    SDL_Surface* s;

    if (format != SDL_PIXELFORMAT_RGB565 || w <= 0 || h <= 0)
        return NULL;
    if (!(s = (SDL_Surface*)calloc(1, sizeof *s)))
        return NULL;
    s->w = w;
    s->h = h;
    s->pitch = (w * 2 + 15) & ~15;
    s->format = format;
    if (!(s->pixels = calloc((size_t)s->pitch, (size_t)h))) {
        set_error("Out of memory for a surface: give ZSNES more memory in Get Info");
        free(s);
        return NULL;
    }
    return s;
}

void zs3_DestroySurface(SDL_Surface* const s)
{
    if (s) {
        free(s->pixels);
        free(s);
    }
}

bool zs3_LockSurface(SDL_Surface* const s) { return true; }
void zs3_UnlockSurface(SDL_Surface* const s) { }

static void clip(SDL_Surface const* const src, SDL_Rect const* const sr, SDL_Surface const* const dst,
    SDL_Rect const* const dr, SDL_Rect* const a, SDL_Rect* const b)
{
    a->x = sr ? sr->x : 0;
    a->y = sr ? sr->y : 0;
    a->w = sr ? sr->w : src->w;
    a->h = sr ? sr->h : src->h;
    b->x = dr ? dr->x : 0;
    b->y = dr ? dr->y : 0;
    b->w = dr ? dr->w : dst->w;
    b->h = dr ? dr->h : dst->h;
}

bool zs3_BlitSurface(SDL_Surface* const src, SDL_Rect const* const sr, SDL_Surface* const dst, SDL_Rect const* const dr)
{
    SDL_Rect a, b;
    int y, w, h;

    clip(src, sr, dst, dr, &a, &b);
    w = a.w;
    h = a.h;
    if (b.x + w > dst->w)
        w = dst->w - b.x;
    if (b.y + h > dst->h)
        h = dst->h - b.y;
    if (a.x + w > src->w)
        w = src->w - a.x;
    if (a.y + h > src->h)
        h = src->h - a.y;
    for (y = 0; y < h; y++)
        memcpy((Uint8*)dst->pixels + (size_t)(b.y + y) * dst->pitch + b.x * 2,
            (Uint8 const*)src->pixels + (size_t)(a.y + y) * src->pitch + a.x * 2, (size_t)w * 2);
    return true;
}

/* Nearest neighbour, whatever the mode: a G3 has better things to do. */
bool zs3_BlitSurfaceScaled(SDL_Surface* const src, SDL_Rect const* const sr, SDL_Surface* const dst,
    SDL_Rect const* const dr, SDL_ScaleMode const mode)
{
    SDL_Rect a, b;
    int x, y;

    (void)mode;
    clip(src, sr, dst, dr, &a, &b);
    if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0)
        return false;
    for (y = 0; y < b.h && b.y + y < dst->h; y++) {
        Uint16 const* const in = (Uint16 const*)((Uint8 const*)src->pixels + (size_t)(a.y + y * a.h / b.h) * src->pitch) + a.x;
        Uint16* const out = (Uint16*)((Uint8*)dst->pixels + (size_t)(b.y + y) * dst->pitch) + b.x;
        int const step = (a.w << 16) / b.w;
        int pos = 0;

        for (x = 0; x < b.w && b.x + x < dst->w; x++, pos += step)
            out[x] = in[pos >> 16];
    }
    return true;
}

/* A 24-bit BMP, bottom row first, little-endian headers. */
bool zs3_SaveBMP(SDL_Surface* const s, char const* const file)
{
    FILE* const f = fopen(file, "wb");
    int const row = (s->w * 3 + 3) & ~3;
    unsigned char h[54];
    unsigned char* line;
    int x, y;
    unsigned long const size = 54ul + (unsigned long)row * s->h;

#define LE32(p, v) ((p)[0] = (unsigned char)(v), (p)[1] = (unsigned char)((v) >> 8), (p)[2] = (unsigned char)((v) >> 16), (p)[3] = (unsigned char)((v) >> 24))
    if (!f)
        return false;
    if (!(line = (unsigned char*)calloc(1, (size_t)row))) {
        fclose(f);
        return false;
    }
    memset(h, 0, sizeof h);
    h[0] = 'B';
    h[1] = 'M';
    LE32(h + 2, size);
    LE32(h + 10, 54u);
    LE32(h + 14, 40u);
    LE32(h + 18, (unsigned long)s->w);
    LE32(h + 22, (unsigned long)s->h);
    h[26] = 1;
    h[28] = 24;
    fwrite(h, 1, sizeof h, f);
    for (y = s->h - 1; y >= 0; y--) {
        Uint16 const* const in = (Uint16 const*)((Uint8 const*)s->pixels + (size_t)y * s->pitch);
        for (x = 0; x < s->w; x++) {
            Uint16 const p = in[x];
            line[x * 3 + 0] = (unsigned char)((p & 0x1F) << 3 | (p & 0x1F) >> 2);
            line[x * 3 + 1] = (unsigned char)((p >> 5 & 0x3F) << 2 | (p >> 5 & 0x3F) >> 4);
            line[x * 3 + 2] = (unsigned char)((p >> 11) << 3 | (p >> 11) >> 2);
        }
        fwrite(line, 1, (size_t)row, f);
    }
#undef LE32
    free(line);
    return fclose(f) == 0;
}

static int focused = 1;

/* --- the window ----------------------------------------------------------- */

struct SDL_Window {
    WindowPtr win;
    GWorldPtr gw;
    int w, h;
    int fullscreen;
    SDL_Surface surface; /* RGB565, what the frontend draws into */
};

static struct SDL_Window the_window;
static int window_open;
static int menubar_hidden;

/* The classic way to take the whole screen: a menu bar zero pixels high,
   and its strip added to the desktop region so a window may draw there. */
static short saved_mbar_height;
static RgnHandle saved_gray;

static void hide_menubar(int const hide)
{
    RgnHandle const gray = LMGetGrayRgn();

    if (hide == menubar_hidden)
        return;
    menubar_hidden = hide;
    if (hide) {
        Rect r = (**GetMainDevice()).gdRect;
        RgnHandle const strip = NewRgn();

        saved_mbar_height = GetMBarHeight();
        saved_gray = NewRgn();
        CopyRgn(gray, saved_gray);
        r.bottom = (short)(r.top + saved_mbar_height);
        RectRgn(strip, &r);
        UnionRgn(gray, strip, gray);
        DisposeRgn(strip);
        LMSetMBarHeight(0);
    } else {
        LMSetMBarHeight(saved_mbar_height);
        if (saved_gray) {
            CopyRgn(saved_gray, gray);
            DisposeRgn(saved_gray);
            saved_gray = NULL;
        }
        DrawMenuBar();
    }
}

static void free_window(struct SDL_Window* const w)
{
    if (w->win) {
        DisposeWindow(w->win);
        w->win = NULL;
    }
    if (w->gw) {
        DisposeGWorld(w->gw);
        w->gw = NULL;
    }
    free(w->surface.pixels);
    w->surface.pixels = NULL;
}

static bool make_window(struct SDL_Window* const w)
{
    Rect const screen = (**GetMainDevice()).gdRect;
    Rect r, g;
    Str255 title;
    QDErr err;

    free_window(w);
    w->surface.w = w->w;
    w->surface.h = w->h;
    w->surface.pitch = (w->w * 2 + 15) & ~15;
    w->surface.format = SDL_PIXELFORMAT_RGB565;
    if (!(w->surface.pixels = calloc((size_t)w->surface.pitch, (size_t)w->h))) {
        set_error("Out of memory for the screen: give ZSNES more memory in Get Info");
        return false;
    }
    SetRect(&g, 0, 0, (short)w->w, (short)w->h);
    err = NewGWorld(&w->gw, 16, &g, NULL, NULL, 0);
    if (err != noErr || !w->gw)
        err = NewGWorld(&w->gw, 16, &g, NULL, NULL, useTempMem);
    if (err != noErr || !w->gw) {
        w->gw = NULL;
        set_error("Could not make an offscreen GWorld");
        return false;
    }
    pstr(title, "ZSNES");
    if (w->fullscreen) {
        hide_menubar(1);
        r = screen;
        w->win = NewCWindow(NULL, &r, title, true, plainDBox, (WindowPtr)-1L, false, 0);
    } else {
        short const mbar = GetMBarHeight();
        short const left = (short)(screen.left + ((screen.right - screen.left) - w->w) / 2);
        short top = (short)(screen.top + mbar + 20 + ((screen.bottom - screen.top - mbar - 20) - w->h) / 2);

        hide_menubar(0);
        if (top < screen.top + mbar + 20)
            top = (short)(screen.top + mbar + 20);
        SetRect(&r, left, top, (short)(left + w->w), (short)(top + w->h));
        w->win = NewCWindow(NULL, &r, title, true, noGrowDocProc, (WindowPtr)-1L, true, 0);
    }
    if (!w->win) {
        set_error("Could not open a window");
        return false;
    }
    SetPort((GrafPtr)w->win);
    {
        RGBColor black = { 0, 0, 0 };
        RGBBackColor(&black);
        EraseRect(&((GrafPtr)w->win)->portRect);
    }
    return true;
}

SDL_Window* zs3_CreateWindow(char const* const title, int const w, int const h, SDL_WindowFlags const flags)
{
    struct SDL_Window* const win = &the_window;

    (void)title;
    macos9_init();
    win->w = w;
    win->h = h;
    win->fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if (!make_window(win)) {
        free_window(win);
        return NULL;
    }
    window_open = 1;
    return win;
}

void zs3_DestroyWindow(SDL_Window* const win)
{
    if (win && window_open) {
        free_window(win);
        hide_menubar(0);
        window_open = 0;
    }
}

bool zs3_SetWindowSize(SDL_Window* const win, int const w, int const h)
{
    if (!win)
        return false;
    if (win->w == w && win->h == h && win->win)
        return true;
    win->w = w;
    win->h = h;
    return make_window(win);
}

bool zs3_SetWindowFullscreen(SDL_Window* const win, bool const fs)
{
    if (!win)
        return false;
    if (win->fullscreen == (int)fs && win->win)
        return true;
    win->fullscreen = fs;
    return make_window(win);
}

bool zs3_SetWindowPosition(SDL_Window* const win, int const x, int const y) { return true; }
bool zs3_SetWindowMouseGrab(SDL_Window* const win, bool const grab) { return true; }
bool zs3_SyncWindow(SDL_Window* const win) { return true; }

SDL_WindowFlags zs3_GetWindowFlags(SDL_Window* const win)
{
    return win && win->fullscreen ? SDL_WINDOW_FULLSCREEN : 0;
}

bool zs3_GetWindowSizeInPixels(SDL_Window* const win, int* const w, int* const h)
{
    if (w)
        *w = win ? win->w : 0;
    if (h)
        *h = win ? win->h : 0;
    return win != NULL;
}

SDL_Surface* zs3_GetWindowSurface(SDL_Window* const win)
{
    return win && win->surface.pixels ? &win->surface : NULL;
}

/* The GWorld to the window: centred in a full-screen one. */
static void present(struct SDL_Window* const win)
{
    PixMapHandle const pm = GetGWorldPixMap(win->gw);
    GrafPtr const port = (GrafPtr)win->win;
    Rect src, dst;
    RGBColor const black = { 0, 0, 0 }, white = { 0xFFFF, 0xFFFF, 0xFFFF };
    short const pw = (short)(port->portRect.right - port->portRect.left);
    short const ph = (short)(port->portRect.bottom - port->portRect.top);

    SetRect(&src, 0, 0, (short)win->w, (short)win->h);
    dst = src;
    if (win->fullscreen)
        OffsetRect(&dst, (short)((pw - win->w) / 2), (short)((ph - win->h) / 2));
    SetPort(port);
    RGBForeColor(&black);
    RGBBackColor(&white);
    if (LockPixels(pm)) {
        CopyBits((BitMap*)*pm, &port->portBits, &src, &dst, srcCopy, NULL);
        UnlockPixels(pm);
    }
    RGBBackColor(&black);
}

bool zs3_UpdateWindowSurface(SDL_Window* const win)
{
    PixMapHandle pm;
    Uint8* base;
    long rb;
    int x, y;

    if (!win || !win->win || !win->gw)
        return false;
    pm = GetGWorldPixMap(win->gw);
    if (!LockPixels(pm))
        return false;
    base = (Uint8*)GetPixBaseAddr(pm);
    rb = (**pm).rowBytes & 0x3FFF;
    /* RGB565 to xRGB1555, two pixels at a time. */
    for (y = 0; y < win->h; y++) {
        Uint32 const* const in = (Uint32 const*)((Uint8 const*)win->surface.pixels + (size_t)y * win->surface.pitch);
        Uint32* const out = (Uint32*)(base + (size_t)y * rb);
        int const n = win->w >> 1;

        for (x = 0; x < n; x++) {
            Uint32 const v = in[x];
            out[x] = ((v >> 1) & 0x7FE07FE0u) | (v & 0x001F001Fu);
        }
        if (win->w & 1) {
            Uint16 const v = ((Uint16 const*)in)[win->w - 1];
            ((Uint16*)out)[win->w - 1] = (Uint16)(((v >> 1) & 0x7FE0u) | (v & 0x1Fu));
        }
    }
    UnlockPixels(pm);
    present(win);
    return true;
}

bool zs3_StartTextInput(SDL_Window* const win) { return true; }

/* SDL hides the cursor over its window only; so does this, or the menu bar
   and the other applications' windows would have no pointer either. */
static int cursor_hidden, cursor_wanted_hidden;

bool zs3_HideCursor(void)
{
    cursor_wanted_hidden = 1;
    return true;
}

static void show_cursor(void)
{
    if (cursor_hidden) {
        ShowCursor();
        cursor_hidden = 0;
    }
}

static void update_cursor(Point const local, int const inside)
{
    (void)local;
    if (cursor_wanted_hidden && inside && focused) {
        if (!cursor_hidden) {
            HideCursor();
            cursor_hidden = 1;
        }
    } else {
        show_cursor();
    }
}

/* --- mouse ---------------------------------------------------------------- */

static Point last_global;
static int mouse_known;
static Uint32 mouse_buttons;

static Point global_mouse(void)
{
    Point p;
    GrafPtr saved;

    GetPort(&saved);
    SetPort(the_window.win ? (GrafPtr)the_window.win : saved);
    GetMouse(&p);
    LocalToGlobal(&p);
    SetPort(saved);
    return p;
}

Uint32 zs3_GetRelativeMouseState(float* const x, float* const y)
{
    Point const p = global_mouse();

    if (!mouse_known) {
        last_global = p;
        mouse_known = 1;
    }
    if (x)
        *x = (float)(p.h - last_global.h);
    if (y)
        *y = (float)(p.v - last_global.v);
    last_global = p;
    return mouse_buttons;
}

/* --- keyboard ------------------------------------------------------------- */

/* Mac virtual key codes (the ADB and USB keyboards' common set) to SDL3
   scancodes, and the character each types for the SDL3 keycode. */
static struct {
    unsigned char sc;
    unsigned char ch;
} const keymap[128] = {
    [0x00] = { SDL_SCANCODE_A, 'a' },
    [0x01] = { SDL_SCANCODE_S, 's' },
    [0x02] = { SDL_SCANCODE_D, 'd' },
    [0x03] = { SDL_SCANCODE_F, 'f' },
    [0x04] = { SDL_SCANCODE_H, 'h' },
    [0x05] = { SDL_SCANCODE_G, 'g' },
    [0x06] = { SDL_SCANCODE_Z, 'z' },
    [0x07] = { SDL_SCANCODE_X, 'x' },
    [0x08] = { SDL_SCANCODE_C, 'c' },
    [0x09] = { SDL_SCANCODE_V, 'v' },
    [0x0A] = { SDL_SCANCODE_NONUSBACKSLASH, '<' },
    [0x0B] = { SDL_SCANCODE_B, 'b' },
    [0x0C] = { SDL_SCANCODE_Q, 'q' },
    [0x0D] = { SDL_SCANCODE_W, 'w' },
    [0x0E] = { SDL_SCANCODE_E, 'e' },
    [0x0F] = { SDL_SCANCODE_R, 'r' },
    [0x10] = { SDL_SCANCODE_Y, 'y' },
    [0x11] = { SDL_SCANCODE_T, 't' },
    [0x12] = { SDL_SCANCODE_1, '1' },
    [0x13] = { SDL_SCANCODE_2, '2' },
    [0x14] = { SDL_SCANCODE_3, '3' },
    [0x15] = { SDL_SCANCODE_4, '4' },
    [0x16] = { SDL_SCANCODE_6, '6' },
    [0x17] = { SDL_SCANCODE_5, '5' },
    [0x18] = { SDL_SCANCODE_EQUALS, '=' },
    [0x19] = { SDL_SCANCODE_9, '9' },
    [0x1A] = { SDL_SCANCODE_7, '7' },
    [0x1B] = { SDL_SCANCODE_MINUS, '-' },
    [0x1C] = { SDL_SCANCODE_8, '8' },
    [0x1D] = { SDL_SCANCODE_0, '0' },
    [0x1E] = { SDL_SCANCODE_RIGHTBRACKET, ']' },
    [0x1F] = { SDL_SCANCODE_O, 'o' },
    [0x20] = { SDL_SCANCODE_U, 'u' },
    [0x21] = { SDL_SCANCODE_LEFTBRACKET, '[' },
    [0x22] = { SDL_SCANCODE_I, 'i' },
    [0x23] = { SDL_SCANCODE_P, 'p' },
    [0x24] = { SDL_SCANCODE_RETURN, '\r' },
    [0x25] = { SDL_SCANCODE_L, 'l' },
    [0x26] = { SDL_SCANCODE_J, 'j' },
    [0x27] = { SDL_SCANCODE_APOSTROPHE, '\'' },
    [0x28] = { SDL_SCANCODE_K, 'k' },
    [0x29] = { SDL_SCANCODE_SEMICOLON, ';' },
    [0x2A] = { SDL_SCANCODE_BACKSLASH, '\\' },
    [0x2B] = { SDL_SCANCODE_COMMA, ',' },
    [0x2C] = { SDL_SCANCODE_SLASH, '/' },
    [0x2D] = { SDL_SCANCODE_N, 'n' },
    [0x2E] = { SDL_SCANCODE_M, 'm' },
    [0x2F] = { SDL_SCANCODE_PERIOD, '.' },
    [0x30] = { SDL_SCANCODE_TAB, '\t' },
    [0x31] = { SDL_SCANCODE_SPACE, ' ' },
    [0x32] = { SDL_SCANCODE_GRAVE, '`' },
    [0x33] = { SDL_SCANCODE_BACKSPACE, '\b' },
    [0x34] = { SDL_SCANCODE_KP_ENTER, 0 },
    [0x35] = { SDL_SCANCODE_ESCAPE, 0x1B },
    [0x37] = { SDL_SCANCODE_LGUI, 0 }, /* Command */
    [0x38] = { SDL_SCANCODE_LSHIFT, 0 },
    [0x39] = { SDL_SCANCODE_CAPSLOCK, 0 },
    [0x3A] = { SDL_SCANCODE_LALT, 0 }, /* Option */
    [0x3B] = { SDL_SCANCODE_LCTRL, 0 },
    [0x3C] = { SDL_SCANCODE_RSHIFT, 0 },
    [0x3D] = { SDL_SCANCODE_RALT, 0 },
    [0x3E] = { SDL_SCANCODE_RCTRL, 0 },
    [0x41] = { SDL_SCANCODE_KP_PERIOD, 0 },
    [0x43] = { SDL_SCANCODE_KP_MULTIPLY, 0 },
    [0x45] = { SDL_SCANCODE_KP_PLUS, 0 },
    [0x47] = { SDL_SCANCODE_NUMLOCKCLEAR, 0 }, /* Clear */
    [0x4B] = { SDL_SCANCODE_KP_DIVIDE, 0 },
    [0x4C] = { SDL_SCANCODE_KP_ENTER, 0 },
    [0x4E] = { SDL_SCANCODE_KP_MINUS, 0 },
    [0x51] = { SDL_SCANCODE_KP_EQUALS, 0 },
    [0x52] = { SDL_SCANCODE_KP_0, 0 },
    [0x53] = { SDL_SCANCODE_KP_1, 0 },
    [0x54] = { SDL_SCANCODE_KP_2, 0 },
    [0x55] = { SDL_SCANCODE_KP_3, 0 },
    [0x56] = { SDL_SCANCODE_KP_4, 0 },
    [0x57] = { SDL_SCANCODE_KP_5, 0 },
    [0x58] = { SDL_SCANCODE_KP_6, 0 },
    [0x59] = { SDL_SCANCODE_KP_7, 0 },
    [0x5B] = { SDL_SCANCODE_KP_8, 0 },
    [0x5C] = { SDL_SCANCODE_KP_9, 0 },
    [0x60] = { SDL_SCANCODE_F5, 0 },
    [0x61] = { SDL_SCANCODE_F6, 0 },
    [0x62] = { SDL_SCANCODE_F7, 0 },
    [0x63] = { SDL_SCANCODE_F3, 0 },
    [0x64] = { SDL_SCANCODE_F8, 0 },
    [0x65] = { SDL_SCANCODE_F9, 0 },
    [0x67] = { SDL_SCANCODE_F11, 0 },
    [0x69] = { SDL_SCANCODE_PRINTSCREEN, 0 }, /* F13 */
    [0x6B] = { SDL_SCANCODE_SCROLLLOCK, 0 },  /* F14 */
    [0x6D] = { SDL_SCANCODE_F10, 0 },
    [0x6F] = { SDL_SCANCODE_F12, 0 },
    [0x71] = { SDL_SCANCODE_PAUSE, 0 }, /* F15 */
    [0x72] = { SDL_SCANCODE_INSERT, 0 }, /* Help */
    [0x73] = { SDL_SCANCODE_HOME, 0 },
    [0x74] = { SDL_SCANCODE_PAGEUP, 0 },
    [0x75] = { SDL_SCANCODE_DELETE, 0x7F },
    [0x76] = { SDL_SCANCODE_F4, 0 },
    [0x77] = { SDL_SCANCODE_END, 0 },
    [0x78] = { SDL_SCANCODE_F2, 0 },
    [0x79] = { SDL_SCANCODE_PAGEDOWN, 0 },
    [0x7A] = { SDL_SCANCODE_F1, 0 },
    [0x7B] = { SDL_SCANCODE_LEFT, 0 },
    [0x7C] = { SDL_SCANCODE_RIGHT, 0 },
    [0x7D] = { SDL_SCANCODE_DOWN, 0 },
    [0x7E] = { SDL_SCANCODE_UP, 0 },
};

static unsigned char keys_down[16];

static int key_is_down(unsigned char const* const km, int const k) { return (km[k >> 3] >> (k & 7)) & 1; }

static SDL_Keymod current_mod(unsigned char const* const km)
{
    SDL_Keymod m = 0;

    if (key_is_down(km, 0x3A) || key_is_down(km, 0x3D))
        m |= SDL_KMOD_ALT;
    return m;
}

/* Every key that went down or up since the last look, as key events. */
static void poll_keys(void)
{
    uint8_t km[16];
    unsigned char now[16];
    int k;

    GetKeys(km);
    memcpy(now, km, sizeof now);
    if (!focused)
        memset(now, 0, sizeof now); /* release everything while in the background */
    for (k = 0; k < 128; k++) {
        int const was = key_is_down(keys_down, k), is = key_is_down(now, k);

        if (was != is && keymap[k].sc) {
            SDL_Event e;

            memset(&e, 0, sizeof e);
            e.type = is ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            e.key.scancode = (SDL_Scancode)keymap[k].sc;
            e.key.key = keymap[k].ch ? (SDL_Keycode)keymap[k].ch : ((SDL_Keycode)keymap[k].sc | 0x40000000u);
            e.key.mod = current_mod(now);
            e.key.down = is != 0;
            push(&e);
        }
    }
    memcpy(keys_down, now, sizeof now);
}

static void do_menu(long const choice)
{
    short const menu = (short)(choice >> 16), item = (short)(choice & 0xFFFF);

    if (menu == MENU_APPLE) {
        if (item == 1) {
            show_cursor();
            about_box();
        } else {
            Str255 name;
            GetMenuItemText(apple_menu, item, name);
            OpenDeskAcc(name);
        }
    } else if (menu == MENU_FILE && item == 1) {
        quit_requested = 1;
    }
    HiliteMenu(0);
}

static void handle(EventRecord* const ev)
{
    switch (ev->what) {
    case mouseDown: {
        WindowPtr w;
        short const part = FindWindow(ev->where, &w);

        switch (part) {
        case inMenuBar:
            show_cursor();
            do_menu(MenuSelect(ev->where));
            break;
        case inSysWindow:
            SystemClick(ev, w);
            break;
        case inDrag: {
            Rect bounds = qd.screenBits.bounds;
            DragWindow(w, ev->where, &bounds);
            break;
        }
        case inGoAway:
            if (TrackGoAway(w, ev->where))
                quit_requested = 1;
            break;
        case inContent:
            if (w != FrontWindow()) {
                SelectWindow(w);
            } else {
                SDL_Event e;
                Uint8 const b = (ev->modifiers & ControlKey) ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;

                memset(&e, 0, sizeof e);
                e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                e.button.button = b;
                mouse_buttons |= 1u << (b - 1);
                push(&e);
            }
            break;
        }
        break;
    }
    case mouseUp:
        if (mouse_buttons) {
            SDL_Event e;
            Uint8 const b = (mouse_buttons & (1u << (SDL_BUTTON_RIGHT - 1))) ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;

            memset(&e, 0, sizeof e);
            e.type = SDL_EVENT_MOUSE_BUTTON_UP;
            e.button.button = b;
            mouse_buttons = 0;
            push(&e);
        }
        break;
    case keyDown:
    case autoKey: {
        char const c = (char)(ev->message & charCodeMask);

        if (ev->modifiers & cmdKey) {
            if (ev->what == keyDown) {
                long const choice = MenuKey(c);
                if (choice >> 16)
                    do_menu(choice);
            }
        } else if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7F) {
            SDL_Event e;

            memset(&e, 0, sizeof e);
            e.type = SDL_EVENT_TEXT_INPUT;
            e.text.text[0] = c;
            push(&e);
        }
        break;
    }
    case updateEvt: {
        WindowPtr const w = (WindowPtr)ev->message;

        BeginUpdate(w);
        if (w == the_window.win && the_window.gw) {
            EraseRect(&((GrafPtr)w)->portRect);
            present(&the_window);
        }
        EndUpdate(w);
        break;
    }
    case activateEvt:
        break;
    case osEvt:
        if (((ev->message >> 24) & 0xFF) == suspendResumeMessage) {
            int const resume = (ev->message & resumeFlag) != 0;

            focused = resume;
            push_type(resume ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST);
            if (!resume) {
                show_cursor();
                if (the_window.fullscreen)
                    hide_menubar(0);
            } else if (the_window.fullscreen) {
                hide_menubar(1);
            }
        }
        break;
    case kHighLevelEvent:
        AEProcessAppleEvent(ev);
        break;
    }
}

void zs3_PumpEvents(void)
{
    EventRecord ev;
    int n;

    macos9_init();
    /* A handful per call: the frontend pumps every frame. Sleep 0 - the
       emulator wants the processor - but other applications still get their
       turn inside WaitNextEvent. */
    for (n = 0; n < 8 && WaitNextEvent(everyEvent, &ev, 0, NULL); n++)
        handle(&ev);
    poll_keys();

    if (the_window.win && focused) {
        Point p;
        static Point last;
        static int have_last;

        SetPort((GrafPtr)the_window.win);
        GetMouse(&p);
        update_cursor(p, PtInRect(p, &((GrafPtr)the_window.win)->portRect) && FrontWindow() == the_window.win);
        if (!have_last || p.h != last.h || p.v != last.v) {
            SDL_Event e;

            memset(&e, 0, sizeof e);
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.x = (float)p.h;
            e.motion.y = (float)p.v;
            e.motion.xrel = have_last ? (float)(p.h - last.h) : 0.f;
            e.motion.yrel = have_last ? (float)(p.v - last.v) : 0.f;
            push(&e);
            last = p;
            have_last = 1;
        }
    }
    if (quit_requested) {
        quit_requested = 0;
        push_type(SDL_EVENT_QUIT);
    }
}

bool zs3_PollEvent(SDL_Event* const e)
{
    if (qhead == qtail)
        zs3_PumpEvents();
    if (qhead == qtail)
        return false;
    if (e)
        *e = queue[qtail % QLEN];
    qtail++;
    return true;
}

void macos9_idle(void) { }

/* --- joysticks: none yet -------------------------------------------------- */

SDL_JoystickID* zs3_GetJoysticks(int* const count)
{
    if (count)
        *count = 0;
    return (SDL_JoystickID*)calloc(1, sizeof(SDL_JoystickID));
}

SDL_Joystick* zs3_OpenJoystick(SDL_JoystickID const id) { return NULL; }
void zs3_CloseJoystick(SDL_Joystick* const j) { }
char const* zs3_GetJoystickName(SDL_Joystick* const j) { return NULL; }
int zs3_GetNumJoystickAxes(SDL_Joystick* const j) { return -1; }
int zs3_GetNumJoystickButtons(SDL_Joystick* const j) { return -1; }
int zs3_GetNumJoystickHats(SDL_Joystick* const j) { return -1; }
void zs3_SetJoystickEventsEnabled(bool const on) { }
bool zs3_RumbleJoystick(SDL_Joystick* const j, Uint16 const lo, Uint16 const hi, Uint32 const ms) { return false; }
bool zs3_IsGamepad(SDL_JoystickID const id) { return false; }
SDL_Gamepad* zs3_OpenGamepad(SDL_JoystickID const id) { return NULL; }
void zs3_CloseGamepad(SDL_Gamepad* const g) { }
char const* zs3_GetGamepadName(SDL_Gamepad* const g) { return NULL; }
bool zs3_GamepadConnected(SDL_Gamepad* const g) { return false; }
bool zs3_GetGamepadButton(SDL_Gamepad* const g, SDL_GamepadButton const b) { return false; }
Sint16 zs3_GetGamepadAxis(SDL_Gamepad* const g, SDL_GamepadAxis const a) { return 0; }
void zs3_SetGamepadEventsEnabled(bool const on) { }
bool zs3_RumbleGamepad(SDL_Gamepad* const g, Uint16 const lo, Uint16 const hi, Uint32 const ms) { return false; }

/* --- threads: none; semaphores count, and a wait is a timed spin ---------- */

SDL_Thread* zs3_CreateThread(SDL_ThreadFunction const fn, char const* const name, void* const data) { return NULL; }
void zs3_WaitThread(SDL_Thread* const t, int* const status) { }

struct SDL_Semaphore {
    volatile Uint32 count;
};

SDL_Semaphore* zs3_CreateSemaphore(Uint32 const n)
{
    SDL_Semaphore* const s = (SDL_Semaphore*)calloc(1, sizeof *s);

    if (s)
        s->count = n;
    return s;
}

void zs3_DestroySemaphore(SDL_Semaphore* const s) { free(s); }
void zs3_SignalSemaphore(SDL_Semaphore* const s) { s->count++; }

bool zs3_WaitSemaphoreTimeout(SDL_Semaphore* const s, Sint32 const ms)
{
    uint64_t const end = macos9_microseconds() + (uint64_t)(ms < 0 ? 0 : ms) * 1000u;

    for (;;) {
        if (s->count) {
            s->count--;
            return true;
        }
        if (ms >= 0 && macos9_microseconds() >= end)
            return false;
    }
}

/* --- sound: the Sound Manager --------------------------------------------- */

#define RING_BYTES (1 << 16)
#define BUF_FRAMES 1024
#define NBUFS 2

struct SDL_AudioStream {
    Uint8 ring[RING_BYTES];
    volatile unsigned head, tail; /* the emulator writes head, the callback tail */
    int freq, channels;
    SndChannelPtr chan;
    SndCallbackUPP cb;
    ExtSoundHeader hdr[NBUFS];
    Sint16* data[NBUFS];
    volatile int stopping;
};

static struct SDL_AudioStream* the_stream;

/* Fill buffer i from the ring, silence for what is missing, and queue it to
   play followed by a callback that comes back here for the same buffer. */
static void queue_buffer(struct SDL_AudioStream* const s, int const i)
{
    unsigned const want = BUF_FRAMES * (unsigned)s->channels * 2u;
    unsigned const have = s->head - s->tail;
    unsigned const n = have < want ? have : want;
    Uint8* const out = (Uint8*)s->data[i];
    unsigned const t = s->tail % RING_BYTES;
    SndCommand c;

    if (t + n <= RING_BYTES) {
        BlockMoveData(s->ring + t, out, (long)n);
    } else {
        BlockMoveData(s->ring + t, out, (long)(RING_BYTES - t));
        BlockMoveData(s->ring, out + (RING_BYTES - t), (long)(n - (RING_BYTES - t)));
    }
    s->tail += n;
    if (n < want)
        memset(out + n, 0, want - n);

    c.cmd = bufferCmd;
    c.param1 = 0;
    c.param2 = (long)&s->hdr[i];
    SndDoCommand(s->chan, &c, true);
    c.cmd = callBackCmd;
    c.param1 = (short)i;
    c.param2 = 0;
    SndDoCommand(s->chan, &c, true);
}

/* At interrupt time: no memory allocation, no Toolbox calls but these. */
static pascal void sound_callback(SndChannelPtr const chan, SndCommand* const cmd)
{
    struct SDL_AudioStream* const s = the_stream;

    (void)chan;
    if (s && !s->stopping)
        queue_buffer(s, cmd->param1);
}

SDL_AudioStream* zs3_OpenAudioDeviceStream(SDL_AudioDeviceID const dev, SDL_AudioSpec const* const spec,
    SDL_AudioStreamCallback const cb, void* const userdata)
{
    struct SDL_AudioStream* s;
    int i;

    (void)dev;
    (void)cb;
    (void)userdata;
    if (!spec || spec->format != SDL_AUDIO_S16 || the_stream || spec->channels < 1 || spec->channels > 2)
        return NULL;
    if (!(s = (struct SDL_AudioStream*)NewPtrClear(sizeof *s))) {
        set_error("Out of memory for sound");
        return NULL;
    }
    s->freq = spec->freq;
    s->channels = spec->channels;
    for (i = 0; i < NBUFS; i++) {
        ExtSoundHeader* const h = &s->hdr[i];

        if (!(s->data[i] = (Sint16*)NewPtrClear(BUF_FRAMES * s->channels * 2))) {
            set_error("Out of memory for sound");
            goto fail;
        }
        h->samplePtr = (Ptr)s->data[i];
        h->numChannels = (unsigned long)s->channels;
        h->sampleRate = (Fixed)((unsigned long)s->freq << 16);
        h->encode = extSH;
        h->baseFrequency = kMiddleC;
        h->numFrames = BUF_FRAMES;
        h->sampleSize = 16;
    }
    s->cb = NewSndCallbackUPP(sound_callback);
    if (SndNewChannel(&s->chan, sampledSynth, s->channels == 2 ? initStereo : initMono, s->cb) != noErr) {
        set_error("The Sound Manager would not open a channel");
        s->chan = NULL;
        goto fail;
    }
    the_stream = s;
    return s;
fail:
    for (i = 0; i < NBUFS; i++)
        if (s->data[i])
            DisposePtr((Ptr)s->data[i]);
    DisposePtr((Ptr)s);
    return NULL;
}

static int sound_started;

bool zs3_ResumeAudioStreamDevice(SDL_AudioStream* const s)
{
    int i;

    if (!s)
        return false;
    if (!sound_started) {
        sound_started = 1;
        for (i = 0; i < NBUFS; i++)
            queue_buffer(s, i);
    }
    return true;
}

void zs3_DestroyAudioStream(SDL_AudioStream* const s)
{
    int i;

    if (!s || s != the_stream)
        return;
    s->stopping = 1;
    SndDisposeChannel(s->chan, true); /* quietNow: flush and stop */
    the_stream = NULL;
    sound_started = 0;
    for (i = 0; i < NBUFS; i++)
        DisposePtr((Ptr)s->data[i]);
    DisposeSndCallbackUPP(s->cb);
    DisposePtr((Ptr)s);
}

bool zs3_PutAudioStreamData(SDL_AudioStream* const s, void const* const buf, int const len)
{
    unsigned room, n, h, first;

    if (!s || len < 0)
        return false;
    room = RING_BYTES - (s->head - s->tail);
    n = (unsigned)len < room ? (unsigned)len : room;
    n &= ~3u; /* whole stereo frames */
    h = s->head % RING_BYTES;
    first = RING_BYTES - h < n ? RING_BYTES - h : n;
    memcpy(s->ring + h, buf, first);
    memcpy(s->ring, (Uint8 const*)buf + first, n - first);
    s->head += n; /* after the copy: the callback reads up to head */
    return true;
}

int zs3_GetAudioStreamQueued(SDL_AudioStream* const s) { return s ? (int)(s->head - s->tail) : -1; }
bool zs3_SetAudioStreamFrequencyRatio(SDL_AudioStream* const s, float const ratio) { return s != NULL; }
SDL_AudioDeviceID zs3_GetAudioStreamDevice(SDL_AudioStream* const s) { return s ? 1 : 0; }

bool zs3_GetAudioDeviceFormat(SDL_AudioDeviceID const dev, SDL_AudioSpec* const spec, int* const frames)
{
    if (!the_stream)
        return false;
    if (spec) {
        spec->format = SDL_AUDIO_S16;
        spec->channels = the_stream->channels;
        spec->freq = the_stream->freq;
    }
    if (frames)
        *frames = BUF_FRAMES;
    return true;
}

char const* zs3_GetCurrentAudioDriver(void) { return "Sound Manager"; }
int zs3_GetNumAudioDrivers(void) { return 0; }
char const* zs3_GetAudioDriver(int const index) { return NULL; }

/* No accelerated renderer on this platform. */
int sr_vsync_on(void);
int sr_vsync_on(void) { return 0; }

/* --- start and end -------------------------------------------------------- */

void macos9_shutdown(void)
{
    static int done;

    if (done)
        return;
    done = 1;
    if (the_stream)
        zs3_DestroyAudioStream(the_stream);
    if (window_open)
        zs3_DestroyWindow(&the_window);
    hide_menubar(0);
    show_cursor();
    macos9_close_log();
}

static int spec_to_path(FSSpec const* const spec, char* const out, size_t const size)
{
    /* The folder's path from posix.c, then the name. */
    char dir[1024];
    char name[64];
    unsigned i;
    CInfoPBRec pb;
    Str63 n;
    long id = spec->parID;
    char tmp[1024];
    size_t pos = sizeof tmp - 1;

    tmp[pos] = 0;
    for (;;) {
        size_t len;

        memset(&pb, 0, sizeof pb);
        pb.dirInfo.ioNamePtr = n;
        pb.dirInfo.ioVRefNum = spec->vRefNum;
        pb.dirInfo.ioDrDirID = id;
        pb.dirInfo.ioFDirIndex = -1;
        if (PBGetCatInfoSync(&pb) != noErr)
            return -1;
        for (i = 0; i < n[0]; i++)
            name[i] = n[1 + i] == '/' ? ':' : (char)n[1 + i];
        name[n[0]] = 0;
        len = strlen(name);
        if (pos < len + 2)
            return -1;
        tmp[--pos] = '/';
        pos -= len;
        memcpy(tmp + pos, name, len);
        if (id == fsRtDirID)
            break;
        id = pb.dirInfo.ioDrParID;
    }
    tmp[--pos] = '/';
    snprintf(dir, sizeof dir, "%s", tmp + pos);
    for (i = 0; i < spec->name[0]; i++)
        name[i] = spec->name[1 + i] == '/' ? ':' : (char)spec->name[1 + i];
    name[spec->name[0]] = 0;
    return snprintf(out, size, "%s%s", dir, name) < (int)size ? 0 : -1;
}

int zsnes_main(int argc, char* argv[]);

/* Retro68 calls main with no arguments worth the name. Start the Toolbox,
   take the Finder's Open Documents event - the ROM dropped on the
   application - and hand that over as the command line. */
int main(int argc, char* argv[])
{
    static char* args[3] = { "ZSNES", NULL, NULL };
    EventRecord ev;
    int i, r;

    (void)argc;
    (void)argv;
    macos9_init();
    /* The Finder sends Open Application or Open Documents first thing. */
    for (i = 0; i < 30 && !opened_path[0]; i++) {
        if (WaitNextEvent(highLevelEventMask, &ev, 2, NULL))
            handle(&ev);
    }
    if (opened_path[0])
        args[1] = opened_path;
    r = zsnes_main(opened_path[0] ? 2 : 1, args);
    macos9_shutdown();
    return r;
}
