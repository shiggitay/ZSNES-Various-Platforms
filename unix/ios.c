/* ios.c - the iOS build's additions to the SDL3 frontend: an on-screen SNES
   controller, touches as the menus' pointer, game controllers as player 1
   without setting up, and the sandbox's folders. See ios.h. */
#include "ios.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../cpu/execute.h" /* pressed[] */
#include "input.h"          /* player 1's keys */

extern unsigned char GUIOn, GUIOn2;
extern unsigned char FPSOn;     /* video/procvid.c */
extern unsigned char frameskip; /* 0 = automatic; the counter needs it */
void SaveSramData(void);
void GUISaveVars(void);

/* --- folders -------------------------------------------------------------- */

void ios_setup(void)
{
    char const* const home = getenv("HOME"); /* the app's sandbox */
    char path[1024];

    if (home) {
        /* Settings where the user does not see them; ROMs, battery saves and
           save states in Documents, which the Files app shows (Info.plist:
           UIFileSharingEnabled, LSSupportsOpeningDocumentsInPlace). */
        snprintf(path, sizeof path, "%s/Library/Application Support", home);
        setenv("XDG_CONFIG_HOME", path, 1);
        snprintf(path, sizeof path, "%s/Documents", home);
        if (chdir(path) != 0)
            perror(path);
    }
    /* Touches are handled here, not turned into mouse clicks by SDL. */
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    /* Sound plays with the ring/silent switch set to silent, as games do. */
    SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    /* The home indicator fades after a moment instead of staying lit. */
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
}

/* As on quitting: the game's battery save (it checks a game is loaded) and
   the settings. */
void ios_background(void)
{
    SaveSramData();
    GUISaveVars();
}

/* --- layout --------------------------------------------------------------- */

enum { C_DPAD, C_A, C_B, C_X, C_Y, C_L, C_R, C_SELECT, C_START, C_MENU, C_FPS, C_COUNT };

typedef struct {
    float x, y;   /* centre, output pixels */
    float rx, ry; /* half width and height; round buttons have rx == ry */
    int round;
    char const* label;
} Control;

static Control ctl[C_COUNT];
static float out_w, out_h;
static SDL_FRect picture;

/* The screen's edges the system keeps for itself (the notch, the home
   indicator), in output pixels. */
static void safe_insets(SDL_Renderer* const r, float* const left, float* const right, float* const bottom)
{
    SDL_Window* const win = SDL_GetRenderWindow(r);
    SDL_Rect safe;
    int ww = 0, wh = 0;

    *left = *right = *bottom = 0;
    if (win && SDL_GetWindowSafeArea(win, &safe) && SDL_GetWindowSize(win, &ww, &wh) && ww > 0 && wh > 0) {
        float const sx = out_w / (float)ww, sy = out_h / (float)wh;

        *left = (float)safe.x * sx;
        *right = (float)(ww - safe.x - safe.w) * sx;
        *bottom = (float)(wh - safe.y - safe.h) * sy;
    }
}

static void place(Control* const c, float const x, float const y, float const rx, float const ry, int const round,
    char const* const label)
{
    c->x = x;
    c->y = y;
    c->rx = rx;
    c->ry = ry;
    c->round = round;
    c->label = label;
}

