/* OpenGL for the Mac OS 9 build, through AGL and Apple's OpenGLLibrary.

   The library is imported weakly (macos9/implib): on a Mac without it every
   function below has a null address, macos9_gl_available() says so, and the
   frontend keeps to its software modes. With it, the frontend's immediate-mode
   renderer (unix/gl_draw.c) runs as it does elsewhere: the frame goes up as a
   texture and the graphics card scales and filters it to the window or the
   screen, which takes that work off the processor.

   Texture uploads go through unix/glupload.c, which repacks them and
   converts them for an OpenGL older than 1.2. */
#define ZGL_IMPL
#include <GL/gl.h>

#include <Quickdraw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macos9.h"
#include "../unix/glupload.h"

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

/* The frontend's name for the question (unix/sdllink.c). */
int zsnes_gl_available(void) { return macos9_gl_available(); }

/* --- the context ---------------------------------------------------------- */

static AGLContext ctx;

void* macos9_gl_create(void* const port, int const vsync)
{
    static GLint const accel[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_ACCELERATED, AGL_NO_RECOVERY, AGL_NONE };
    AGLPixelFormat pf;

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

    zgl_detect();
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

/* The window was made anew (a mode change that kept the context). */
void macos9_gl_attach(void* const port)
{
    if (ctx) {
        aglSetDrawable(ctx, (AGLDrawable)port);
        aglSetCurrentContext(ctx);
        aglUpdateContext(ctx);
    }
}
