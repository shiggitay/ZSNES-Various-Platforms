/* The OpenGL 1.1 subset ZSNES's immediate-mode renderer (unix/gl_draw.c)
   uses, for Mac OS 9's OpenGLLibrary, which Retro68's headers do not cover.
   Values are the standard ones. Texture uploads go through macos9/gl.c, which
   converts the 5:6:5 pixels for an OpenGL older than 1.2. */
#ifndef ZSNES_MACOS9_GL_H
#define ZSNES_MACOS9_GL_H

typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef unsigned char GLubyte;
typedef short GLshort;
typedef unsigned short GLushort;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

#define GL_ZERO 0
#define GL_QUADS 0x0007
#define GL_DST_COLOR 0x0306
#define GL_LIGHTING 0x0B50
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_TEXTURE_1D 0x0DE0
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_DECAL 0x2101
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_TEXTURE_ENV 0x2300
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP 0x2900
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_UNSIGNED_SHORT_5_6_5 0x8363

/* Uploads and pixel-store state go through macos9/gl.c. */
#ifndef ZSNES_MACOS9_GL_IMPL
#define glTexImage2D zgl_TexImage2D
#define glTexSubImage2D zgl_TexSubImage2D
#define glPixelStorei zgl_PixelStorei
#endif

void glBegin(GLenum mode);
void glBindTexture(GLenum target, GLuint texture);
void glBlendFunc(GLenum sfactor, GLenum dfactor);
void glClear(GLbitfield mask);
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void glDeleteTextures(GLsizei n, GLuint const* textures);
void glDisable(GLenum cap);
void glEnable(GLenum cap);
void glEnd(void);
void glFinish(void);
void glFlush(void);
void glGenTextures(GLsizei n, GLuint* textures);
GLenum glGetError(void);
GLubyte const* glGetString(GLenum name);
void glLoadIdentity(void);
void glMatrixMode(GLenum mode);
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void glPixelStorei(GLenum pname, GLint param);
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid* pixels);
void glTexCoord1f(GLfloat s);
void glTexCoord2f(GLfloat s, GLfloat t);
void glTexEnvi(GLenum target, GLenum pname, GLint param);
void glTexImage1D(GLenum target, GLint level, GLint internal, GLsizei w, GLint border, GLenum format,
    GLenum type, GLvoid const* pixels);
void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border,
    GLenum format, GLenum type, GLvoid const* pixels);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei w, GLsizei h, GLenum format,
    GLenum type, GLvoid const* pixels);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);

#endif