static void layout(SDL_Renderer* const r)
{
    int w = 0, h = 0;
    float il, ir, ib, u, lx, rx, cy;

    SDL_GetCurrentRenderOutputSize(r, &w, &h);
    if ((float)w == out_w && (float)h == out_h)
        return;
    out_w = (float)w;
    out_h = (float)h;
    safe_insets(r, &il, &ir, &ib);
    u = out_h; /* sizes follow the screen's height */
    /* The picture: 4:3 and as tall as the screen, or as wide as it on a
       screen narrower than that (an iPad in some sizes). */
    picture.h = out_h;
    picture.w = out_h * 4.0f / 3.0f;
    if (picture.w > out_w) {
        picture.w = out_w;
        picture.h = out_w * 3.0f / 4.0f;
    }
    picture.x = (out_w - picture.w) / 2;
    picture.y = (out_h - picture.h) / 2;

    /* The controls go in the margins beside the picture, sized to them:
       full size beside a notched iPhone's picture, smaller on a 16:9 one
       (an iPhone 8 or SE), where they also reach a little over it. */
    {
        float const avail_l = picture.x - il, avail_r = out_w - ir - (picture.x + picture.w);
        float const avail = avail_l < avail_r ? avail_l : avail_r;
        float k = avail / (2.0f * 0.19f * u);
        float half;

        k = k > 1.0f ? 1.0f : k < 0.75f ? 0.75f : k;
        half = 0.19f * u * k; /* half the width of the d-pad or the buttons */
        lx = il + (avail_l / 2 > half + 0.01f * u ? avail_l / 2 : half + 0.01f * u);
        rx = out_w - ir - (avail_r / 2 > half + 0.01f * u ? avail_r / 2 : half + 0.01f * u);
        cy = out_h - ib - 0.36f * u;
        place(&ctl[C_DPAD], lx, cy, 0.17f * u * k, 0.17f * u * k, 1, NULL);
        place(&ctl[C_A], rx + 0.115f * u * k, cy, 0.068f * u * k, 0.068f * u * k, 1, "A");
        place(&ctl[C_B], rx, cy + 0.115f * u * k, 0.068f * u * k, 0.068f * u * k, 1, "B");
        place(&ctl[C_X], rx, cy - 0.115f * u * k, 0.068f * u * k, 0.068f * u * k, 1, "X");
        place(&ctl[C_Y], rx - 0.115f * u * k, cy, 0.068f * u * k, 0.068f * u * k, 1, "Y");
        place(&ctl[C_L], lx, 0.09f * u, 0.11f * u * k, 0.045f * u, 0, "L");
        place(&ctl[C_R], rx, 0.09f * u, 0.11f * u * k, 0.045f * u, 0, "R");
        place(&ctl[C_SELECT], lx, out_h - ib - 0.07f * u, 0.085f * u * k, 0.035f * u, 0, "SELECT");
        place(&ctl[C_START], rx, out_h - ib - 0.07f * u, 0.085f * u * k, 0.035f * u, 0, "START");
    }
    place(&ctl[C_MENU], out_w / 2 - 0.085f * u, 0.045f * u, 0.07f * u, 0.03f * u, 0, "MENU");
    place(&ctl[C_FPS], out_w / 2 + 0.085f * u, 0.045f * u, 0.07f * u, 0.03f * u, 0, "FPS");
}

void ios_picture_rect(SDL_Renderer* const r, SDL_FRect* const dst)
{
    layout(r);
    *dst = picture;
}

/* --- presses -------------------------------------------------------------- */

/* Player 1's key for each button, whatever Config > Input has made it. */
static unsigned key_of(int const c)
{
    switch (c) {
    case C_A: return pl1Ak;
    case C_B: return pl1Bk;
    case C_X: return pl1Xk;
    case C_Y: return pl1Yk;
    case C_L: return pl1Lk;
    case C_R: return pl1Rk;
    case C_SELECT: return pl1selk;
    case C_START: return pl1startk;
    }
    return 0;
}

enum { D_UP = 1, D_DOWN = 2, D_LEFT = 4, D_RIGHT = 8 };

/* Every source of a press counts separately - each finger, the controller -
   and a key is down while any of them holds it. */
enum { SRC_TOUCH, SRC_PAD, SRC_STICK, SRC_COUNT };
static unsigned char held[SRC_COUNT][C_COUNT];
static int pointer_mode; /* the menus were last moved through with the stick */
static unsigned char dirs[SRC_COUNT];

static void set_key(unsigned const k, int const down)
{
    if (k && k < sizeof pressed)
        pressed[k] = (unsigned char)(down != 0);
}

static void refresh(void)
{
    int c, s;
    unsigned d = 0;

    for (c = C_A; c <= C_START; c++) {
        int down = 0;

        for (s = 0; s < SRC_COUNT; s++)
            down |= held[s][c];
        set_key(key_of(c), down);
    }
    for (s = 0; s < SRC_COUNT; s++)
        d |= dirs[s];
    set_key(pl1upk, d & D_UP);
    set_key(pl1downk, d & D_DOWN);
    set_key(pl1leftk, d & D_LEFT);
    set_key(pl1rightk, d & D_RIGHT);
}

/* A key as the keyboard would send it, through the frontend's own handling
   (so the menus see it as typed, too). */
static void push_key(SDL_Scancode const sc, SDL_Keycode const key, int const down)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.scancode = sc;
    e.key.key = key;
    e.key.down = down != 0;
    SDL_PushEvent(&e);
}

/* Esc: in a game it opens the ZSNES menu, in the menu it goes back. */
static void press_menu(int const down) { push_key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, down); }

