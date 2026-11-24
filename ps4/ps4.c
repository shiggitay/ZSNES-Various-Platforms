/* ps4.c - the part of SDL3 the ZSNES frontend uses (unix/sdl12compat.h,
   whose declarations this file implements), on the PlayStation 4's own
   libraries through the OpenOrbis toolchain, for homebrew run with GoldHEN.

   - Video: three 1920x1080 frame buffers in direct memory, flipped on the
     TV's vertical blank (the system scales them to a 720p TV). Each frame is
     converted from the frontend's RGB565 and scaled, 4:3 and as tall as the
     screen, into a buffer the screen is not showing; a buffer is reused only
     once its flip has gone through, and every wait is bounded.
   - Controllers: the DualShock 4 of the first logged-in user is player 1 on
     the keys Config > Input gives player 1; the next three users' pads are
     players 2 to 4. Options+touch pad (Start+Select) or R3 opens the ZSNES
     menu; L3 the frame counter. In the menus pad 1's left stick moves the
     pointer, Cross picks and Circle goes back.
   - Sound: a 48 kHz audio port, fed by a thread of its own whose output
     call paces it; it plays silence whenever the emulator has nothing
     queued, so it never waits on the emulator. The DSP's 32000 Hz goes
     through unix/resample.c.
   - Files: settings, saves and the log in /data/zsnes; ROMs from there
     (/data/zsnes/roms) or a USB drive (/mnt/usb0).
   - No threads for the frontend: as on Mac OS 9 and the PS3, its frame
     timer runs without one. */
#include <sys/types.h> /* the BSD types the PS4's headers use, first */

#include <orbis/AudioOut.h>
#include <orbis/Pad.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "../unix/resample.h"
#include "../unix/sdl12compat.h"
#include "input.h" /* the players' keys */

typedef uint32_t u32;

/* A line in "ZSNES Log.txt" (see ps4_setup), written and flushed at once. */
static FILE* log_file;
#define LOG(...) \
    do { \
        if (log_file) { \
            fprintf(log_file, "ps4: " __VA_ARGS__); \
            fputc('\n', log_file); \
            fflush(log_file); \
        } \
    } while (0)

/* The emulator's key state (cpu/execute.h). */
#define PRESSED_SIZE (256 + 128 + 64)
extern unsigned char pressed[PRESSED_SIZE];
extern unsigned char GUIOn, GUIOn2;
extern unsigned char FPSOn, frameskip, vsyncon, BilinearFilter;

static int quit_requested;
static Uint32 inited;
static char error_text[128];

static void set_error(char const* const s) { snprintf(error_text, sizeof error_text, "%s", s); }
char const* zs3_GetError(void) { return error_text; }
bool zs3_SetHint(char const* const name, char const* const value) { return true; }
char const* zs3_GetHint(char const* const name) { return NULL; }
void zs3_free(void* const p) { free(p); }

static int in_menus(void) { return GUIOn || GUIOn2; }

static void checkpoint(char const* step); /* the last step reached, logged */

/* --- time ----------------------------------------------------------------- */

static uint64_t microseconds(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000u + (uint64_t)tv.tv_usec;
}

Uint64 zs3_GetTicks(void)
{
    static uint64_t t0;

    if (!t0)
        t0 = microseconds();
    return (microseconds() - t0) / 1000u;
}

/* --- the event queue the frontend reads ----------------------------------- */

#define QLEN 256
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

static SDL_Keymod key_mod; /* the USB keyboard's modifiers */

static void push_key(SDL_Scancode const sc, SDL_Keycode const key, int const down)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.scancode = sc;
    e.key.key = key;
    e.key.mod = key_mod;
    e.key.down = down != 0;
    push(&e);
}

/* --- the system ----------------------------------------------------------- */


static void audio_stop(void);
static void video_stop(void);
static void pads_init(void);

static int system_ready;

/* Quitting: back to the PS4's home screen, which a homebrew's exit() alone
   does not do. */
static void notify(char const* fmt, ...);

static void shutdown_all(void)
{
    LOG("quitting");
    notify("ZSNES has quit");
    audio_stop();
    video_stop();
    sceSystemServiceLoadExec("exit", NULL);
}

static void system_init(void)
{
    if (system_ready)
        return;
    system_ready = 1;
    checkpoint("pads");
    if (scePadInit() != 0)
        LOG("scePadInit failed");
    pads_init();
    atexit(shutdown_all);
}

/* Before the frontend reads its paths: everything in /data/zsnes, which the
   PS4 lets homebrew write and FTP reach. */
/* A notification in the corner of the screen: what the PS4 shows of a
   program before it has a picture, or when it has nowhere to write a log. */
static void notify(char const* const fmt, ...)
{
    OrbisNotificationRequest r;
    va_list ap;

    memset(&r, 0, sizeof r);
    va_start(ap, fmt);
    vsnprintf(r.message, sizeof r.message, fmt, ap);
    va_end(ap);
    r.type = NotificationRequest;
    r.unk3 = 0;
    r.useIconImageUri = 1;
    r.targetId = -1;
    snprintf(r.iconUri, sizeof r.iconUri, "cxml://psnotification/tex_icon_system");
    sceKernelSendNotificationRequest(0, &r, sizeof r, 0);
    LOG("notification: %s", r.message);
}

/* A folder this program can write to: one it makes, or has, and in which a
   file it writes can be read back. */
static int writable(char const* const dir)
{
    char path[512];
    FILE* f;

    mkdir(dir, 0777);
    snprintf(path, sizeof path, "%s/.zsnes-write-test", dir);
    if (!(f = fopen(path, "w")))
        return 0;
    fputs("ok", f);
    fclose(f);
    remove(path);
    return 1;
}

