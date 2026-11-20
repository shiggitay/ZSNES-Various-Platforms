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

static int big_endian(void)
{
    union {
        GLushort s;
        GLubyte b[2];
    } const u = { 0x0102 };

    return u.b[0] == 1;
}

void zgl_detect(void)
{
    char const* const v = (char const*)glGetString(GL_VERSION);
    char const* const ext = (char const*)glGetString(GL_EXTENSIONS);
    char const* const r = (char const*)glGetString(GL_RENDERER);
    char const* const force = getenv("ZSNES_GL_UPLOAD");
    int major = 1, minor = 0;

    if (v)
        sscanf(v, "%d.%d", &major, &minor);
    packed_565 = major > 1 || (major == 1 && minor >= 2)
        || (ext && (strstr(ext, "GL_APPLE_packed_pixel") != NULL));
    /* Mesa's hardware drivers of the XFree86 4.3 era read 5:6:5 texels with
       their bytes swapped on a big-endian machine: on Yellow Dog Linux 3
       with a Radeon 7000 and DRI, the picture came out in inverted, wrong
       colours. Bytes in RGBA order mean the same to every driver. */
    if (packed_565 && big_endian() && r && strstr(r, "Mesa DRI"))
        packed_565 = 0;
    /* ZSNES_GL_UPLOAD=565 or =rgba overrides the choice. */
    if (force && !strcmp(force, "565"))
        packed_565 = 1;
    else if (force && !strcmp(force, "rgba"))
        packed_565 = 0;
    printf("OpenGL: %s, %s%s\n", r ? r : "?", v ? v : "?",
        packed_565 ? "" : " (converting 5:6:5 uploads to RGBA)");
}

/* 5:6:5 to the four bytes R, G, B, A, as one 32-bit store whatever the
   host's byte order. */
static GLuint* rgba_lut;

static int make_rgba_lut(void)
{
    unsigned p;

    if (rgba_lut)
        return 1;
    if (!(rgba_lut = (GLuint*)malloc(65536 * sizeof *rgba_lut)))
        return 0;
    for (p = 0; p < 65536; p++) {
        unsigned const r5 = p >> 11, g6 = p >> 5 & 63, b5 = p & 31;
        GLubyte const px[4] = { (GLubyte)(r5 << 3 | r5 >> 2), (GLubyte)(g6 << 2 | g6 >> 4),
            (GLubyte)(b5 << 3 | b5 >> 2), 0xFF };

        memcpy(&rgba_lut[p], px, 4);
    }
    return 1;
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
    if (!packed_565 && !make_rgba_lut())
        return NULL;
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
            GLuint* const out = (GLuint*)(void*)(buf + (size_t)y * w * 4);

            for (x = 0; x < w; x++)
                out[x] = rgba_lut[in[x]];
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

