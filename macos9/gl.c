/* OpenGL for the Mac OS 9 build, through AGL and Apple's OpenGLLibrary.

   The library is imported weakly (macos9/implib): on a Mac without it every
   function below has a null address, macos9_gl_available() says so, and the
   frontend keeps to its software modes. With it, the frontend's immediate-mode
   renderer (unix/gl_draw.c) runs as it does elsewhere: the frame goes up as a
   texture and the graphics card scales and filters it to the window or the
   screen, which takes that work off the processor.

   The renderer uploads 5:6:5 pixels, an OpenGL 1.2 format. The OpenGL of
   Mac OS 9.0 and earlier is 1.1; there the uploads are converted to RGBA
   here first. */
#define ZSNES_MACOS9_GL_IMPL
#include <GL/gl.h>

#include <Quickdraw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macos9.h"

/* --- AGL, from Apple's agl.h ---------------------------------------------- */

typedef struct __AGLPixelFormatRec* AGLPixelFormat;
typedef struct __AGLContextRec* AGLContext;
typedef GDHandle AGLDevice;
typedef CGrafPtr AGLDrawable;

enum {
    AGL_NONE = 0,
    AGL_RGBA = 4,
    AGL_DOUBLEBUFFER = 5,
    AGL_NO_RECOVERY = 72,
    AGL_ACCELERATED = 73,
    AGL_SWAP_INTERVAL = 222
};

AGLPixelFormat aglChoosePixelFormat(AGLDevice const* gdevs, GLint ndev, GLint const* attribs);
void aglDestroyPixelFormat(AGLPixelFormat pix);
AGLContext aglCreateContext(AGLPixelFormat pix, AGLContext share);
GLboolean aglDestroyContext(AGLContext ctx);
GLboolean aglSetCurrentContext(AGLContext ctx);
GLboolean aglSetDrawable(AGLContext ctx, AGLDrawable draw);
GLboolean aglSetInteger(AGLContext ctx, GLenum pname, GLint const* params);
void aglSwapBuffers(AGLContext ctx);
GLboolean aglUpdateContext(AGLContext ctx);
GLenum aglGetError(void);

/* --- is it there ---------------------------------------------------------- */

/* A weak import's address is null when the library is missing. Read through
   a volatile so the compiler cannot assume a function's address non-null. */
static int present(void (*fn)(void))
{
    void (*volatile p)(void) = fn;

    return p != NULL;
}

int macos9_gl_available(void)
{
    static int known = -1;

    if (known < 0) {
        known = present((void (*)(void))aglChoosePixelFormat)
            && present((void (*)(void))aglCreateContext)
            && present((void (*)(void))aglSetDrawable)
            && present((void (*)(void))aglSwapBuffers)
            && present((void (*)(void))glTexSubImage2D)
            && present((void (*)(void))glBegin);
        if (!known)
            printf("OpenGL: not installed; the OpenGL video modes are off\n");
    }
    return known;
}

/* --- the context ---------------------------------------------------------- */

static AGLContext ctx;
static int packed_565; /* the driver takes GL_UNSIGNED_SHORT_5_6_5 */

void* macos9_gl_create(void* const port, int const vsync)
{
    static GLint const accel[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_ACCELERATED, AGL_NO_RECOVERY, AGL_NONE };
    AGLPixelFormat pf;
    char const* v;
    char const* ext;

    if (!macos9_gl_available())
        return NULL;
    /* Only a renderer on the graphics card: Apple's software one is slower
       than ZSNES's own software modes. */
    pf = aglChoosePixelFormat(NULL, 0, accel);
    if (!pf) {
        printf("OpenGL: no accelerated renderer for this screen (AGL error %u)\n", (unsigned)aglGetError());
        return NULL;
    }
    ctx = aglCreateContext(pf, NULL);
    aglDestroyPixelFormat(pf);
    if (!ctx) {
        printf("OpenGL: could not create a context (AGL error %u)\n", (unsigned)aglGetError());
        return NULL;
    }
    if (!aglSetDrawable(ctx, (AGLDrawable)port) || !aglSetCurrentContext(ctx)) {
        printf("OpenGL: could not attach to the window (AGL error %u)\n", (unsigned)aglGetError());
        aglDestroyContext(ctx);
        ctx = NULL;
        return NULL;
    }
    macos9_gl_set_vsync(vsync);

    v = (char const*)glGetString(GL_VERSION);
    ext = (char const*)glGetString(GL_EXTENSIONS);
    {
        int major = 1, minor = 0;
        if (v)
            sscanf(v, "%d.%d", &major, &minor);
        packed_565 = major > 1 || (major == 1 && minor >= 2) || (ext && strstr(ext, "GL_APPLE_packed_pixel"));
    }
    printf("OpenGL: %s, %s%s\n", (char const*)glGetString(GL_RENDERER), v ? v : "?",
        packed_565 ? "" : " (converting 5:6:5 uploads)");
    return ctx;
}

