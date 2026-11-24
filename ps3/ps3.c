/* ps3.c - the part of SDL3 the ZSNES frontend uses (unix/sdl12compat.h,
   whose declarations this file implements), on the PlayStation 3's own
   libraries through PSL1GHT, for homebrew run from the XMB.

   - Video: the frontend draws RGB565 into a surface in main memory; each
     frame is converted to 32 bits into a buffer in the RSX's memory, and the
     RSX scales it, 4:3, onto the screen at the resolution the PS3 is set to
     (720p, 1080p, ...), with black at the sides.
   - Controllers: DualShock 3 (or any pad the PS3 takes) 1 is player 1 on the
     keys Config > Input gives player 1; pads 2 to 4 are players 2 to 4. In
     the ZSNES menus pad 1's left stick moves the pointer, Cross picks and
     Circle goes back; Start+Select opens the menu from a game.
   - A USB keyboard works as a keyboard.
   - Sound: a 48 kHz audio port fed from a thread of its own, the DSP's
     32000 Hz converted with unix/resample.c.
   - No threads for the frontend: as on Mac OS 9, its frame-timer thread is
     not started and its waits are timed. */
#include <audio/audio.h>
#include <io/kb.h>
#include <io/pad.h>
#include <malloc.h>
#include <ppu-types.h>
#include <rsx/rsx.h>
#include <sys/process.h>
#include <sys/systime.h>
#include <sys/thread.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../unix/resample.h"
#include "../unix/sdl12compat.h"
#include "input.h" /* the players' keys */

SYS_PROCESS_PARAM(1001, 0x100000);

/* A line in "ZSNES Log.txt" (see ps3_setup), written and flushed at once. */
static FILE* log_file;
#define LOG(...) \
    do { \
        if (log_file) { \
            fprintf(log_file, "ps3: " __VA_ARGS__); \
            fputc('\n', log_file); \
            fflush(log_file); \
        } \
    } while (0)

/* The emulator's key state (cpu/execute.h, which this file cannot include:
   its u8 is 8 bytes and PSL1GHT's 8 bits). */
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

/* --- the system: the XMB's quit, and the PS button's overlay -------------- */

static void sysutil_callback(u64 const status, u64 const param, void* const data)
{
    (void)param;
    (void)data;
    if (status == SYSUTIL_EXIT_GAME) {
        LOG("the XMB asks to quit");
        quit_requested = 1;
    }
}

static void audio_stop(void);
static void video_stop(void);

static void shutdown_all(void)
{
    audio_stop();
    video_stop();
    ioKbEnd();
    ioPadEnd();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
}

static int system_ready;
static void pads_init(void);

static void system_init(void)
{
    if (system_ready)
        return;
    system_ready = 1;
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_callback, NULL);
    ioPadInit(7);
    ioKbInit(1);
    pads_init();
    atexit(shutdown_all);
}

/* Before the frontend reads its paths: settings beside the program, in its
   USRDIR, and the file list starting there. */
void ps3_setup(char const* const argv0)
{
    char dir[1024], path[1100];
    char* slash;

    /* The program's folder, from its path; the installed one if that is not
       a full path. */
    snprintf(dir, sizeof dir, "%s", argv0 ? argv0 : "");
    slash = strrchr(dir, '/');
    if (dir[0] == '/' && slash && slash != dir)
        *slash = '\0';
    else
        snprintf(dir, sizeof dir, "/dev_hdd0/game/ZSNE00001/USRDIR");
    setenv("HOME", dir, 1);
    setenv("XDG_CONFIG_HOME", dir, 1);
    /* The log, by its whole path and on a file of its own: the PS3 shows a
       program no console, and a crash leaves what was written. */
    snprintf(path, sizeof path, "%s/ZSNES Log.txt", dir);
    log_file = fopen(path, "w");
    if (log_file)
        setvbuf(log_file, NULL, _IONBF, 0);
    LOG("ZSNES for PS3, started from %s; folder %s", argv0 ? argv0 : "?", dir);
    if (chdir(dir) != 0)
        LOG("chdir %s failed", dir);
    /* ZSNES's own messages, where the system lets standard output go. */
    snprintf(path, sizeof path, "%s/ZSNES Output.txt", dir);
    if (freopen(path, "w", stdout))
        setvbuf(stdout, NULL, _IONBF, 0);
    else
        LOG("standard output stays the system's");
}

bool zs3_Init(Uint32 const flags)
{
    system_init();
    inited |= flags;
    return true;
}