static char data_dir[256];

/* The last step reached, for the log when something crashes. */
static char const* volatile last_step = "start";

static void checkpoint(char const* const step)
{
    last_step = step;
    LOG("step: %s", step);
}

/* A crash: what and where, in the log, then the system's own handling (the
   error screen). fprintf is not safe in a signal handler in general; here
   the program is ending anyway, and the log is all there will be. */
static void on_crash(int const sig, siginfo_t* const info, void* const ctx)
{
    (void)ctx;
    LOG("CRASH: signal %d (code %d) at address %p, after step \"%s\"", sig, info ? info->si_code : 0,
        info ? info->si_addr : NULL, last_step);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void crash_handlers(void)
{
    static int const sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT };
    struct sigaction sa;
    unsigned i;

    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    for (i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
        sigaction(sigs[i], &sa, NULL);
}

/* Before the frontend reads its paths: settings, saves and the log in the
   first folder that takes them. /data reaches the whole system and FTP,
   but a program can only write there if it is let out of its sandbox; the
   app's own download data (the package asks for some) and its temporary
   folder are always its own. */
void ps4_setup(char const* const argv0)
{
    static char const* const dirs[] = { "/data/zsnes", "/download0/zsnes", "/download0", "/temp0/zsnes" };
    OrbisUserServiceInitializeParams param;
    char path[300];
    unsigned i;

    notify("ZSNES is starting");
    data_dir[0] = '\0';
    for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        if (writable(dirs[i])) {
            snprintf(data_dir, sizeof data_dir, "%s", dirs[i]);
            break;
        }
    }
    if (!data_dir[0]) {
        notify("ZSNES: no folder it can write to (tried /data, /download0, /temp0)");
        snprintf(data_dir, sizeof data_dir, "/app0");
    }
    snprintf(path, sizeof path, "%s/roms", data_dir);
    mkdir(path, 0777);
    setenv("HOME", data_dir, 1);
    setenv("XDG_CONFIG_HOME", data_dir, 1);
    /* The log, on a file of its own: a PS4 program shows no console, and a
       crash leaves what was written. */
    snprintf(path, sizeof path, "%s/ZSNES Log.txt", data_dir);
    log_file = fopen(path, "w");
    if (log_file)
        setvbuf(log_file, NULL, _IONBF, 0);
    LOG("ZSNES for PS4, started from %s; folder %s", argv0 ? argv0 : "?", data_dir);
    notify("ZSNES: settings and log in %s", data_dir);
    /* The current directory: the PS4's own chdir fails, so ps4/posix.c
       keeps one (ZSNES works out its paths from it). */
    ps4_set_cwd(data_dir);
    if (chdir(data_dir) != 0)
        LOG("chdir %s failed", data_dir);
    /* ZSNES's own messages: standard output's descriptor onto a file, where
       re-opening the stream is refused. */
    snprintf(path, sizeof path, "%s/ZSNES Output.txt", data_dir);
    if (freopen(path, "w", stdout)) {
        setvbuf(stdout, NULL, _IONBF, 0);
    } else {
        int const fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd >= 0 && dup2(fd, STDOUT_FILENO) >= 0) {
            setvbuf(stdout, NULL, _IONBF, 0);
            LOG("ZSNES's messages in %s", path);
        } else {
            LOG("ZSNES's messages are not kept");
        }
    }
    crash_handlers();
    memset(&param, 0, sizeof param);
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    LOG("user service: %x", (unsigned)sceUserServiceInitialize(&param));
    checkpoint("set up");
}

bool zs3_Init(Uint32 const flags)
{
    checkpoint("SDL init");
    system_init();
    inited |= flags;
    return true;
}

bool zs3_InitSubSystem(Uint32 const flags) { return zs3_Init(flags); }
void zs3_QuitSubSystem(Uint32 const flags) { inited &= ~flags; }
Uint32 zs3_WasInit(Uint32 const flags) { return inited & flags; }
void zs3_Quit(void) { }

/* --- the screen: VideoOut ------------------------------------------------- */

#define DISP_W 1920
#define DISP_H 1080
#define NBUF 3
/* The largest picture a mode draws (640x480), for the window's checks. */
#define TEX_W 1024
#define TEX_H 512

static int32_t video_handle = -1;
static off_t vmem_off;
static size_t vmem_size;
static void* vmem;
static u32* fb[NBUF];
static int fb_next;          /* the buffer the next frame is drawn in */
static int fb_clean[NBUF];   /* black where the picture does not reach */
static int video_ready;
static unsigned disp_w = DISP_W, disp_h = DISP_H;
static unsigned frames_shown;
static int64_t flip_arg;