void macos9_gl_set_vsync(int const on)
{
    GLint const v = on ? 1 : 0;

    if (ctx)
        aglSetInteger(ctx, AGL_SWAP_INTERVAL, &v);
}

void macos9_gl_destroy(void)
{
    if (ctx) {
        aglSetCurrentContext(NULL);
        aglSetDrawable(ctx, NULL);
        aglDestroyContext(ctx);
        ctx = NULL;
    }
}

/* After the window moves or changes size. */
void macos9_gl_update(void)
{
    if (ctx)
        aglUpdateContext(ctx);
}

void macos9_gl_swap(void)
{
    if (ctx)
        aglSwapBuffers(ctx);
}

/* --- uploads -------------------------------------------------------------- */

static GLint row_length, skip_pixels, skip_rows;

void zgl_PixelStorei(GLenum const pname, GLint const param)
{
    if (pname == GL_UNPACK_ROW_LENGTH)
        row_length = param;
    else if (pname == GL_UNPACK_SKIP_PIXELS)
        skip_pixels = param;
    else if (pname == GL_UNPACK_SKIP_ROWS)
        skip_rows = param;
    glPixelStorei(pname, param);
}

/* 5:6:5 to RGBA bytes, honouring the unpack state the caller set, which the
   converted copy then no longer needs. */
static GLubyte const* to_rgba(GLsizei const w, GLsizei const h, GLvoid const* const pixels)
{
    static GLubyte* buf;
    static size_t cap;
    size_t const need = (size_t)w * (size_t)h * 4;
    GLint const stride = row_length ? row_length : w;
    GLsizei x, y;

    if (need > cap) {
        free(buf);
        buf = (GLubyte*)malloc(need);
        cap = buf ? need : 0;
        if (!buf)
            return NULL;
    }
    for (y = 0; y < h; y++) {
        GLushort const* in = (GLushort const*)pixels + (size_t)(y + skip_rows) * stride + skip_pixels;
        GLubyte* out = buf + (size_t)y * w * 4;

        for (x = 0; x < w; x++) {
            GLushort const p = in[x];
            out[0] = (GLubyte)((p >> 8 & 0xF8) | p >> 13);
            out[1] = (GLubyte)((p >> 3 & 0xFC) | (p >> 9 & 3));
            out[2] = (GLubyte)((p << 3 & 0xF8) | (p >> 2 & 7));
            out[3] = 0xFF;
            out += 4;
        }
    }
    return buf;
}

static void unpack_plain(int const plain)
{
    glPixelStorei(GL_UNPACK_ROW_LENGTH, plain ? 0 : row_length);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, plain ? 0 : skip_pixels);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, plain ? 0 : skip_rows);
}

void zgl_TexImage2D(GLenum const target, GLint const level, GLint const internal, GLsizei const w,
    GLsizei const h, GLint const border, GLenum const format, GLenum const type, GLvoid const* const pixels)
{
    if (type == GL_UNSIGNED_SHORT_5_6_5 && !packed_565 && pixels) {
        GLubyte const* const rgba = to_rgba(w, h, pixels);

        if (!rgba)
            return;
        unpack_plain(1);
        glTexImage2D(target, level, internal, w, h, border, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        unpack_plain(0);
        return;
    }
    glTexImage2D(target, level, internal, w, h, border, format, type, pixels);
}

void zgl_TexSubImage2D(GLenum const target, GLint const level, GLint const x, GLint const y,
    GLsizei const w, GLsizei const h, GLenum const format, GLenum const type, GLvoid const* const pixels)
{
    if (type == GL_UNSIGNED_SHORT_5_6_5 && !packed_565) {
        GLubyte const* const rgba = to_rgba(w, h, pixels);

        if (!rgba)
            return;
        unpack_plain(1);
        glTexSubImage2D(target, level, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        unpack_plain(0);
        return;
    }
    glTexSubImage2D(target, level, x, y, w, h, format, type, pixels);
}

/* The window was made anew (a mode change that kept the context). */
void macos9_gl_attach(void* const port)
{
    if (ctx) {
        aglSetDrawable(ctx, (AGLDrawable)port);
        aglSetCurrentContext(ctx);
        aglUpdateContext(ctx);
    }
}