/* A left click at the menus' pointer. */
static void push_click(int const down)
{
    SDL_Event e;

    memset(&e, 0, sizeof e);
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.down = down != 0;
    e.button.clicks = 1;
    SDL_PushEvent(&e);
}

static int in_menus(void) { return GUIOn || GUIOn2; }

/* --- touches -------------------------------------------------------------- */

#define FINGERS 10
static struct {
    SDL_FingerID id;
    int used;
    int control; /* C_COUNT: pointing at the menus */
} finger[FINGERS];

/* With a game controller connected (Bluetooth: MFi, Xbox, PlayStation)
   nothing is drawn over the game; it comes back when the controller goes. */
static int overlay_shown(void) { return !SDL_HasGamepad(); }

static int hit(Control const* const c, float const x, float const y, float const slack)
{
    float const dx = (x - c->x) / (c->rx * slack), dy = (y - c->y) / (c->ry * slack);

    return c->round ? dx * dx + dy * dy <= 1.0f : fabsf(dx) <= 1.0f && fabsf(dy) <= 1.0f;
}

static int control_at(float const x, float const y)
{
    int c;

    if (!overlay_shown()) {
        /* The controller's Start+Select (or Home) opens the menu; a tap at
           the top middle, where MENU was, still does as well. */
        Control const* const m = &ctl[C_MENU];

        return y < m->y + 2 * m->ry && x > out_w * 0.35f && x < out_w * 0.65f ? C_MENU : C_COUNT;
    }
    if (hit(&ctl[C_MENU], x, y, 1.3f))
        return C_MENU;
    if (hit(&ctl[C_FPS], x, y, 1.3f))
        return C_FPS;
    if (GUIOn || GUIOn2)
        return C_COUNT;
    if (hit(&ctl[C_DPAD], x, y, 1.35f))
        return C_DPAD;
    for (c = C_A; c <= C_START; c++)
        if (hit(&ctl[c], x, y, c <= C_Y ? 1.25f : 1.4f))
            return c;
    return C_COUNT;
}

/* Eight ways, by the angle from the pad's centre, with a dead spot in the
   middle. */
static unsigned dpad_dirs(float const x, float const y)
{
    Control const* const d = &ctl[C_DPAD];
    float const dx = x - d->x, dy = y - d->y;
    float a;
    unsigned v = 0;

    if (dx * dx + dy * dy < (0.22f * d->rx) * (0.22f * d->rx))
        return 0;
    a = atan2f(-dy, dx) * (180.0f / 3.14159265f); /* 0 right, 90 up */
    if (a < 0)
        a += 360.0f;
    if (a < 67.5f || a >= 292.5f)
        v |= D_RIGHT;
    if (a >= 22.5f && a < 157.5f)
        v |= D_UP;
    if (a >= 112.5f && a < 247.5f)
        v |= D_LEFT;
    if (a >= 202.5f && a < 337.5f)
        v |= D_DOWN;
    return v;
}

/* The d-pad and the buttons from all the fingers now down. */
static float fx_pos[FINGERS], fy_pos[FINGERS];

static void touch_state(void)
{
    int i;

    memset(held[SRC_TOUCH], 0, sizeof held[SRC_TOUCH]);
    dirs[SRC_TOUCH] = 0;
    for (i = 0; i < FINGERS; i++) {
        if (!finger[i].used)
            continue;
        if (finger[i].control == C_DPAD)
            dirs[SRC_TOUCH] |= (unsigned char)dpad_dirs(fx_pos[i], fy_pos[i]);
        else if (finger[i].control < C_MENU)
            held[SRC_TOUCH][finger[i].control] = 1;
    }
    refresh();
}