static int video_start(void)
{
    OrbisVideoOutBufferAttribute attr;
    size_t const fb_size = (size_t)DISP_W * DISP_H * 4;
    size_t const align = 0x200000;
    int i;

    if (video_ready)
        return 1;
    checkpoint("video");
    video_handle = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
    if (video_handle < 0) {
        notify("ZSNES: no video output (%x)", (unsigned)video_handle);
        LOG("sceVideoOutOpen failed: %x", (unsigned)video_handle);
        set_error("Could not open the video output");
        return 0;
    }
    vmem_size = (fb_size * NBUF + align - 1) / align * align;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), vmem_size, align, 3, &vmem_off) < 0) {
        notify("ZSNES: could not allocate video memory");
        LOG("could not allocate %u bytes of video memory", (unsigned)vmem_size);
        set_error("Out of video memory");
        return 0;
    }
    if (sceKernelMapDirectMemory(&vmem, vmem_size, 0x33, 0, vmem_off, align) < 0) {
        LOG("could not map video memory");
        sceKernelReleaseDirectMemory(vmem_off, vmem_size);
        set_error("Out of video memory");
        return 0;
    }
    for (i = 0; i < NBUF; i++) {
        fb[i] = (u32*)((char*)vmem + (size_t)i * fb_size);
        memset(fb[i], 0, fb_size);
        fb_clean[i] = 1;
    }
    /* 8-bit sRGB, linear (not tiled), 1920x1080, pitch in pixels. */
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, DISP_W, DISP_H, DISP_W);
    if (sceVideoOutRegisterBuffers(video_handle, 0, (void* const*)fb, NBUF, &attr) != 0) {
        LOG("sceVideoOutRegisterBuffers failed");
        set_error("Could not set up the video buffers");
        return 0;
    }
    sceVideoOutSetFlipRate(video_handle, 0); /* every vertical blank: 60 Hz */
    video_ready = 1;
    LOG("video out %ux%u, %d buffers", disp_w, disp_h, NBUF);
    notify("ZSNES: video ready");
    return 1;
}

static void video_stop(void)
{
    if (video_ready) {
        sceVideoOutClose(video_handle);
        video_ready = 0;
    }
}

/* Before drawing into a buffer: of three, the one on screen and the one
   queued are left alone, so the third is free once at most one flip is
   pending. Bounded: past 100 ms (the TV gone, the system busy) the frame
   is drawn anyway - a torn frame rather than a hang - and the log says. */
