/* ios_glstub.c - the iOS build has no desktop OpenGL. Its OpenGL video
   modes draw through SDL's renderer (Metal), so the immediate-mode OpenGL
   path - unix/gl_draw.c and the few calls in unix/sdllink.c - is never
   chosen there (UseLegacyGL). These keep the program linking: the GL 1.1
   subset of unix/glcompat/GL/gl.h, doing nothing. */
#define ZGL_IMPL
#include <GL/gl.h>

#include "gl_draw.h"

char allow_glvsync = 0;
int gl_start(int width, int height, int req_depth, int FullScreen) { (void)width; (void)height; (void)req_depth; (void)FullScreen; return 0; }
void gl_end(void) { }
void gl_clearwin(void) { }
void gl_drawwin(void) { }
void SetGLAttributes(void) { }

void glBegin(GLenum mode) { }
void glBindTexture(GLenum target, GLuint texture) { }
void glBlendFunc(GLenum sfactor, GLenum dfactor) { }
void glClear(GLbitfield mask) { }
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) { }
void glDeleteTextures(GLsizei n, GLuint const* textures) { }
void glDisable(GLenum cap) { }
void glEnable(GLenum cap) { }
void glEnd(void) { }
void glFinish(void) { }
void glFlush(void) { }
void glGenTextures(GLsizei n, GLuint* textures) { }
GLenum glGetError(void) { return 0; }
GLubyte const* glGetString(GLenum name) { return 0; }
void glLoadIdentity(void) { }
void glMatrixMode(GLenum mode) { }
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { }
void glPixelStorei(GLenum pname, GLint param) { }
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid* pixels) { }
void glTexCoord1f(GLfloat s) { }
void glTexCoord2f(GLfloat s, GLfloat t) { }
void glTexEnvi(GLenum target, GLenum pname, GLint param) { }
void glTexImage1D(GLenum target, GLint level, GLint internal, GLsizei w, GLint border, GLenum format, GLenum type, GLvoid const* pixels) { }
void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, GLvoid const* pixels) { }
void glTexParameteri(GLenum target, GLenum pname, GLint param) { }
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid const* pixels) { }
void glVertex2f(GLfloat x, GLfloat y) { }
void glVertex3f(GLfloat x, GLfloat y, GLfloat z) { }
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { }
/* unix/glupload.c's names, which the header maps uploads to elsewhere. */
void zgl_detect(void) { }
void zgl_TexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border,
    GLenum format, GLenum type, GLvoid const* pixels) { }
void zgl_TexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format,
    GLenum type, GLvoid const* pixels) { }
void zgl_PixelStorei(GLenum pname, GLint param) { }