int ios_touch(SDL_Event const* const e, float* const fx, float* const fy, int* const phase)
{
    float const x = e->tfinger.x * out_w, y = e->tfinger.y * out_h;
    int i, slot = -1;

    for (i = 0; i < FINGERS; i++)
        if (finger[i].used && finger[i].id == e->tfinger.fingerID)
            slot = i;

    if (e->type == SDL_EVENT_FINGER_DOWN) {
        if (slot < 0) {
            for (i = 0; i < FINGERS && slot < 0; i++)
                if (!finger[i].used)
                    slot = i;
            if (slot < 0)
                return 0;
        }
        finger[slot].used = 1;
        finger[slot].id = e->tfinger.fingerID;
        finger[slot].control = control_at(x, y);
        fx_pos[slot] = x;
        fy_pos[slot] = y;
        if (finger[slot].control == C_MENU) {
            press_menu(1);
            return 0;
        }
        if (finger[slot].control == C_FPS) {
            /* The frame counter, as Command-F or Ctrl-F do on the other
               ports. ZSNES counts frames only while it picks the frameskip. */
            frameskip = 0;
            FPSOn ^= 1;
            return 0;
        }
        if (finger[slot].control < C_COUNT) {
            touch_state();
            return 0;
        }
        *phase = 1;
    } else if (slot < 0) {
        return 0;
    } else if (e->type == SDL_EVENT_FINGER_MOTION) {
        fx_pos[slot] = x;
        fy_pos[slot] = y;
        if (finger[slot].control < C_COUNT) {
            /* A thumb may slide between the face buttons. */
            if (finger[slot].control >= C_A && finger[slot].control <= C_Y) {
                int const c = control_at(x, y);

                if (c >= C_A && c <= C_Y)
                    finger[slot].control = c;
            }
            touch_state();
            return 0;
        }
        *phase = -1;
    } else { /* up, or cancelled by the system */
        int const c = finger[slot].control;

        finger[slot].used = 0;
        if (c == C_MENU) {
            press_menu(0);
            return 0;
        }
        if (c == C_FPS)
            return 0;
        if (c < C_COUNT) {
            touch_state();
            return 0;
        }
        *phase = 0;
    }
    /* A finger pointing at the menus: where in the picture. */
    if (picture.w <= 0 || picture.h <= 0)
        return 0;
    *fx = (x - picture.x) / picture.w;
    *fy = (y - picture.y) / picture.h;
    *fx = *fx < 0 ? 0 : *fx > 1 ? 1 : *fx;
    *fy = *fy < 0 ? 0 : *fy > 1 ? 1 : *fy;
    return 1;
}

/* --- game controllers ----------------------------------------------------- */

/* The face buttons. A Nintendo controller (a Switch Pro Controller, Joy-Con)
   has the SNES's letters in the SNES's places, so it goes by its letters,
   which keeps A as A however the system reports the buttons' positions.
   Any other (Xbox, PlayStation, MFi) goes by position, as a SNES pad's
   buttons sit: bottom B, right A, left Y, top X. */
static int face_button(SDL_JoystickID const which, int const button)
{
    SDL_Gamepad* const pad = SDL_GetGamepadFromID(which);

    if (pad) {
        switch (SDL_GetGamepadType(pad)) {
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
            switch (SDL_GetGamepadButtonLabel(pad, (SDL_GamepadButton)button)) {
            case SDL_GAMEPAD_BUTTON_LABEL_A: return C_A;
            case SDL_GAMEPAD_BUTTON_LABEL_B: return C_B;
            case SDL_GAMEPAD_BUTTON_LABEL_X: return C_X;
            case SDL_GAMEPAD_BUTTON_LABEL_Y: return C_Y;
            default: break;
            }
            break;
        default:
            break;
        }
    }
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return C_B;
    case SDL_GAMEPAD_BUTTON_EAST: return C_A;
    case SDL_GAMEPAD_BUTTON_WEST: return C_Y;
    case SDL_GAMEPAD_BUTTON_NORTH: return C_X;
    }
    return -1;
}