static void wait_buffer(void)
{
    OrbisVideoOutFlipStatus st;
    int tries;

    for (tries = 0; tries < 400; tries++) {
        memset(&st, 0, sizeof st);
        if (sceVideoOutGetFlipStatus(video_handle, &st) != 0 || st.numFlipPending <= 1)
            return;
        sceKernelUsleep(250);
    }
    LOG("video: flips not going through (%d pending)", (int)st.numFlipPending);
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

char const* zs3_GetDisplayName(SDL_DisplayID const id) { return id == 1 ? "TV" : NULL; }
SDL_PropertiesID zs3_GetDisplayProperties(SDL_DisplayID const id) { return 0; }
bool zs3_GetBooleanProperty(SDL_PropertiesID const p, char const* const n, bool const def) { return def; }
bool zs3_DisableScreenSaver(void) { return true; }

SDL_DisplayMode const* zs3_GetCurrentDisplayMode(SDL_DisplayID const id)
{
    static SDL_DisplayMode m;

    video_start();
    m.displayID = 1;
    m.w = (int)disp_w;
    m.h = (int)disp_h;
    m.pixel_density = 1.0f;
    m.refresh_rate = 0.0f;
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

struct SDL_Window {
    int w, h, fullscreen;
    SDL_Surface surface;        /* RGB565, what the frontend draws into */
    SDL_Surface const* pending; /* a frame blitted whole, converted at update */
};

static struct SDL_Window the_window;
static int window_open;

bool zs3_BlitSurface(SDL_Surface* const src, SDL_Rect const* const sr, SDL_Surface* const dst, SDL_Rect const* const dr)
{
    SDL_Rect a, b;
    int y, w, h;

    if (dst == &the_window.surface && !sr && !dr && src->w == dst->w && src->h == dst->h) {
        the_window.pending = src;
        return true;
    }
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

/* --- the window: the whole screen ----------------------------------------- */

static int make_surface(struct SDL_Window* const w)
{
    free(w->surface.pixels);
    w->surface.w = w->w;
    w->surface.h = w->h;
    w->surface.pitch = (w->w * 2 + 15) & ~15;
    w->surface.format = SDL_PIXELFORMAT_RGB565;
    w->surface.pixels = calloc((size_t)w->surface.pitch, (size_t)w->h);
    w->pending = NULL;
    return w->surface.pixels != NULL;
}

SDL_Window* zs3_CreateWindow(char const* const title, int const w, int const h, SDL_WindowFlags const flags)
{
    struct SDL_Window* const win = &the_window;

    (void)title;
    system_init();
    if (flags & SDL_WINDOW_OPENGL) {
        set_error("No OpenGL on the PS3 build");
        return NULL;
    }
    if (!video_start())
        return NULL;
    if (w > TEX_W || h > TEX_H) {
        set_error("Video mode too large");
        return NULL;
    }
    win->w = w;
    win->h = h;
    win->fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    LOG("window %dx%d", w, h);
    if (!make_surface(win)) {
        set_error("Out of memory for the screen");
        return NULL;
    }
    window_open = 1;
    return win;
}

void zs3_DestroyWindow(SDL_Window* const win)
{
    if (win && window_open) {
        free(win->surface.pixels);
        win->surface.pixels = NULL;
        win->pending = NULL;
        window_open = 0;
    }
}

bool zs3_SetWindowSize(SDL_Window* const win, int const w, int const h)
{
    if (!win || w > TEX_W || h > TEX_H)
        return false;
    if (win->w == w && win->h == h && win->surface.pixels)
        return true;
    win->w = w;
    win->h = h;
    LOG("window size %dx%d", w, h);
    return make_surface(win);
}

bool zs3_SetWindowFullscreen(SDL_Window* const win, bool const fs)
{
    if (win)
        win->fullscreen = fs;
    return win != NULL;
}

bool zs3_SetWindowPosition(SDL_Window* const win, int const x, int const y) { return true; }
bool zs3_SetWindowMouseGrab(SDL_Window* const win, bool const grab) { return true; }
bool zs3_SyncWindow(SDL_Window* const win) { return true; }
SDL_WindowFlags zs3_GetWindowFlags(SDL_Window* const win) { return win && win->fullscreen ? SDL_WINDOW_FULLSCREEN : 0; }

bool zs3_GetWindowSizeInPixels(SDL_Window* const win, int* const w, int* const h)
{
    if (w)
        *w = win ? win->w : 0;
    if (h)
        *h = win ? win->h : 0;
    return win != NULL;
}

SDL_Surface* zs3_GetWindowSurface(SDL_Window* const win) { return win && win->surface.pixels ? &win->surface : NULL; }

/* RGB565 to the RSX's 32 bits, through a table. */
static u32* lut32;

bool zs3_UpdateWindowSurface(SDL_Window* const win)
{
    /* The picture's place: 4:3, as tall as the screen, centred. */
    enum { OUT_H = DISP_H, OUT_W = DISP_H * 4 / 3, OUT_X = (DISP_W - DISP_H * 4 / 3) / 2 };
    static int xmap[OUT_W];
    static int ymap[OUT_H];
    static int map_w, map_h;
    SDL_Surface const* src;
    u32* buf;
    int x, y, w, h, prev_sy = -1;

    if (!win || !win->surface.pixels || !video_ready)
        return false;
    if (!lut32) {
        unsigned p;

        if (!(lut32 = (u32*)malloc(65536 * sizeof *lut32)))
            return false;
        for (p = 0; p < 65536; p++) {
            unsigned const r = p >> 11, g = p >> 5 & 63, b = p & 31;
            lut32[p] = 0xFF000000u | (u32)((r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2));
        }
    }
    src = win->pending ? win->pending : &win->surface;
    win->pending = NULL;
    w = win->w < src->w ? win->w : src->w;
    h = win->h < src->h ? win->h : src->h;
    if (w <= 0 || h <= 0)
        return false;
    if (w != map_w || h != map_h) {
        for (x = 0; x < OUT_W; x++)
            xmap[x] = x * w / OUT_W;
        for (y = 0; y < OUT_H; y++)
            ymap[y] = y * h / OUT_H;
        map_w = w;
        map_h = h;
        LOG("picture %dx%d", w, h);
    }

    wait_buffer();
    buf = fb[fb_next];
    if (!fb_clean[fb_next]) {
        memset(buf, 0, (size_t)DISP_W * DISP_H * 4);
        fb_clean[fb_next] = 1;
    }
    /* Nearest-neighbour: each source row converted once, into the first
       screen row that shows it, and copied into the rows that repeat it. */
    for (y = 0; y < OUT_H; y++) {
        u32* const o = buf + (size_t)y * DISP_W + OUT_X;
        int const sy = ymap[y];

        if (sy == prev_sy) {
            memcpy(o, o - DISP_W, OUT_W * 4);
        } else {
            Uint16 const* const in = (Uint16 const*)((Uint8 const*)src->pixels + (size_t)sy * src->pitch);

            for (x = 0; x < OUT_W; x++)
                o[x] = lut32[in[xmap[x]]];
            prev_sy = sy;
        }
    }
    if (frames_shown == 0 && flip_arg == 0)
        checkpoint("first frame");
    if (sceVideoOutSubmitFlip(video_handle, fb_next, ORBIS_VIDEO_OUT_FLIP_VSYNC, ++flip_arg) != 0)
        LOG("video: a flip was refused");
    fb_next = (fb_next + 1) % NBUF;
    frames_shown++;
    return true;
}

/* --- OpenGL and the renderer: none ---------------------------------------- */

bool zs3_GL_SetAttribute(SDL_GLAttr const attr, int const value) { return false; }
bool zs3_GL_SetSwapInterval(int const interval) { return false; }
SDL_GLContext zs3_GL_CreateContext(SDL_Window* const win) { return NULL; }
bool zs3_GL_DestroyContext(SDL_GLContext const ctx_) { return true; }
bool zs3_GL_SwapWindow(SDL_Window* const win) { return false; }
int sr_start(int width, int height, int req_depth, int fs) { return 0; }
void sr_end(void) { }
void sr_clearwin(void) { }
void sr_drawwin(void) { }
int sr_vsync_on(void) { return 0; }

bool zs3_StartTextInput(SDL_Window* const win) { return true; }
bool zs3_HideCursor(void) { return true; }

/* --- the menus' pointer --------------------------------------------------- */

/* In the picture's own pixels, moved by pad 1's left stick. */
static float ptr_x = 128, ptr_y = 112;
static Uint32 mouse_buttons;
static float rel_x, rel_y;

static void pointer_move(float const dx, float const dy)
{
    SDL_Event e;
    float const w = (float)(the_window.w > 0 ? the_window.w : 256);
    float const h = (float)(the_window.h > 0 ? the_window.h : 224);

    ptr_x += dx;
    ptr_y += dy;
    ptr_x = ptr_x < 0 ? 0 : ptr_x > w - 1 ? w - 1 : ptr_x;
    ptr_y = ptr_y < 0 ? 0 : ptr_y > h - 1 ? h - 1 : ptr_y;
    rel_x += dx;
    rel_y += dy;
    memset(&e, 0, sizeof e);
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.x = ptr_x;
    e.motion.y = ptr_y;
    e.motion.xrel = dx;
    e.motion.yrel = dy;
    push(&e);
}

static void push_click(int const down)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button = SDL_BUTTON_LEFT;
    if (down)
        mouse_buttons |= 1;
    else
        mouse_buttons &= ~1u;
    push(&e);
}

Uint32 zs3_GetRelativeMouseState(float* const x, float* const y)
{
    if (x)
        *x = rel_x;
    if (y)
        *y = rel_y;
    rel_x = rel_y = 0;
    return mouse_buttons;
}

/* --- pads ----------------------------------------------------------------- */

enum { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_START, B_SELECT, B_A, B_B, B_X, B_Y, B_L, B_R, B_COUNT };
#define PLAYERS 4

/* Players 1 to 4's keys, as Config > Input has them. */
static unsigned int* const keys[PLAYERS][B_COUNT] = {
    { &pl1upk, &pl1downk, &pl1leftk, &pl1rightk, &pl1startk, &pl1selk, &pl1Ak, &pl1Bk, &pl1Xk, &pl1Yk, &pl1Lk, &pl1Rk },
    { &pl2upk, &pl2downk, &pl2leftk, &pl2rightk, &pl2startk, &pl2selk, &pl2Ak, &pl2Bk, &pl2Xk, &pl2Yk, &pl2Lk, &pl2Rk },
    { &pl3upk, &pl3downk, &pl3leftk, &pl3rightk, &pl3startk, &pl3selk, &pl3Ak, &pl3Bk, &pl3Xk, &pl3Yk, &pl3Lk, &pl3Rk },
    { &pl4upk, &pl4downk, &pl4leftk, &pl4rightk, &pl4startk, &pl4selk, &pl4Ak, &pl4Bk, &pl4Xk, &pl4Yk, &pl4Lk, &pl4Rk },
};

/* Pads 2 to 4 have codes of their own past the keyboard's, given to players
   2 to 4 when they have no keys yet - so a second pad plays player 2 with
   no setting up - and pressing them in Config > Input binds them. */
#define PAD_CODE(pad, b) (256u + 32u * (unsigned)(pad) + (unsigned)(b))

static void pads_init(void)
{
    int p, b;

    for (p = 1; p < PLAYERS; p++) {
        int unset = 1;

        for (b = 0; b < B_COUNT; b++)
            if (*keys[p][b])
                unset = 0;
        if (unset)
            for (b = 0; b < B_COUNT; b++)
                *keys[p][b] = PAD_CODE(p, b);
    }
}

typedef struct {
    int connected;
    unsigned char btn[B_COUNT]; /* the SNES buttons as the pad has them */
    unsigned char l3;           /* the frame counter */
    unsigned char r3;           /* the ZSNES menu */
    float lx, ly;               /* left stick, -1 to 1 */
} Pad;

static Pad pads[PLAYERS];
static unsigned char game_down[PLAYERS][B_COUNT]; /* what each pad holds in the game's keys */
static int pointer_mode;                          /* the menus last moved with the stick */

static void set_key(unsigned const k, int const down)
{
    if (k && k < PRESSED_SIZE)
        pressed[k] = (unsigned char)(down != 0);
}

/* The SNES buttons each pad holds, onto its player's keys. */
static void apply(int const p, unsigned char const* const want)
{
    int b;

    for (b = 0; b < B_COUNT; b++) {
        unsigned const k = p == 0 ? *keys[0][b] : PAD_CODE(p, b);

        if (want[b] != game_down[p][b]) {
            game_down[p][b] = want[b];
            set_key(k, want[b]);
            /* a player 2-4 bound to other keys in Config > Input */
            if (p > 0 && *keys[p][b] != k)
                set_key(*keys[p][b], want[b]);
        }
    }
}

static int32_t pad_handle[PLAYERS] = { -1, -1, -1, -1 };
static int32_t pad_user[PLAYERS] = { -1, -1, -1, -1 };

/* Players 1 to 4: the logged-in users, in the system's order. Checked again
   every two seconds, for users who log in or switch on a pad later. */
static void pads_open(void)
{
    OrbisUserServiceLoginUserIdList list;
    int p;

    memset(&list, 0xFF, sizeof list);
    if (sceUserServiceGetLoginUserIdList(&list) != 0)
        return;
    for (p = 0; p < PLAYERS && p < (int)(sizeof list.userId / sizeof list.userId[0]); p++) {
        int32_t const user = list.userId[p];

        if (user == pad_user[p] && pad_handle[p] >= 0)
            continue;
        if (pad_handle[p] >= 0) {
            scePadClose(pad_handle[p]);
            pad_handle[p] = -1;
            LOG("pad %d closed", p + 1);
        }
        pad_user[p] = user;
        if (user < 0 || user == ORBIS_USER_SERVICE_USER_ID_SYSTEM)
            continue;
        pad_handle[p] = scePadOpen(user, 0, 0, NULL);
        if (pad_handle[p] < 0) {
            /* Already open (a second open of one user's pad fails): ask for it. */
            pad_handle[p] = scePadGetHandle(user, 0, 0);
        }
        LOG("player %d: user %x, pad handle %d", p + 1, (unsigned)user, (int)pad_handle[p]);
    }
}

static void pad_read(int const p, OrbisPadData const* const d)
{
    Pad* const s = &pads[p];
    uint32_t const b = d->buttons;

    s->btn[B_UP] = (b & ORBIS_PAD_BUTTON_UP) != 0;
    s->btn[B_DOWN] = (b & ORBIS_PAD_BUTTON_DOWN) != 0;
    s->btn[B_LEFT] = (b & ORBIS_PAD_BUTTON_LEFT) != 0;
    s->btn[B_RIGHT] = (b & ORBIS_PAD_BUTTON_RIGHT) != 0;
    /* Options is Start; the touch pad's click is Select (the Share button
       belongs to the system). */
    s->btn[B_START] = (b & ORBIS_PAD_BUTTON_OPTIONS) != 0;
    s->btn[B_SELECT] = (b & ORBIS_PAD_BUTTON_TOUCH_PAD) != 0;
    /* By position, as a SNES pad's buttons sit: Circle A, Cross B,
       Triangle X, Square Y; L1 or L2 L, R1 or R2 R. */
    s->btn[B_A] = (b & ORBIS_PAD_BUTTON_CIRCLE) != 0;
    s->btn[B_B] = (b & ORBIS_PAD_BUTTON_CROSS) != 0;
    s->btn[B_X] = (b & ORBIS_PAD_BUTTON_TRIANGLE) != 0;
    s->btn[B_Y] = (b & ORBIS_PAD_BUTTON_SQUARE) != 0;
    s->btn[B_L] = (b & (ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_L2)) != 0;
    s->btn[B_R] = (b & (ORBIS_PAD_BUTTON_R1 | ORBIS_PAD_BUTTON_R2)) != 0;
    s->l3 = (b & ORBIS_PAD_BUTTON_L3) != 0;
    s->r3 = (b & ORBIS_PAD_BUTTON_R3) != 0;
    s->lx = ((float)d->leftStick.x - 128.0f) / 127.0f;
    s->ly = ((float)d->leftStick.y - 128.0f) / 127.0f;
}

static void pads_poll(void)
{
    static unsigned char chord, menu_cross, menu_circle, l3_was;
    static uint64_t last_us, next_open;
    int p;
    uint64_t const now = microseconds();
    float const dt = last_us ? (float)(now - last_us) / 1e6f : 0.0f;

    last_us = now;
    if (now >= next_open) {
        pads_open();
        next_open = now + 2000000u;
    }
    for (p = 0; p < PLAYERS; p++) {
        OrbisPadData d;

        memset(&d, 0, sizeof d);
        if (pad_handle[p] < 0 || scePadReadState(pad_handle[p], &d) != 0 || !d.connected) {
            memset(&pads[p], 0, sizeof pads[p]);
            continue;
        }
        pads[p].connected = 1;
        pad_read(p, &d);
    }

    /* L3 on pad 1: the frame counter, as Command-F or Ctrl-F do on the
       other ports. ZSNES counts frames only while it picks the frameskip. */
    if (pads[0].l3 && !l3_was) {
        frameskip = 0;
        FPSOn ^= 1;
    }
    l3_was = pads[0].l3;

    /* Options+touch pad (Start+Select), or R3, on any pad: the ZSNES menu,
       from a game (held as Esc until let go). The PS button belongs to the
       system. */
    {
        int both = 0;

        for (p = 0; p < PLAYERS; p++)
            both |= (pads[p].btn[B_START] && pads[p].btn[B_SELECT]) || pads[p].r3;
        if (both && !chord) {
            chord = in_menus() ? 2 : 1;
            if (chord == 1)
                push_key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, 1);
        } else if (!both && chord) {
            if (chord == 1)
                push_key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, 0);
            chord = 0;
        }
    }

    for (p = 0; p < PLAYERS; p++) {
        Pad const* const s = &pads[p];
        unsigned char want[B_COUNT];
        int b;

        for (b = 0; b < B_COUNT; b++)
            want[b] = s->btn[b];
        if (!(in_menus() && p == 0)) {
            /* The left stick as the d-pad, past half way. */
            want[B_LEFT] |= s->lx < -0.5f;
            want[B_RIGHT] |= s->lx > 0.5f;
            want[B_UP] |= s->ly < -0.5f;
            want[B_DOWN] |= s->ly > 0.5f;
        }
        if (chord) {
            want[B_START] = 0;
            want[B_SELECT] = 0;
        }
        if (in_menus()) {
            /* The menus take Cross and Circle below; Start (Return by
               default) and the rest do nothing there. */
            want[B_START] = want[B_SELECT] = 0;
            want[B_A] = want[B_B] = want[B_X] = want[B_Y] = want[B_L] = want[B_R] = 0;
            if (p == 0 && (want[B_UP] | want[B_DOWN] | want[B_LEFT] | want[B_RIGHT]))
                pointer_mode = 0; /* the d-pad moves the highlight */
        }
        apply(p, want);
    }

    /* The menus, from pad 1: the stick moves the pointer; Cross picks (a
       click after the stick, Return on the highlight after the d-pad);
       Circle goes back. A press's release ends what it started. */
    {
        Pad const* const s = &pads[0];
        float const m = sqrtf(s->lx * s->lx + s->ly * s->ly);
        int const cross = in_menus() && s->btn[B_B], circle = in_menus() && s->btn[B_A];

        if (in_menus() && m >= 0.2f && dt > 0.0f && dt < 0.25f) {
            float sp = (m - 0.2f) / 0.8f;
            float const w = (float)(the_window.w > 0 ? the_window.w : 256);

            sp = sp * sp * 1.1f * w / m; /* about a picture's width a second */
            pointer_mode = 1;
            pointer_move(s->lx * sp * dt, s->ly * sp * dt);
        }
        if (cross && !menu_cross) {
            menu_cross = pointer_mode ? 2 : 1;
            if (menu_cross == 2)
                push_click(1);
            else
                push_key(SDL_SCANCODE_RETURN, SDLK_RETURN, 1);
        } else if (!s->btn[B_B] && menu_cross) {
            if (menu_cross == 2)
                push_click(0);
            else
                push_key(SDL_SCANCODE_RETURN, SDLK_RETURN, 0);
            menu_cross = 0;
        }
        if (circle && !menu_circle) {
            menu_circle = 1;
            push_key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, 1);
        } else if (!s->btn[B_A] && menu_circle) {
            menu_circle = 0;
            push_key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, 0);
        }
    }
}