bool zs3_InitSubSystem(Uint32 const flags) { return zs3_Init(flags); }
void zs3_QuitSubSystem(Uint32 const flags) { inited &= ~flags; }
Uint32 zs3_WasInit(Uint32 const flags) { return inited & flags; }
void zs3_Quit(void) { }

/* --- the screen: RSX ------------------------------------------------------ */

#define CB_SIZE 0x100000
#define HOST_SIZE (32 * 1024 * 1024)
#define GCM_LABEL_INDEX 255
/* The picture's buffer on the RSX: room for the largest mode (640x480). */
#define TEX_W 1024
#define TEX_H 512

static gcmContextData* ctx;
static void* host_mem;
static u32 disp_w, disp_h, color_pitch, depth_pitch;
static u32* color_buf[2];
static u32 color_off[2];
static u32* depth_buf;
static u32 depth_off;
static u32* tex[2];
static u32 tex_off[2];
static int cur_fb, cur_tex, first_flip = 1, video_ready;
static unsigned frames_shown;
static u32 label_val = 1;

static void wait_rsx_idle(void)
{
    rsxSetWriteBackendLabel(ctx, GCM_LABEL_INDEX, label_val);
    rsxSetWaitLabel(ctx, GCM_LABEL_INDEX, label_val);
    label_val++;
    rsxSetWriteBackendLabel(ctx, GCM_LABEL_INDEX, label_val);
    rsxFlushBuffer(ctx);
    while (*(vu32*)gcmGetLabelAddress(GCM_LABEL_INDEX) != label_val)
        usleep(30);
    label_val++;
}

static void set_render_target(int const i)
{
    gcmSurface sf;

    memset(&sf, 0, sizeof sf);
    sf.colorFormat = GCM_SURFACE_X8R8G8B8;
    sf.colorTarget = GCM_SURFACE_TARGET_0;
    sf.colorLocation[0] = GCM_LOCATION_RSX;
    sf.colorOffset[0] = color_off[i];
    sf.colorPitch[0] = color_pitch;
    sf.colorLocation[1] = sf.colorLocation[2] = sf.colorLocation[3] = GCM_LOCATION_RSX;
    sf.colorPitch[1] = sf.colorPitch[2] = sf.colorPitch[3] = 64;
    sf.depthFormat = GCM_SURFACE_ZETA_Z16;
    sf.depthLocation = GCM_LOCATION_RSX;
    sf.depthOffset = depth_off;
    sf.depthPitch = depth_pitch;
    sf.type = GCM_SURFACE_TYPE_LINEAR;
    sf.antiAlias = GCM_SURFACE_CENTER_1;
    sf.width = disp_w;
    sf.height = disp_h;
    rsxSetSurface(ctx, &sf);
}

static int video_start(void)
{
    videoState state;
    videoConfiguration vc;
    videoResolution res;
    int i;

    if (video_ready)
        return 1;
    host_mem = memalign(1024 * 1024, HOST_SIZE);
    if (!host_mem || rsxInit(&ctx, CB_SIZE, HOST_SIZE, host_mem) != 0 || !ctx) {
        set_error("Could not start the RSX");
        return 0;
    }
    if (videoGetState(0, 0, &state) != 0 || videoGetResolution(state.displayMode.resolution, &res) != 0) {
        set_error("Could not read the video mode");
        return 0;
    }
    memset(&vc, 0, sizeof vc);
    vc.resolution = state.displayMode.resolution;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch = res.width * sizeof(u32);
    wait_rsx_idle();
    videoConfigure(0, &vc, NULL, 0);
    videoGetState(0, 0, &state);
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    disp_w = res.width;
    disp_h = res.height;
    LOG("video %ux%u (resolution id %d)", (unsigned)disp_w, (unsigned)disp_h, (int)state.displayMode.resolution);
    color_pitch = disp_w * sizeof(u32);
    depth_pitch = disp_w * sizeof(u32);
    for (i = 0; i < 2; i++) {
        color_buf[i] = (u32*)rsxMemalign(64, disp_h * color_pitch);
        tex[i] = (u32*)rsxMemalign(64, TEX_W * TEX_H * 4);
        if (!color_buf[i] || !tex[i]) {
            set_error("Out of video memory");
            return 0;
        }
        rsxAddressToOffset(color_buf[i], &color_off[i]);
        rsxAddressToOffset(tex[i], &tex_off[i]);
        gcmSetDisplayBuffer(i, color_off[i], color_pitch, disp_w, disp_h);
        memset(color_buf[i], 0, disp_h * color_pitch);
    }
    depth_buf = (u32*)rsxMemalign(64, disp_h * depth_pitch);
    rsxAddressToOffset(depth_buf, &depth_off);
    set_render_target(cur_fb);
    video_ready = 1;
    LOG("RSX ready: frame buffers at %08x %08x, picture buffers at %08x %08x", (unsigned)color_off[0],
        (unsigned)color_off[1], (unsigned)tex_off[0], (unsigned)tex_off[1]);
    return 1;
}

