/* posix.c - the current directory the PS4 build keeps for itself (see
   ps4/compat.h): the system's when chdir works, its own when it does not. */
#include "compat.h"

#undef chdir
#undef getcwd
#undef realpath
#undef fopen
#undef access

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char cwd[1024];

void ps4_set_cwd(char const* const path)
{
    snprintf(cwd, sizeof cwd, "%s", path);
}

int ps4_chdir(char const* const path)
{
    char full[1024];
    struct stat st;

    if (!path || !*path) {
        errno = ENOENT;
        return -1;
    }
    if (path[0] == '/' || !cwd[0])
        snprintf(full, sizeof full, "%s", path);
    else
        snprintf(full, sizeof full, "%s/%s", strcmp(cwd, "/") ? cwd : "", path);
    if (stat(full, &st) != 0)
        return -1;
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }
    (void)chdir(full); /* where the system allows it */
    /* ".." and "." are left in: every system the paths reach takes them. */
    snprintf(cwd, sizeof cwd, "%s", full);
    return 0;
}

char* ps4_getcwd(char* const buf, size_t const size)
{
    if (!cwd[0] && getcwd(cwd, sizeof cwd) == NULL)
        snprintf(cwd, sizeof cwd, "/");
    if (!buf || strlen(cwd) + 1 > size) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, cwd, strlen(cwd) + 1);
    return buf;
}

/* Directories (see ps4/compat.h). */
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <orbis/libkernel.h>

#undef opendir
#undef readdir
#undef closedir
#undef dirfd
#undef fstatat
#undef stat

#define MAX_DIRS 16
#define DIR_FD_BASE 0x4000

struct ps4_dir {
    int fd;
    int len, pos;
    char path[1024];
    char buf[8192];
    struct dirent ent;
};

static struct ps4_dir* dirs[MAX_DIRS];

static void full_path(char* const out, size_t const size, char const* const path)
{
    if (path[0] == '/' || !cwd[0])
        snprintf(out, size, "%s", path);
    else
        snprintf(out, size, "%s/%s", strcmp(cwd, "/") ? cwd : "", path);
}

int ps4_stat(char const* const path, struct stat* const st)
{
    char full[1024];
    OrbisKernelStat ks;

    if (!path || !*path) {
        errno = ENOENT;
        return -1;
    }
    full_path(full, sizeof full, path);
    /* The kernel's own call first: its structure is the kernel's, where the
       C library's might not match the headers ZSNES is built with. */
    if (sceKernelStat(full, &ks) != 0)
        return stat(full, st);
    memset(st, 0, sizeof *st);
    st->st_mode = ks.st_mode;
    st->st_size = ks.st_size;
    st->st_uid = ks.st_uid;
    st->st_gid = ks.st_gid;
    st->st_mtime = ks.st_mtim.tv_sec;
    return 0;
}

DIR* ps4_opendir(char const* const path)
{
    struct ps4_dir* d;
    int i, fd;

    for (i = 0; i < MAX_DIRS && dirs[i]; i++)
        ;
    if (i == MAX_DIRS) {
        errno = EMFILE;
        return NULL;
    }
    d = calloc(1, sizeof *d);
    if (!d) {
        errno = ENOMEM;
        return NULL;
    }
    full_path(d->path, sizeof d->path, path && *path ? path : ".");
    fd = sceKernelOpen(d->path, 0x20000 /* O_DIRECTORY */ | 0 /* O_RDONLY */, 0);
    if (fd < 0) {
        ps4_log("opendir %s failed: 0x%08x", d->path, (unsigned)fd);
        free(d);
        errno = ENOENT;
        return NULL;
    }
    d->fd = fd;
    dirs[i] = d;
    return (DIR*)d;
}

struct dirent* ps4_readdir(DIR* const dir)
{
    struct ps4_dir* const d = (struct ps4_dir*)dir;