/* --- events --------------------------------------------------------------- */

void zs3_PumpEvents(void)
{
    static uint64_t next_note;
    static unsigned frames_seen;
    uint64_t const now = microseconds();

    /* Every ten seconds: still running, and how fast frames are coming. */
    frames_seen++;
    if (now >= next_note) {
        if (next_note)
            LOG("alive: %u frames shown and %u polls in 10 s, menus %s", frames_shown, frames_seen,
                in_menus() ? "open" : "closed");
        frames_shown = 0;
        frames_seen = 0;
        next_note = now + 10000000u;
    }
    system_init();
    pads_poll();
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

/* --- joysticks: the pads go straight to the players' keys above ----------- */

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

/* --- threads: none for the frontend; semaphores count, waits are timed ---- */

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
    uint64_t const end = microseconds() + (uint64_t)(ms < 0 ? 0 : ms) * 1000u;

    for (;;) {
        if (s->count) {
            s->count--;
            return true;
        }
        if (ms >= 0 && microseconds() >= end)
            return false;
        sceKernelUsleep(100);
    }
}

/* --- sound: an audio port, fed from a thread ------------------------------ */

#define DEVICE_RATE 48000
#define PORT_FRAMES 256
#define RING_BYTES (1 << 16)

struct SDL_AudioStream {
    Uint8 ring[RING_BYTES];
    unsigned head, tail; /* the emulator writes head, the thread tail: atomically */
    int channels, in_freq;
    int resample;
    zrs rs;
};

