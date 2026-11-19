/* OpenGL for the SDL 1.2 build, found at run time.

   The SDL 1.2 build does not link libGL: a Yellow Dog Linux, or any other
   machine of its age, may have no OpenGL, or only Mesa's software one, and the
   emulator has to start regardless. SDL 1.2 loads the library when asked for
   an OpenGL window; each GL function the renderer calls (the subset in
   unix/glcompat/GL/gl.h) is a forwarder to the address SDL looks up there.
   Generated from the header's prototypes. */
#define ZGL_IMPL
#include <GL/gl.h>

#include <SDL.h>

#include <stdio.h>
#include <string.h>

int zsnes_sdl12_gl_load(void);

static void (*p_glBegin)(GLenum mode);
void glBegin(GLenum mode) { p_glBegin(mode); }
static void (*p_glBindTexture)(GLenum target, GLuint texture);
void glBindTexture(GLenum target, GLuint texture) { p_glBindTexture(target, texture); }
static void (*p_glBlendFunc)(GLenum sfactor, GLenum dfactor);
void glBlendFunc(GLenum sfactor, GLenum dfactor) { p_glBlendFunc(sfactor, dfactor); }
static void (*p_glClear)(GLbitfield mask);
void glClear(GLbitfield mask) { p_glClear(mask); }
static void (*p_glClearColor)(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { p_glClearColor(r, g, b, a); }
static void (*p_glDeleteTextures)(GLsizei n, GLuint const* textures);
void glDeleteTextures(GLsizei n, GLuint const* textures) { p_glDeleteTextures(n, textures); }
static void (*p_glDisable)(GLenum cap);
void glDisable(GLenum cap) { p_glDisable(cap); }
static void (*p_glEnable)(GLenum cap);
void glEnable(GLenum cap) { p_glEnable(cap); }
static void (*p_glEnd)(void);
void glEnd(void) { p_glEnd(); }
static void (*p_glFinish)(void);
void glFinish(void) { p_glFinish(); }
static void (*p_glFlush)(void);
void glFlush(void) { p_glFlush(); }
static void (*p_glGenTextures)(GLsizei n, GLuint* textures);
void glGenTextures(GLsizei n, GLuint* textures) { p_glGenTextures(n, textures); }
static GLenum (*p_glGetError)(void);
GLenum glGetError(void) { return p_glGetError(); }
static GLubyte const* (*p_glGetString)(GLenum name);
GLubyte const* glGetString(GLenum name) { return p_glGetString(name); }
static void (*p_glLoadIdentity)(void);
void glLoadIdentity(void) { p_glLoadIdentity(); }
static void (*p_glMatrixMode)(GLenum mode);
void glMatrixMode(GLenum mode) { p_glMatrixMode(mode); }
static void (*p_glOrtho)(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { p_glOrtho(l, r, b, t, n, f); }
static void (*p_glPixelStorei)(GLenum pname, GLint param);
void glPixelStorei(GLenum pname, GLint param) { p_glPixelStorei(pname, param); }
static void (*p_glReadPixels)(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid* pixels);
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid* pixels) { p_glReadPixels(x, y, w, h, format, type, pixels); }
static void (*p_glTexCoord1f)(GLfloat s);
void glTexCoord1f(GLfloat s) { p_glTexCoord1f(s); }
static void (*p_glTexCoord2f)(GLfloat s, GLfloat t);
void glTexCoord2f(GLfloat s, GLfloat t) { p_glTexCoord2f(s, t); }
static void (*p_glTexEnvi)(GLenum target, GLenum pname, GLint param);
void glTexEnvi(GLenum target, GLenum pname, GLint param) { p_glTexEnvi(target, pname, param); }
static void (*p_glTexImage1D)(GLenum target, GLint level, GLint internal, GLsizei w, GLint border, GLenum format, GLenum type, GLvoid const* pixels);
void glTexImage1D(GLenum target, GLint level, GLint internal, GLsizei w, GLint border, GLenum format, GLenum type, GLvoid const* pixels) { p_glTexImage1D(target, level, internal, w, border, format, type, pixels); }
static void (*p_glTexImage2D)(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, GLvoid const* pixels);
void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, GLvoid const* pixels) { p_glTexImage2D(target, level, internal, w, h, border, format, type, pixels); }
static void (*p_glTexParameteri)(GLenum target, GLenum pname, GLint param);
void glTexParameteri(GLenum target, GLenum pname, GLint param) { p_glTexParameteri(target, pname, param); }
static void (*p_glTexSubImage2D)(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid const* pixels);
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid const* pixels) { p_glTexSubImage2D(target, level, x, y, w, h, format, type, pixels); }
static void (*p_glVertex2f)(GLfloat x, GLfloat y);
void glVertex2f(GLfloat x, GLfloat y) { p_glVertex2f(x, y); }
static void (*p_glVertex3f)(GLfloat x, GLfloat y, GLfloat z);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { p_glVertex3f(x, y, z); }
static void (*p_glViewport)(GLint x, GLint y, GLsizei w, GLsizei h);
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { p_glViewport(x, y, w, h); }

/* Every entry point, or none: 0 if the library lacks any of them. */
int zsnes_sdl12_gl_load(void)
{
    struct {
        char const* name;
        void** slot;
    } const fns[] = {
        { "glBegin", (void**)&p_glBegin },
        { "glBindTexture", (void**)&p_glBindTexture },
        { "glBlendFunc", (void**)&p_glBlendFunc },
        { "glClear", (void**)&p_glClear },
        { "glClearColor", (void**)&p_glClearColor },
        { "glDeleteTextures", (void**)&p_glDeleteTextures },
        { "glDisable", (void**)&p_glDisable },
        { "glEnable", (void**)&p_glEnable },
        { "glEnd", (void**)&p_glEnd },
        { "glFinish", (void**)&p_glFinish },
        { "glFlush", (void**)&p_glFlush },
        { "glGenTextures", (void**)&p_glGenTextures },
        { "glGetError", (void**)&p_glGetError },
        { "glGetString", (void**)&p_glGetString },
        { "glLoadIdentity", (void**)&p_glLoadIdentity },
        { "glMatrixMode", (void**)&p_glMatrixMode },
        { "glOrtho", (void**)&p_glOrtho },
        { "glPixelStorei", (void**)&p_glPixelStorei },
        { "glReadPixels", (void**)&p_glReadPixels },
        { "glTexCoord1f", (void**)&p_glTexCoord1f },
        { "glTexCoord2f", (void**)&p_glTexCoord2f },
        { "glTexEnvi", (void**)&p_glTexEnvi },
        { "glTexImage1D", (void**)&p_glTexImage1D },
        { "glTexImage2D", (void**)&p_glTexImage2D },
        { "glTexParameteri", (void**)&p_glTexParameteri },
        { "glTexSubImage2D", (void**)&p_glTexSubImage2D },
        { "glVertex2f", (void**)&p_glVertex2f },
        { "glVertex3f", (void**)&p_glVertex3f },
        { "glViewport", (void**)&p_glViewport },
    };
    size_t i;

    for (i = 0; i < sizeof fns / sizeof fns[0]; i++) {
        *fns[i].slot = SDL_GL_GetProcAddress(fns[i].name);
        if (!*fns[i].slot) {
            fprintf(stderr, "OpenGL: the library has no %s\n", fns[i].name);
            return 0;
        }
    }
    return 1;
}