void ios_gamepad_button(SDL_JoystickID const which, int const button, int const down)
{
    int c = -1;
    unsigned char d = 0;

    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_WEST:
    case SDL_GAMEPAD_BUTTON_NORTH:
        c = face_button(which, button);
        if (c < 0)
            return;
        break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: c = C_L; break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: c = C_R; break;
    case SDL_GAMEPAD_BUTTON_BACK: c = C_SELECT; break;
    case SDL_GAMEPAD_BUTTON_START: c = C_START; break;
    case SDL_GAMEPAD_BUTTON_GUIDE: press_menu(down); return;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: d = D_UP; break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: d = D_DOWN; break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: d = D_LEFT; break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: d = D_RIGHT; break;
    default: return;
    }
    /* In the ZSNES menus: A picks and B goes back. A picks the way the menus
       were last moved through - after the left stick, it clicks where the
       pointer is; after the d-pad, it is Return on what is highlighted -
       so it never does both at once. What a press started, its release
       ends, wherever the menus are by then. */
    if (c == C_A || c == C_B) {
        static int action[2]; /* 0 the game's button, 1 Return or Esc, 2 click */
        int* const a = &action[c == C_B];

        if (down) {
            *a = !in_menus() ? 0 : c == C_B ? 1 : pointer_mode ? 2 : 1;
        }
        if (*a == 1 && c == C_A) {
            push_key(SDL_SCANCODE_RETURN, SDLK_RETURN, down);
        } else if (*a == 1) {
            press_menu(down);
        } else if (*a == 2) {
            push_click(down);
        }
        if (*a) {
            held[SRC_PAD][c] = 0;
            if (!down)
                *a = 0;
            refresh();
            return;
        }
    }
    if (d && down && in_menus())
        pointer_mode = 0; /* the d-pad moves the highlight */

    if (c >= 0)
        held[SRC_PAD][c] = (unsigned char)(down != 0);
    else if (down)
        dirs[SRC_PAD] |= d;
    else
        dirs[SRC_PAD] &= (unsigned char)~d;

    /* Start+Select together: the ZSNES menu, held as Esc until either is
       let go, and neither reaches the game meanwhile. A controller's Home
       button may be kept by iOS for itself, so this is the sure way. */
    {
        static unsigned char start, select, chord;

        if (button == SDL_GAMEPAD_BUTTON_START)
            start = (unsigned char)(down != 0);
        if (button == SDL_GAMEPAD_BUTTON_BACK)
            select = (unsigned char)(down != 0);
        if (start && select && !chord) {
            chord = 1;
            press_menu(1);
        } else if (chord && !(start && select)) {
            chord = 0;
            press_menu(0);
        }
        if (chord || (start && select)) {
            held[SRC_PAD][C_START] = 0;
            held[SRC_PAD][C_SELECT] = 0;
        }
        if (chord && !start)
            held[SRC_PAD][C_SELECT] = 0; /* until both are up again */
        if (chord && !select)
            held[SRC_PAD][C_START] = 0;
    }
    /* In the menus Start and Select are only the chord: SNES Start is the
       Return key by default, and would pick menu items otherwise. */
    if (in_menus()) {
        held[SRC_PAD][C_START] = 0;
        held[SRC_PAD][C_SELECT] = 0;
    }
    refresh();
}

/* The left stick: the d-pad in a game (past half way), the pointer in the
   menus (ios_pointer_motion). */
static float stick_x, stick_y;

static void stick_dirs(void)
{
    unsigned char d = 0;

    if (!in_menus()) {
        if (stick_x < -0.5f)
            d |= D_LEFT;
        else if (stick_x > 0.5f)
            d |= D_RIGHT;
        if (stick_y < -0.5f)
            d |= D_UP;
        else if (stick_y > 0.5f)
            d |= D_DOWN;
    }
    if (d != dirs[SRC_STICK]) {
        dirs[SRC_STICK] = d;
        refresh();
    }
}

void ios_gamepad_axis(int const axis, int const value)
{
    float const v = (float)value / 32767.0f;

    if (axis == SDL_GAMEPAD_AXIS_LEFTX)
        stick_x = v;
    else if (axis == SDL_GAMEPAD_AXIS_LEFTY)
        stick_y = v;
    else
        return;
    stick_dirs();
}

int ios_pointer_motion(float* const dx, float* const dy)
{
    static Uint64 last;
    Uint64 const now = SDL_GetTicksNS();
    float const dt = last ? (float)(now - last) / 1e9f : 0.0f;
    float const m = sqrtf(stick_x * stick_x + stick_y * stick_y);
    float speed;

    last = now;
    /* Only with a controller connected: one that went away mid-tilt leaves
       nothing behind, and touches and keys work as they always do. */
    if (!SDL_HasGamepad()) {
        stick_x = stick_y = 0.0f;
        pointer_mode = 0;
    }
    stick_dirs(); /* the menus may have opened or closed under a held stick */
    if (!in_menus() || m < 0.2f || dt <= 0.0f || dt > 0.25f)
        return 0;
    pointer_mode = 1;
    /* Slow near the middle for aiming, about a picture's width in a second
       at full tilt. */
    speed = (m - 0.2f) / 0.8f;
    speed = speed * speed * 1.1f / m;
    *dx = stick_x * speed * dt;
    *dy = stick_y * speed * dt * (4.0f / 3.0f); /* the picture is 4:3 */
    return 1;
}

/* --- drawing -------------------------------------------------------------- */

#define SEGS 32