static struct SDL_AudioStream* the_stream; /* read and written atomically */
static int32_t audio_port = -1;
static OrbisPthread audio_thread_id;
static volatile int audio_running;
static volatile int audio_in_block; /* the thread is reading a stream */

/* One block of the port, from the ring: silence for what is missing. The
   port always plays 16-bit stereo. */
static void fill_block(int16_t* const out)
{
    struct SDL_AudioStream* s;
    int i;

    /* Say it is reading before taking the pointer; zs3_DestroyAudioStream
       clears the pointer before looking (both sequentially consistent), so
       either it sees this block under way or this block sees no stream. */
    __atomic_store_n(&audio_in_block, 1, __ATOMIC_SEQ_CST);
    s = __atomic_load_n(&the_stream, __ATOMIC_SEQ_CST);
    for (i = 0; i < PORT_FRAMES; i++) {
        int16_t l = 0, r = 0;

        if (s) {
            unsigned const frame = 2u * (unsigned)s->channels;
            unsigned const head = __atomic_load_n(&s->head, __ATOMIC_ACQUIRE);
            unsigned const tail = s->tail;

            if (head - tail >= frame) {
                int16_t v[2];

                memcpy(v, s->ring + tail % RING_BYTES, frame);
                l = v[0];
                r = s->channels == 2 ? v[1] : l;
                __atomic_store_n(&s->tail, tail + frame, __ATOMIC_RELEASE);
            }
        }
        out[i * 2] = l;
        out[i * 2 + 1] = r;
    }
    __atomic_store_n(&audio_in_block, 0, __ATOMIC_SEQ_CST);
}

