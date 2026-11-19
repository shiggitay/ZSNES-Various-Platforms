/* Texture uploads for the immediate-mode renderer (unix/gl_draw.c), shared
   by the builds that reach OpenGL through a layer of their own: Mac OS 9
   (AGL) and SDL 1.2. GL/gl.h in unix/glcompat routes glTexImage2D,
   glTexSubImage2D and glPixelStorei here.

   The renderer uploads 5:6:5 pixels, an OpenGL 1.2 format; the OpenGL of
   Mac OS 9.0 and of older Mesa is 1.1, and there the uploads are converted
   to RGBA first. */
#define ZGL_IMPL
#include <GL/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glupload.h"

static int packed_565; /* the driver takes GL_UNSIGNED_SHORT_5_6_5 */

void zgl_detect(void)
{
    char const* const v = (char const*)glGetString(GL_VERSION);
    char const* const ext = (char const*)glGetString(GL_EXTENSIONS);
    int major = 1, minor = 0;

    if (v)
        sscanf(v, "%d.%d", &major, &minor);
    packed_565 = major > 1 || (major == 1 && minor >= 2)
        || (ext && (strstr(ext, "GL_APPLE_packed_pixel") != NULL));
    printf("OpenGL: %s, %s%s\n", (char const*)glGetString(GL_RENDERER), v ? v : "?",
        packed_565 ? "" : " (converting 5:6:5 uploads)");
}

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

/* Every 5:6:5 upload is handed over tightly packed, with the unpack state
   at its defaults. The renderer uploads the emulator's frame in place - rows
   of 288 pixels of which 256 are shown, described with GL_UNPACK_ROW_LENGTH
   and GL_UNPACK_SKIP_PIXELS - and the Mac OS 9 OpenGL of a Radeon was seen
   to ignore those for this format: each row came out 32 pixels behind the
   last, a picture sheared and wrapped every 8 rows. Copying ~115 KB a frame
   costs next to nothing; the conversion to RGBA, where the driver has no
   5:6:5, is done in the same pass. */
static GLvoid const* repack(GLsizei const w, GLsizei const h, GLvoid const* const pixels)
{
    static GLubyte* buf;
    static size_t cap;
    size_t const need = (size_t)w * (size_t)h * (packed_565 ? 2 : 4);
    GLint const stride = row_length ? row_length : w;
    GLsizei x, y;

    if (!pixels)
        return NULL;
    if (packed_565 && stride == w && !skip_pixels && !skip_rows)
        return pixels;
    if (need > cap) {
        free(buf);
        buf = (GLubyte*)malloc(need);
        cap = buf ? need : 0;
        if (!buf)
            return NULL;
    }
    for (y = 0; y < h; y++) {
        GLushort const* const in = (GLushort const*)pixels + (size_t)(y + skip_rows) * stride + skip_pixels;

        if (packed_565) {
            memcpy(buf + (size_t)y * w * 2, in, (size_t)w * 2);
        } else {
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
    if (type == GL_UNSIGNED_SHORT_5_6_5) {
        GLvoid const* const data = repack(w, h, pixels);

        if (pixels && !data)
            return;
        unpack_plain(1);
        if (packed_565)
            glTexImage2D(target, level, internal, w, h, border, format, type, data);
        else
            glTexImage2D(target, level, internal, w, h, border, GL_RGBA, GL_UNSIGNED_BYTE, data);
        unpack_plain(0);
        return;
    }
    glTexImage2D(target, level, internal, w, h, border, format, type, pixels);
}

void zgl_TexSubImage2D(GLenum const target, GLint const level, GLint const x, GLint const y,
    GLsizei const w, GLsizei const h, GLenum const format, GLenum const type, GLvoid const* const pixels)
{
    if (type == GL_UNSIGNED_SHORT_5_6_5) {
        GLvoid const* const data = repack(w, h, pixels);

        if (!data)
            return;
        unpack_plain(1);
        if (packed_565)
            glTexSubImage2D(target, level, x, y, w, h, format, type, data);
        else
            glTexSubImage2D(target, level, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
        unpack_plain(0);
        return;
    }
    glTexSubImage2D(target, level, x, y, w, h, format, type, pixels);
}