static void fill_ellipse(SDL_Renderer* const r, float const x, float const y, float const rx, float const ry,
    SDL_FColor const col)
{
    SDL_Vertex v[SEGS + 2];
    int idx[SEGS * 3];
    int i;

    memset(v, 0, sizeof v);
    v[0].position.x = x;
    v[0].position.y = y;
    v[0].color = col;
    for (i = 0; i <= SEGS; i++) {
        float const a = (float)i * (2.0f * 3.14159265f / SEGS);

        v[i + 1].position.x = x + rx * cosf(a);
        v[i + 1].position.y = y + ry * sinf(a);
        v[i + 1].color = col;
    }
    for (i = 0; i < SEGS; i++) {
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(r, NULL, v, SEGS + 2, idx, SEGS * 3);
}

/* A rounded bar: a rectangle with a half circle at each end. */
static void fill_pill(SDL_Renderer* const r, Control const* const c, SDL_FColor const col)
{
    float const rad = c->ry;
    SDL_FRect const mid = { c->x - c->rx + rad, c->y - c->ry, 2 * (c->rx - rad), 2 * c->ry };

    SDL_SetRenderDrawColorFloat(r, col.r, col.g, col.b, col.a);
    SDL_RenderFillRect(r, &mid);
    fill_ellipse(r, c->x - c->rx + rad, c->y, rad, rad, col);
    fill_ellipse(r, c->x + c->rx - rad, c->y, rad, rad, col);
}

/* SDL's built-in 8-pixel font, scaled to `size` pixels and centred. */
static void label(SDL_Renderer* const r, char const* const s, float const x, float const y, float const size)
{
    float const k = size / 8.0f;
    float const w = (float)strlen(s) * 8.0f * k;

    SDL_SetRenderScale(r, k, k);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 200);
    SDL_RenderDebugText(r, (x - w / 2) / k, (y - size / 2) / k, s);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
}

void ios_draw_overlay(SDL_Renderer* const r)
{
    SDL_FColor const idle = { 1.0f, 1.0f, 1.0f, 0.16f }, lit = { 1.0f, 1.0f, 1.0f, 0.42f };
    SDL_FColor const edge = { 0.0f, 0.0f, 0.0f, 0.25f };
    int c;

    layout(r);
    if (!overlay_shown())
        return; /* a controller is connected */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    fill_pill(r, &ctl[C_MENU], idle);
    label(r, "MENU", ctl[C_MENU].x, ctl[C_MENU].y, ctl[C_MENU].ry * 0.9f);
    fill_pill(r, &ctl[C_FPS], FPSOn ? lit : idle);
    label(r, "FPS", ctl[C_FPS].x, ctl[C_FPS].y, ctl[C_FPS].ry * 0.9f);
    if (GUIOn || GUIOn2) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        return;
    }

    /* The d-pad: a cross on a disc, each arm lit while held. */
    {
        Control const* const d = &ctl[C_DPAD];
        unsigned const on = dirs[SRC_TOUCH] | dirs[SRC_PAD] | dirs[SRC_STICK];
        float const arm = d->rx * 0.36f, len = d->rx * 0.92f;
        SDL_FRect const up = { d->x - arm, d->y - len, 2 * arm, len - arm };
        SDL_FRect const down = { d->x - arm, d->y + arm, 2 * arm, len - arm };
        SDL_FRect const left = { d->x - len, d->y - arm, len - arm, 2 * arm };
        SDL_FRect const right = { d->x + arm, d->y - arm, len - arm, 2 * arm };
        SDL_FRect const mid = { d->x - arm, d->y - arm, 2 * arm, 2 * arm };

        fill_ellipse(r, d->x, d->y, d->rx, d->ry, edge);
        SDL_SetRenderDrawColorFloat(r, idle.r, idle.g, idle.b, idle.a);
        SDL_RenderFillRect(r, &mid);
#define ARM(rect, bit)                                                                                       \
    do {                                                                                                     \
        SDL_FColor const k = (on & (bit)) ? lit : idle;                                                      \
        SDL_SetRenderDrawColorFloat(r, k.r, k.g, k.b, k.a);                                                  \
        SDL_RenderFillRect(r, &(rect));                                                                      \
    } while (0)
        ARM(up, D_UP);
        ARM(down, D_DOWN);
        ARM(left, D_LEFT);
        ARM(right, D_RIGHT);
#undef ARM
    }
    for (c = C_A; c <= C_START; c++) {
        Control const* const k = &ctl[c];
        int const on = held[SRC_TOUCH][c] | held[SRC_PAD][c];

        if (k->round)
            fill_ellipse(r, k->x, k->y, k->rx, k->ry, on ? lit : idle);
        else
            fill_pill(r, k, on ? lit : idle);
        label(r, k->label, k->x, k->y, k->round ? k->ry * 0.8f : k->ry * 0.85f);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}