/* sceAudioOutOutput returns once the port has room for the next block, so
   the hardware sets the pace; the thread never waits on the emulator. */
static void* audio_thread(void* const arg)
{
    static int16_t block[2][PORT_FRAMES * 2];
    int i = 0;

    (void)arg;
    while (audio_running) {
        fill_block(block[i]);
        if (sceAudioOutOutput(audio_port, block[i]) < 0) {
            LOG("audio: output failed; sound stops");
            break;
        }
        i ^= 1;
    }
    sceAudioOutOutput(audio_port, NULL); /* let the last block play out */
    return NULL;
}

static int audio_start(void)
{
    if (audio_running)
        return 1;
    if (audio_port < 0) {
        int const rc = sceAudioOutInit();

        if (rc != 0 && rc != (int)0x8026000E) /* already initialised */
            LOG("sceAudioOutInit: %x", (unsigned)rc);
        audio_port = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
            PORT_FRAMES, DEVICE_RATE, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
        if (audio_port <= 0) {
            LOG("sceAudioOutOpen failed: %x", (unsigned)audio_port);
            audio_port = -1;
            return 0;
        }
    }
    audio_running = 1;
    if (scePthreadCreate(&audio_thread_id, NULL, audio_thread, NULL, "zsnes_audio") != 0) {
        LOG("could not start the sound thread");
        audio_running = 0;
        return 0;
    }
    LOG("audio port %d: %d Hz, %d-frame blocks", (int)audio_port, DEVICE_RATE, PORT_FRAMES);
    return 1;
}

static void audio_stop(void)
{
    if (audio_running) {
        audio_running = 0;
        scePthreadJoin(audio_thread_id, NULL);
    }
    if (audio_port >= 0) {
        sceAudioOutClose(audio_port);
        audio_port = -1;
    }
}