static void video_stop(void)
{
    if (video_ready) {
        gcmSetWaitFlip(ctx);
        rsxFinish(ctx, 1);
        video_ready = 0;
    }
}


/* The last frame's flip. Logged when it takes over a second: a hung RSX
   (a command it refused) shows as a frozen picture, and the log then says
   so rather than leaving it to guesswork. */
static void wait_flip(void)
{
    uint64_t const start = microseconds();
    int noted = 0;

    while (gcmGetFlipStatus() != 0) {
        sysUsleep(200);
        if (!noted && microseconds() - start > 1000000u) {
            LOG("RSX: a flip has not finished after a second");
            noted = 1;
        }
    }
    if (noted)
        LOG("RSX: the flip finished after %u ms", (unsigned)((microseconds() - start) / 1000u));
    gcmResetFlipStatus();
}

static void flip(void)
{
    if (!first_flip)
        wait_flip();
    else
        gcmResetFlipStatus();
    gcmSetFlip(ctx, cur_fb);
    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);
    cur_fb ^= 1;
    set_render_target(cur_fb);
    first_flip = 0;
}

/* The picture (w x h, in tex[cur_tex]) onto the screen, 4:3 and as tall as
   the screen, by the RSX's scaler. */
static void show(int const w, int const h)
{
    gcmTransferScale scale;
    gcmTransferSurface surface;
    float const out_h = (float)disp_h, out_w = (float)disp_h * 4.0f / 3.0f;
    float const ox = ((float)disp_w - out_w) / 2.0f;
    int const linear = BilinearFilter && !in_menus();

    /* Sync to the TV with ZSNES's VSync setting; a late frame waits then.
       Set only when it changes. */
    {
        static int mode = -1;
        int const want = vsyncon ? 1 : 0;

        if (want != mode) {
            mode = want;
            gcmSetFlipMode(want ? GCM_FLIP_VSYNC : GCM_FLIP_HSYNC);
            LOG("vsync %s", want ? "on" : "off");
        }
    }
    rsxSetClearColor(ctx, 0);
    rsxClearSurface(ctx, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);

    memset(&scale, 0, sizeof scale);
    scale.conversion = GCM_TRANSFER_CONVERSION_TRUNCATE;
    scale.format = GCM_TRANSFER_SCALE_FORMAT_A8R8G8B8;
    scale.origin = linear ? GCM_TRANSFER_ORIGIN_CENTER : GCM_TRANSFER_ORIGIN_CORNER;
    scale.operation = GCM_TRANSFER_OPERATION_SRCCOPY;
    scale.interp = linear ? GCM_TRANSFER_INTERPOLATOR_LINEAR : GCM_TRANSFER_INTERPOLATOR_NEAREST;
    scale.clipX = 0;
    scale.clipY = 0;
    scale.clipW = disp_w;
    scale.clipH = disp_h;
    scale.outX = (s16)ox;
    scale.outY = 0;
    scale.outW = (u16)out_w;
    scale.outH = (u16)out_h;
    scale.ratioX = rsxGetFixedSint32((float)w / out_w);
    scale.ratioY = rsxGetFixedSint32((float)h / out_h);
    scale.inX = rsxGetFixedUint16(0.0f);
    scale.inY = rsxGetFixedUint16(0.0f);
    scale.inW = (u16)w;
    scale.inH = (u16)h;
    scale.offset = tex_off[cur_tex];
    scale.pitch = TEX_W * 4;

    memset(&surface, 0, sizeof surface);
    surface.format = GCM_TRANSFER_SURFACE_FORMAT_A8R8G8B8;
    surface.pitch = color_pitch;
    surface.offset = color_off[cur_fb];

    rsxSetTransferScaleMode(ctx, GCM_TRANSFER_LOCAL_TO_LOCAL, GCM_TRANSFER_SURFACE);
    rsxSetTransferScaleSurface(ctx, &scale, &surface);
    flip();
    cur_tex ^= 1;
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
    SDL_Surface const* src;
    u32* out;
    int x, y, w, h;

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
    out = tex[cur_tex];
    for (y = 0; y < h; y++) {
        Uint16 const* const in = (Uint16 const*)((Uint8 const*)src->pixels + (size_t)y * src->pitch);
        u32* const o = out + (size_t)y * TEX_W;

        for (x = 0; x < w; x++)
            o[x] = lut32[in[x]];
    }
    show(w, h);
    frames_shown++;
    {
        static int last_w, last_h;

        if (w != last_w || h != last_h) {
            LOG("picture %dx%d", w, h);
            last_w = w;
            last_h = h;
        }
    }
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

static void pad_read(int const p, padData const* const d)
{
    Pad* const s = &pads[p];

    s->btn[B_UP] = d->BTN_UP;
    s->btn[B_DOWN] = d->BTN_DOWN;
    s->btn[B_LEFT] = d->BTN_LEFT;
    s->btn[B_RIGHT] = d->BTN_RIGHT;
    s->btn[B_START] = d->BTN_START;
    s->btn[B_SELECT] = d->BTN_SELECT;
    /* By position, as a SNES pad's buttons sit: Circle A, Cross B,
       Triangle X, Square Y; L1 or L2 L, R1 or R2 R. */
    s->btn[B_A] = d->BTN_CIRCLE;
    s->btn[B_B] = d->BTN_CROSS;
    s->btn[B_X] = d->BTN_TRIANGLE;
    s->btn[B_Y] = d->BTN_SQUARE;
    s->btn[B_L] = d->BTN_L1 | d->BTN_L2;
    s->btn[B_R] = d->BTN_R1 | d->BTN_R2;
    s->l3 = d->BTN_L3;
    s->lx = ((float)d->ANA_L_H - 128.0f) / 127.0f;
    s->ly = ((float)d->ANA_L_V - 128.0f) / 127.0f;
}

static void pads_poll(void)
{
    static unsigned char chord, menu_cross, menu_circle, l3_was;
    static uint64_t last_us;
    padInfo info;
    int p;
    uint64_t const now = microseconds();
    float const dt = last_us ? (float)(now - last_us) / 1e6f : 0.0f;

    last_us = now;
    if (ioPadGetInfo(&info) != 0)
        return;
    for (p = 0; p < PLAYERS; p++) {
        padData d;

        pads[p].connected = info.status[p] != 0;
        if (!pads[p].connected) {
            memset(&pads[p], 0, sizeof pads[p]);
            continue;
        }
        if (ioPadGetData((u32)p, &d) == 0 && d.len > 0)
            pad_read(p, &d);
    }

    /* L3 on pad 1: the frame counter, as Command-F or Ctrl-F do on the
       other ports. ZSNES counts frames only while it picks the frameskip. */
    if (pads[0].l3 && !l3_was) {
        frameskip = 0;
        FPSOn ^= 1;
    }
    l3_was = pads[0].l3;

    /* Start+Select on any pad: the ZSNES menu, from a game (held as Esc
       until let go). The PS button belongs to the system. */
    {
        int both = 0;

        for (p = 0; p < PLAYERS; p++)
            both |= pads[p].btn[B_START] && pads[p].btn[B_SELECT];
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

/* --- a USB keyboard ------------------------------------------------------- */

/* The PS3 reports keys by their USB usage, which is what SDL's scancodes
   are; the character for the keycode is worked out for the plain keys. */
static SDL_Keycode keycode_of(unsigned const u)
{
    if (u >= 0x04 && u <= 0x1D)
        return (SDL_Keycode)('a' + (u - 0x04));
    if (u >= 0x1E && u <= 0x26)
        return (SDL_Keycode)('1' + (u - 0x1E));
    switch (u) {
    case 0x27: return '0';
    case 0x28: return '\r';
    case 0x29: return 27;
    case 0x2A: return '\b';
    case 0x2B: return '\t';
    case 0x2C: return ' ';
    case 0x2D: return '-';
    case 0x2E: return '=';
    case 0x2F: return '[';
    case 0x30: return ']';
    case 0x31: return '\\';
    case 0x33: return ';';
    case 0x34: return '\'';
    case 0x35: return '`';
    case 0x36: return ',';
    case 0x37: return '.';
    case 0x38: return '/';
    }
    return (SDL_Keycode)(u | 0x40000000u); /* SDL's keycode for a scancode */
}

static void keyboard_poll(void)
{
    static unsigned char down[256];
    unsigned char now[256];
    KbInfo info;
    KbData data;
    int i;

    if (ioKbGetInfo(&info) != 0 || !info.status[0])
        return;
    if (ioKbRead(0, &data) != 0)
        return;
    if (data.nb_keycode <= 0)
        return; /* nothing new */
    memset(now, 0, sizeof now);
    for (i = 0; i < data.nb_keycode && i < MAX_KEYCODES; i++) {
        unsigned const k = data.keycode[i] & 0xFF;

        if (k > 3)
            now[k] = 1;
    }
    /* The modifier keys come as bits: 0xE0 onwards in usage order (left
       Ctrl, Shift, Alt, GUI, then the right ones). */
    for (i = 0; i < 8; i++)
        now[0xE0 + i] = (unsigned char)((data.mkey._KbMkeyU.mkeys >> i) & 1);
    key_mod = (SDL_Keymod)((now[0xE0] ? SDL_KMOD_LCTRL : 0) | (now[0xE4] ? SDL_KMOD_RCTRL : 0)
        | (now[0xE1] ? SDL_KMOD_LSHIFT : 0) | (now[0xE5] ? SDL_KMOD_RSHIFT : 0)
        | (now[0xE2] ? SDL_KMOD_LALT : 0) | (now[0xE6] ? SDL_KMOD_RALT : 0));
    for (i = 4; i < 256; i++) {
        if (now[i] != down[i]) {
            SDL_Keycode const kc = keycode_of((unsigned)i);

            push_key((SDL_Scancode)i, kc, now[i]);
            if (now[i] && kc >= 0x20 && kc < 0x7F) {
                SDL_Event e;
                int const shift = (data.mkey._KbMkeyU.mkeys & 0x22) != 0;

                memset(&e, 0, sizeof e);
                e.type = SDL_EVENT_TEXT_INPUT;
                e.text.text[0] = (char)(shift && kc >= 'a' && kc <= 'z' ? kc - 32 : kc);
                push(&e);
            }
        }
    }
    memcpy(down, now, sizeof down);
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
    sysUtilCheckCallback();
    pads_poll();
    keyboard_poll();
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
        usleep(100);
    }
}

/* --- sound: an audio port, fed from a thread ------------------------------ */

#define DEVICE_RATE 48000
#define RING_BYTES (1 << 16)
#define PORT_BLOCKS 8

struct SDL_AudioStream {
    Uint8 ring[RING_BYTES];
    volatile unsigned head, tail; /* the emulator writes head, the thread tail */
    int channels, in_freq;
    int resample;
    zrs rs;
};

static struct SDL_AudioStream* the_stream; /* read and written atomically */
static volatile int audio_in_block;        /* the thread is reading a stream */
static u32 port_num;
static audioPortConfig port_config;
static sys_ppu_thread_t audio_thread_id;
static volatile int audio_running;

/* One 256-frame block of the port, from the ring: silence for what is
   missing. The port always plays stereo floats. */
static void fill_block(float* const out)
{
    struct SDL_AudioStream* s;
    int i;

    /* Say it is reading before taking the pointer; zs3_DestroyAudioStream
       clears the pointer before looking (both sequentially consistent), so
       either it sees this block under way or this block sees no stream. */
    __atomic_store_n(&audio_in_block, 1, __ATOMIC_SEQ_CST);
    s = __atomic_load_n(&the_stream, __ATOMIC_SEQ_CST);

    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float l = 0, r = 0;

        if (s && s->head - s->tail >= (unsigned)(2 * s->channels)) {
            Sint16 v[2];
            unsigned const t = s->tail % RING_BYTES;

            __sync_synchronize();
            memcpy(v, s->ring + t, (size_t)(2 * s->channels));
            l = (float)v[0] / 32768.0f;
            r = s->channels == 2 ? (float)v[1] / 32768.0f : l;
            s->tail += (unsigned)(2 * s->channels);
        }
        out[i * 2] = l;
        out[i * 2 + 1] = r;
    }
    __atomic_store_n(&audio_in_block, 0, __ATOMIC_SEQ_CST);
}

static void audio_thread(void* const arg)
{
    u64 const* const read_index = (u64 const*)(uintptr_t)port_config.readIndex;
    float* const data = (float*)(uintptr_t)port_config.audioDataStart;
    u64 w = (*read_index + 1) % PORT_BLOCKS;

    (void)arg;
    while (audio_running) {
        u64 const r = *read_index;
        /* Keep three blocks (16 ms) ahead of what the port is playing. */
        if ((w + PORT_BLOCKS - r) % PORT_BLOCKS < 3) {
            fill_block(data + (size_t)w * 2 * AUDIO_BLOCK_SAMPLES);
            w = (w + 1) % PORT_BLOCKS;
        } else {
            sysUsleep(1000);
        }
    }
    sysThreadExit(0);
}

static void audio_stop(void)
{
    if (audio_running) {
        u64 ret;

        audio_running = 0;
        sysThreadJoin(audio_thread_id, &ret);
        audioPortStop(port_num);
        audioPortClose(port_num);
        audioQuit();
    }
}

SDL_AudioStream* zs3_OpenAudioDeviceStream(SDL_AudioDeviceID const dev, SDL_AudioSpec const* const spec,
    SDL_AudioStreamCallback const cb, void* const userdata)
{
    struct SDL_AudioStream* s;
    audioPortParam params;

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
    if (!audio_running) {
        memset(&params, 0, sizeof params);
        params.numChannels = AUDIO_PORT_2CH;
        params.numBlocks = AUDIO_BLOCK_8;
        params.attrib = 0;
        params.level = 1.0f;
        if (audioInit() != 0 || audioPortOpen(&params, &port_num) != 0
            || audioGetPortConfig(port_num, &port_config) != 0) {
            set_error("Could not open the audio port");
            if (s->resample)
                zrs_free(&s->rs);
            free(s);
            return NULL;
        }
        memset((void*)(uintptr_t)port_config.audioDataStart, 0, port_config.portSize);
        audioPortStart(port_num);
        LOG("audio port %u: %u blocks at %08x, input %d Hz %d channels", (unsigned)port_num,
            (unsigned)port_config.numBlocks, (unsigned)port_config.audioDataStart, spec->freq, spec->channels);
        audio_running = 1;
        __atomic_store_n(&the_stream, s, __ATOMIC_SEQ_CST);
        if (sysThreadCreate(&audio_thread_id, audio_thread, NULL, 1000, 0x10000, THREAD_JOINABLE,
                (char*)"ZSNES sound")
            != 0) {
            audio_running = 0;
            audioPortStop(port_num);
            audioPortClose(port_num);
            set_error("Could not start the sound thread");
            __atomic_store_n(&the_stream, NULL, __ATOMIC_SEQ_CST);
            if (s->resample)
                zrs_free(&s->rs);
            free(s);
            return NULL;
        }
    }
    __atomic_store_n(&the_stream, s, __ATOMIC_SEQ_CST);
    return s;
}

bool zs3_ResumeAudioStreamDevice(SDL_AudioStream* const s) { return s != NULL; }

void zs3_DestroyAudioStream(SDL_AudioStream* const s)
{
    int spins = 0;

    if (!s || s != __atomic_load_n(&the_stream, __ATOMIC_SEQ_CST))
        return;
    LOG("audio stream closed");
    __atomic_store_n(&the_stream, NULL, __ATOMIC_SEQ_CST); /* silence from here */
    /* A block takes microseconds and calls nothing that blocks, so this ends
       as soon as the thread runs; freeing sooner could pull the ring out
       from under it. */
    while (__atomic_load_n(&audio_in_block, __ATOMIC_SEQ_CST)) {
        sysUsleep(10);
        spins++;
    }
    if (spins > 1000)
        LOG("audio: waited %d ms for the sound thread", spins / 100);
    if (s->resample)
        zrs_free(&s->rs);
    free(s);
}

static void ring_put(struct SDL_AudioStream* const s, Uint8 const* const buf, unsigned const len)
{
    unsigned const frame = 2u * (unsigned)s->channels;
    unsigned room = RING_BYTES - (s->head - s->tail);
    unsigned n = len < room ? len : room;
    unsigned h, first;

    n -= n % frame;
    h = s->head % RING_BYTES;
    first = RING_BYTES - h < n ? RING_BYTES - h : n;
    memcpy(s->ring + h, buf, first);
    memcpy(s->ring, buf + first, n - first);
    __sync_synchronize(); /* the samples before the head that shows them */
    s->head += n;
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
    n = s->head - s->tail;
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
        *frames = AUDIO_BLOCK_SAMPLES * 4;
    return true;
}

char const* zs3_GetCurrentAudioDriver(void) { return "PS3 audio"; }
int zs3_GetNumAudioDrivers(void) { return 0; }
char const* zs3_GetAudioDriver(int const index) { return NULL; }