    for (;;) {
        unsigned char const* e;
        unsigned reclen, namlen;

        if (d->pos >= d->len) {
            int const n = sceKernelGetdents(d->fd, d->buf, sizeof d->buf);
            if (n <= 0)
                return NULL;
            d->len = n;
            d->pos = 0;
        }
        /* FreeBSD's dirent: fileno (4), reclen (2), type (1), namlen (1),
           then the name. */
        e = (unsigned char const*)d->buf + d->pos;
        reclen = e[4] | e[5] << 8;
        if (reclen < 8 || d->pos + (int)reclen > d->len) {
            d->pos = d->len;
            continue;
        }
        d->pos += reclen;
        namlen = e[7];
        if (*(uint32_t const*)e == 0 || namlen == 0)
            continue;
        if (namlen >= sizeof d->ent.d_name)
            namlen = sizeof d->ent.d_name - 1;
        memcpy(d->ent.d_name, e + 8, namlen);
        d->ent.d_name[namlen] = '\0';
        d->ent.d_type = e[6];
        return &d->ent;
    }
}

int ps4_closedir(DIR* const dir)
{
    struct ps4_dir* const d = (struct ps4_dir*)dir;
    int i;

    for (i = 0; i < MAX_DIRS; i++)
        if (dirs[i] == d)
            dirs[i] = NULL;
    sceKernelClose(d->fd);
    free(d);
    return 0;
}

int ps4_dirfd(DIR* const dir)
{
    int i;

    for (i = 0; i < MAX_DIRS; i++)
        if (dirs[i] == (struct ps4_dir*)dir)
            return DIR_FD_BASE + i;
    errno = EBADF;
    return -1;
}

int ps4_fstatat(int const fd, char const* const name, struct stat* const st, int const flag)
{
    char path[1280];
    struct ps4_dir* d;
    int const i = fd - DIR_FD_BASE;

    (void)flag;
    if (name[0] == '/')
        return ps4_stat(name, st);
    if (i < 0 || i >= MAX_DIRS || !(d = dirs[i])) {
        errno = EBADF;
        return -1;
    }
    snprintf(path, sizeof path, "%s/%s", strcmp(d->path, "/") ? d->path : "", name);
    if (ps4_stat(path, st) == 0)
        return 0;
    /* Not one the PS4 will stat: listed by the type its directory gave. */
    if (!strcmp(name, d->ent.d_name) && (d->ent.d_type == DT_DIR || d->ent.d_type == DT_REG)) {
        memset(st, 0, sizeof *st);
        st->st_mode = d->ent.d_type == DT_DIR ? S_IFDIR | 0777 : S_IFREG | 0666;
        return 0;
    }
    return -1;
}

/* Paths (see ps4/compat.h). */
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

char* ps4_realpath(char const* const path, char* const resolved)
{
    char in[PATH_MAX * 2], out[PATH_MAX];
    char* save = NULL;
    char* part;
    size_t len = 0;
    struct stat st;

    if (!path || !*path) {
        errno = ENOENT;
        return NULL;
    }
    full_path(in, sizeof in, path);
    out[0] = '\0';
    for (part = strtok_r(in, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if (!strcmp(part, "."))
            continue;
        if (!strcmp(part, "..")) {
            char* const slash = strrchr(out, '/');

            if (slash) {
                *slash = '\0';
                len = (size_t)(slash - out);
            }
            continue;
        }
        if (len + 1 + strlen(part) + 1 > sizeof out) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        out[len++] = '/';
        strcpy(out + len, part);
        len += strlen(part);
    }
    if (!len)
        strcpy(out, "/");
    if (ps4_stat(out, &st) != 0) {
        ps4_log("realpath %s: no such file", out);
        return NULL;
    }
    if (resolved) {
        strcpy(resolved, out); /* the callers' buffers are PATH_MAX long */
        return resolved;
    }
    return strdup(out);
}

FILE* ps4_fopen(char const* const path, char const* const mode)
{
    char full[PATH_MAX];
    FILE* f;

    if (!path || !*path) {
        errno = ENOENT;
        return NULL;
    }
    full_path(full, sizeof full, path);
    f = fopen(full, mode);
    if (!f && mode[0] != 'r')
        ps4_log("could not open %s (%s): errno %d", full, mode, errno);
    else if (!f && errno != ENOENT)
        ps4_log("could not read %s: errno %d", full, errno);
    return f;
}

int ps4_access(char const* const path, int const mode)
{
    char full[PATH_MAX];
    struct stat st;

    (void)mode; /* everything ZSNES's own folders hold is its to read and write */
    if (!path || !*path) {
        errno = ENOENT;
        return -1;
    }
    full_path(full, sizeof full, path);
    if (ps4_stat(full, &st) != 0) {
        ps4_log("access %s: not found", full);
        return -1;
    }
    return 0;
}