SDL_AudioStream* zs3_OpenAudioDeviceStream(SDL_AudioDeviceID const dev, SDL_AudioSpec const* const spec,
    SDL_AudioStreamCallback const cb, void* const userdata)
{
    struct SDL_AudioStream* s;

    (void)dev;
    (void)cb;
    (void)userdata;
    if (!spec || spec->format != SDL_AUDIO_S16 || __atomic_load_n(&the_stream, __ATOMIC_SEQ_CST)
        || spec->channels < 1 || spec->channels > 2)
        return NULL;
    if (!(s = (struct SDL_AudioStream*)calloc(1, sizeof *s)))
        return NULL;
    s->channels = spec->channels;
    s->in_freq = spec->freq;
    s->resample = spec->freq != DEVICE_RATE
        && zrs_init(&s->rs, (unsigned)spec->freq, DEVICE_RATE, (unsigned)spec->channels) == 0;
    checkpoint("sound");
    if (!audio_start()) {
        set_error("Could not open the audio port");
        if (s->resample)
            zrs_free(&s->rs);
        free(s);
        return NULL;
    }
    LOG("audio stream opened: %d Hz, %d channels", spec->freq, spec->channels);
    __atomic_store_n(&the_stream, s, __ATOMIC_SEQ_CST);
    return s;
}

bool zs3_ResumeAudioStreamDevice(SDL_AudioStream* const s) { return s != NULL; }

/* The thread stops reading the stream before it is freed: it takes the
   pointer anew for each block and says while it is in one. */
void zs3_DestroyAudioStream(SDL_AudioStream* const s)
{
    int spins;

    if (!s || s != __atomic_load_n(&the_stream, __ATOMIC_SEQ_CST))
        return;
    LOG("audio stream closed");
    __atomic_store_n(&the_stream, NULL, __ATOMIC_SEQ_CST);
    /* A block takes microseconds and calls nothing that blocks, so this
       ends as soon as the thread runs; freeing sooner could pull the ring
       out from under it. */
    for (spins = 0; __atomic_load_n(&audio_in_block, __ATOMIC_SEQ_CST); spins++)
        sceKernelUsleep(10);
    if (spins > 1000)
        LOG("audio: waited %d ms for the sound thread", spins / 100);
    if (s->resample)
        zrs_free(&s->rs);
    free(s);
}

static void ring_put(struct SDL_AudioStream* const s, Uint8 const* const buf, unsigned const len)
{
    unsigned const frame = 2u * (unsigned)s->channels;
    unsigned const head = s->head;
    unsigned const room = RING_BYTES - (head - __atomic_load_n(&s->tail, __ATOMIC_ACQUIRE));
    unsigned n = len < room ? len : room;
    unsigned h, first;

    n -= n % frame;
    h = head % RING_BYTES;
    first = RING_BYTES - h < n ? RING_BYTES - h : n;
    memcpy(s->ring + h, buf, first);
    memcpy(s->ring, buf + first, n - first);
    __atomic_store_n(&s->head, head + n, __ATOMIC_RELEASE); /* after the samples */
}

bool zs3_PutAudioStreamData(SDL_AudioStream* const s, void const* const buf, int const len)
{
    if (!s || len < 0)
        return false;
    if (!s->resample) {
        ring_put(s, (Uint8 const*)buf, (unsigned)len);
    } else {
        enum { PIECE = 512 };
        static Sint16 out[2 * (PIECE * 3 + 8)];
        Sint16 const* in = (Sint16 const*)buf;
        unsigned frames = (unsigned)len / (2u * (unsigned)s->channels);

        while (frames) {
            unsigned const n = frames < PIECE ? frames : PIECE;
            unsigned const made = zrs_run(&s->rs, in, n, out);

            ring_put(s, (Uint8 const*)out, made * 2u * (unsigned)s->channels);
            in += n * (unsigned)s->channels;
            frames -= n;
        }
    }
    return true;
}

/* What is queued, in the frontend's bytes (at its rate), as SDL3 counts it. */
int zs3_GetAudioStreamQueued(SDL_AudioStream* const s)
{
    unsigned n, frame;

    if (!s)
        return -1;
    n = __atomic_load_n(&s->head, __ATOMIC_ACQUIRE) - __atomic_load_n(&s->tail, __ATOMIC_ACQUIRE);
    if (s->resample) {
        frame = 2u * (unsigned)s->channels;
        n = (unsigned)((unsigned long long)(n / frame) * (unsigned)s->in_freq / DEVICE_RATE) * frame;
    }
    return (int)n;
}

bool zs3_SetAudioStreamFrequencyRatio(SDL_AudioStream* const s, float const ratio) { return s != NULL; }
SDL_AudioDeviceID zs3_GetAudioStreamDevice(SDL_AudioStream* const s) { return s ? 1 : 0; }

bool zs3_GetAudioDeviceFormat(SDL_AudioDeviceID const dev, SDL_AudioSpec* const spec, int* const frames)
{
    struct SDL_AudioStream* const s = __atomic_load_n(&the_stream, __ATOMIC_SEQ_CST);

    if (!s)
        return false;
    if (spec) {
        spec->format = SDL_AUDIO_S16;
        spec->channels = s->channels;
        spec->freq = s->resample ? DEVICE_RATE : s->in_freq;
    }
    if (frames)
        *frames = PORT_FRAMES * 4;
    return true;
}

char const* zs3_GetCurrentAudioDriver(void) { return "PS4 audio"; }
int zs3_GetNumAudioDrivers(void) { return 0; }
char const* zs3_GetAudioDriver(int const index) { return NULL; }

/* macos9/net_stub.c's clock, which the PS4 build shares. */
uint64_t macos9_microseconds(void) { return microseconds(); }
