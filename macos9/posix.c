/* POSIX file and time calls for ZSNES on classic Mac OS, over the File
   Manager.

   The emulator names files the Unix way, and keeps doing so here: "/" is a
   list of the mounted volumes, "/Macintosh HD/Games/ZSNES/" a folder, and a
   relative path starts in the current folder, which begins as the one the
   application is in. A Mac name with a "/" in it shows as ":", as Mac OS X
   does it. Names are cut to HFS's 31 characters, keeping the extension.

   These replace Retro68's own syscall layer (libretrocrt's syscalls.c), which
   takes Mac partial paths and has no stat, mkdir, rename or directories;
   every function that file defines is defined here, so the linker never
   pulls it in. */
#include <Errors.h>
#include <Files.h>
#include <MacMemory.h>
#include <OSUtils.h>
#include <Processes.h>
#include <Timer.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <reent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

#include "macextra.h"
#include "macos9.h"

#define REF_OFFSET 10 /* file descriptors are File Manager refnums + this */
#define CREATOR 'ZSN2'

/* Seconds from 1904 (the Mac epoch) to 1970. */
#define MAC_EPOCH_OFFSET 2082844800UL

/* --- locations ------------------------------------------------------------ */

/* A folder. vref 0 with dir 0 is "/", the volume list. */
typedef struct {
    short vref;
    long dir;
} Loc;

static Loc cwd, app_loc;
static int cwd_known;

static Loc start_loc(void)
{
    if (!cwd_known) {
        ProcessSerialNumber psn = { 0, kCurrentProcess };
        ProcessInfoRec info;
        FSSpec app;

        memset(&info, 0, sizeof info);
        info.processInfoLength = sizeof info;
        info.processAppSpec = &app;
        if (GetProcessInformation(&psn, &info) == noErr) {
            cwd.vref = app.vRefNum;
            cwd.dir = app.parID;
        } else {
            short vref;
            long dir;
            HGetVol(NULL, &vref, &dir);
            cwd.vref = vref;
            cwd.dir = dir;
        }
        app_loc = cwd;
        cwd_known = 1;
    }
    return cwd;
}

static int is_vroot(Loc const l) { return l.vref == 0 && l.dir == 0; }

static int set_errno(OSErr const err)
{
    switch (err) {
    case noErr:
        return 0;
    case fnfErr:
    case dirNFErr:
    case nsvErr:
    case bdNamErr:
        errno = ENOENT;
        break;
    case dupFNErr:
        errno = EEXIST;
        break;
    case fBsyErr:
    case opWrErr:
        errno = EBUSY;
        break;
    case wPrErr:
    case vLckdErr:
        errno = EROFS;
        break;
    case fLckdErr:
    case permErr:
        errno = EACCES;
        break;
    case dskFulErr:
        errno = ENOSPC;
        break;
    case tmfoErr:
        errno = EMFILE;
        break;
    default:
        errno = EIO;
        break;
    }
    return -1;
}

/* A Unix name as an HFS one: "/" for ":", 31 characters at most, the
   extension kept when it has to be cut. */
static void to_pname(char const* const name, size_t len, unsigned char* const out)
{
    char buf[256];
    size_t i;

    if (len > 255)
        len = 255;
    for (i = 0; i < len; i++)
        buf[i] = name[i] == ':' ? '/' : name[i];
    if (len > 31) {
        size_t ext = 0, j;

        for (j = len; j > 0 && len - j < 8; j--) /* an extension of up to 7 */
            if (buf[j - 1] == '.') {
                ext = len - (j - 1);
                break;
            }
        char const* const dot = buf + len - ext;

        if (ext && ext < 31) {
            memmove(buf + 31 - ext, dot, ext);
        }
        len = 31;
    }
    out[0] = (unsigned char)len;
    memcpy(out + 1, buf, len);
}

/* An HFS name as a Unix one. */
static void from_pname(unsigned char const* const p, char* const out)
{
    unsigned i;

    for (i = 0; i < p[0]; i++)
        out[i] = p[1 + i] == '/' ? ':' : (char)p[1 + i];
    out[p[0]] = 0;
}

static OSErr cat_info(short const vref, long const dir, unsigned char const* const name, CInfoPBRec* const pb, Str63 namebuf)
{
    memset(pb, 0, sizeof *pb);
    if (name)
        memcpy(namebuf, name, name[0] + 1u);
    else
        namebuf[0] = 0;
    pb->hFileInfo.ioNamePtr = namebuf;
    pb->hFileInfo.ioVRefNum = vref;
    pb->hFileInfo.ioDirID = dir;
    pb->hFileInfo.ioFDirIndex = name ? 0 : -1; /* -1: the folder dir itself */
    return PBGetCatInfoSync(pb);
}

static int is_dir(CInfoPBRec const* const pb) { return (pb->hFileInfo.ioFlAttrib & ioDirMask) != 0; }

/* One step down (or up, for "..") from l. */
static OSErr step(Loc* const l, char const* const comp, size_t const len)
{
    if (len == 0 || (len == 1 && comp[0] == '.'))
        return noErr;
    if (len == 2 && comp[0] == '.' && comp[1] == '.') {
        CInfoPBRec pb;
        Str63 n;
        OSErr err;

        if (is_vroot(*l))
            return noErr;
        if (l->dir == fsRtDirID) {
            l->vref = 0;
            l->dir = 0;
            return noErr;
        }
        err = cat_info(l->vref, l->dir, NULL, &pb, n);
        if (err)
            return err;
        l->dir = pb.dirInfo.ioDrParID;
        return noErr;
    }
    if (is_vroot(*l)) {
        HParamBlockRec vp;
        Str255 vname;
        OSErr err;

        to_pname(comp, len, vname);
        vname[++vname[0]] = ':';
        memset(&vp, 0, sizeof vp);
        vp.volumeParam.ioNamePtr = vname;
        vp.volumeParam.ioVolIndex = -1;
        err = PBHGetVInfoSync(&vp);
        if (err)
            return err;
        l->vref = vp.volumeParam.ioVRefNum;
        l->dir = fsRtDirID;
        return noErr;
    } else {
        CInfoPBRec pb;
        Str63 name, n;
        OSErr err;

        to_pname(comp, len, name);
        err = cat_info(l->vref, l->dir, name, &pb, n);
        if (err)
            return err;
        if (!is_dir(&pb))
            return dirNFErr;
        l->dir = pb.dirInfo.ioDrDirID;
        return noErr;
    }
}

enum { K_FILE, K_DIR };

/* Where a path leads: a folder (K_DIR, in *dir) or something that is not
   one, existing or not (K_FILE, in *spec, with err fnfErr if missing). */
typedef struct {
    int kind;
    Loc dir;
    FSSpec spec;
    OSErr err;
} Target;

static OSErr resolve(char const* path, Target* const t)
{
    Loc l;
    char const* last;
    size_t n;
    OSErr err;

    if (!path || !*path)
        return fnfErr;
    if (*path == '/') {
        l.vref = 0;
        l.dir = 0;
    } else {
        l = start_loc();
    }
    /* Every component but the last is a folder. */
    n = strlen(path);
    while (n > 1 && path[n - 1] == '/')
        n--;
    last = path + n;
    while (last > path && last[-1] != '/')
        last--;
    {
        char const* p = path;
        while (p < last) {
            char const* const e = memchr(p, '/', (size_t)(last - p));
            size_t const len = (size_t)((e ? e : last) - p);

            if ((err = step(&l, p, len)) != noErr)
                return err == fnfErr ? dirNFErr : err;
            p += len + (e ? 1 : 0);
        }
    }
    n = (size_t)(path + n - last);
    if (n == 0 || (n == 1 && last[0] == '/') || (n == 1 && last[0] == '.') || (n == 2 && last[0] == '.' && last[1] == '.') || is_vroot(l)) {
        if (!(n == 1 && last[0] == '/') && (err = step(&l, last, n)) != noErr)
            return err;
        t->kind = K_DIR;
        t->dir = l;
        t->err = noErr;
        return noErr;
    }
    {
        Str63 name, nb;
        CInfoPBRec pb;

        to_pname(last, n, name);
        err = cat_info(l.vref, l.dir, name, &pb, nb);
        if (err == noErr && is_dir(&pb)) {
            t->kind = K_DIR;
            t->dir.vref = l.vref;
            t->dir.dir = pb.dirInfo.ioDrDirID;
            t->err = noErr;
            return noErr;
        }
        if (err != noErr && err != fnfErr)
            return err;
        t->kind = K_FILE;
        t->spec.vRefNum = l.vref;
        t->spec.parID = l.dir;
        memcpy(t->spec.name, name, name[0] + 1u);
        t->err = err;
        return noErr;
    }
}

/* The Unix path of folder l, "/" ending. */
static int loc_path(Loc l, char* const buf, size_t const size)
{
    char tmp[PATH_MAX];
    size_t pos = sizeof tmp - 1;

    tmp[pos] = 0;
    while (!is_vroot(l)) {
        CInfoPBRec pb;
        Str63 n;
        char name[64];
        size_t len;

        if (cat_info(l.vref, l.dir, NULL, &pb, n) != noErr)
            return -1;
        from_pname(n, name);
        len = strlen(name);
        if (pos < len + 1)
            return -1;
        tmp[--pos] = '/';
        pos -= len;
        memcpy(tmp + pos, name, len);
        if (l.dir == fsRtDirID) {
            l.vref = 0;
            l.dir = 0;
        } else {
            l.dir = pb.dirInfo.ioDrParID;
        }
    }
    tmp[--pos] = '/';
    if (sizeof tmp - 1 - pos >= size) {
        errno = ERANGE;
        return -1;
    }
    memcpy(buf, tmp + pos, sizeof tmp - pos);
    return 0;
}

/* --- stat ----------------------------------------------------------------- */

static void fill_stat(CInfoPBRec const* const pb, struct stat* const st, int const locked_vol)
{
    unsigned long mod;

    memset(st, 0, sizeof *st);
    st->st_nlink = 1;
    if (is_dir(pb)) {
        st->st_mode = S_IFDIR | 0755;
        st->st_ino = (ino_t)pb->dirInfo.ioDrDirID;
        mod = pb->dirInfo.ioDrMdDat;
    } else {
        st->st_mode = S_IFREG | 0644;
        st->st_size = (off_t)pb->hFileInfo.ioFlLgLen;
        st->st_ino = (ino_t)pb->hFileInfo.ioDirID;
        mod = pb->hFileInfo.ioFlMdDat;
        if (pb->hFileInfo.ioFlAttrib & 0x01)
            st->st_mode &= ~0222; /* locked */
    }
    if (locked_vol)
        st->st_mode &= ~0222;
    st->st_mtime = st->st_ctime = st->st_atime = mod > MAC_EPOCH_OFFSET ? (time_t)(mod - MAC_EPOCH_OFFSET) : 0;
    st->st_blksize = 4096;
    st->st_blocks = (st->st_size + 511) / 512;
}

static int stat_target(Target const* const t, struct stat* const st)
{
    CInfoPBRec pb;
    Str63 n;
    OSErr err;

    if (t->kind == K_DIR) {
        if (is_vroot(t->dir)) {
            memset(st, 0, sizeof *st);
            st->st_mode = S_IFDIR | 0555;
            st->st_nlink = 1;
            return 0;
        }
        err = cat_info(t->dir.vref, t->dir.dir, NULL, &pb, n);
    } else {
        if (t->err)
            return set_errno(t->err);
        err = cat_info(t->spec.vRefNum, t->spec.parID, t->spec.name, &pb, n);
    }
    if (err)
        return set_errno(err);
    fill_stat(&pb, st, 0);
    return 0;
}

int _stat_r(struct _reent* const r, char const* const path, struct stat* const st)
{
    Target t;
    OSErr const err = resolve(path, &t);

    (void)r;
    if (err)
        return set_errno(err);
    return stat_target(&t, st);
}

int lstat(char const* const path, struct stat* const st) { return stat(path, st); }

int access(char const* const path, int const mode)
{
    struct stat st;

    if (stat(path, &st))
        return -1;
    if ((mode & W_OK) && !(st.st_mode & 0200)) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

/* --- files ---------------------------------------------------------------- */

static OSType type_for(unsigned char const* const name)
{
    char n[64];
    char const* dot;

    from_pname(name, n);
    dot = strrchr(n, '.');
    if (dot && (!strcasecmp(dot, ".cfg") || !strcasecmp(dot, ".txt") || !strcasecmp(dot, ".log") || !strcasecmp(dot, ".cht")))
        return 'TEXT';
    if (dot && !strcasecmp(dot, ".bmp"))
        return 'BMPf';
    if (dot && !strcasecmp(dot, ".png"))
        return 'PNGf';
    return 'ZSNd';
}

int _open_r(struct _reent* const r, char const* const path, int const flags, int const mode)
{
    Target t;
    OSErr err;
    short ref;

    (void)r;
    (void)mode;
    if ((err = resolve(path, &t)) != noErr)
        return set_errno(err);
    if (t.kind == K_DIR) {
        errno = EISDIR;
        return -1;
    }
    if (t.err == fnfErr) {
        if (!(flags & O_CREAT))
            return set_errno(fnfErr);
        if ((err = FSpCreate(&t.spec, CREATOR, type_for(t.spec.name), smSystemScript)) != noErr)
            return set_errno(err);
    } else if ((flags & O_CREAT) && (flags & O_EXCL)) {
        errno = EEXIST;
        return -1;
    }
    err = FSpOpenDF(&t.spec, (flags & O_ACCMODE) == O_RDONLY ? fsRdPerm : fsRdWrPerm, &ref);
    if (err)
        return set_errno(err);
    if (flags & O_TRUNC)
        SetEOF(ref, 0);
    if (flags & O_APPEND)
        SetFPos(ref, fsFromLEOF, 0);
    return ref + REF_OFFSET;
}

int creat(char const* const path, mode_t const mode) { return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode); }

ssize_t _consolewrite(int fd, void const* buf, size_t count);

ssize_t _write_r(struct _reent* const r, int const fd, void const* const buf, size_t const count)
{
    long n = (long)count;
    OSErr err;

    (void)r;
    if (fd < REF_OFFSET)
        return _consolewrite(fd, buf, count);
    err = FSWrite((short)(fd - REF_OFFSET), &n, buf);
    if (err && n == 0)
        return set_errno(err);
    return n;
}

ssize_t _read_r(struct _reent* const r, int const fd, void* const buf, size_t const count)
{
    long n = (long)count;
    OSErr err;

    (void)r;
    if (fd < REF_OFFSET) {
        errno = EBADF;
        return -1;
    }
    err = FSRead((short)(fd - REF_OFFSET), &n, buf);
    if (err && err != eofErr)
        return set_errno(err);
    return n;
}

int _close_r(struct _reent* const r, int const fd)
{
    short vref;
    short const ref = (short)(fd - REF_OFFSET);

    (void)r;
    if (fd < REF_OFFSET)
        return 0;
    if (GetVRefNum(ref, &vref) != noErr)
        vref = 0;
    if (FSClose(ref) != noErr) {
        errno = EBADF;
        return -1;
    }
    if (vref)
        FlushVol(NULL, vref);
    return 0;
}

off_t _lseek_r(struct _reent* const r, int const fd, off_t const off, int const whence)
{
    short const ref = (short)(fd - REF_OFFSET);
    long pos, eof;
    OSErr err;

    (void)r;
    if (fd < REF_OFFSET) {
        errno = ESPIPE;
        return -1;
    }
    if ((err = GetEOF(ref, &eof)) != noErr)
        return set_errno(err);
    switch (whence) {
    case SEEK_SET:
        pos = (long)off;
        break;
    case SEEK_CUR:
        if ((err = GetFPos(ref, &pos)) != noErr)
            return set_errno(err);
        pos += (long)off;
        break;
    case SEEK_END:
        pos = eof + (long)off;
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    if (pos < 0) {
        errno = EINVAL;
        return -1;
    }
    if (pos > eof && (err = SetEOF(ref, pos)) != noErr) /* POSIX lets a seek pass the end */
        return set_errno(err);
    if ((err = SetFPos(ref, fsFromStart, pos)) != noErr)
        return set_errno(err);
    return pos;
}

int _fstat_r(struct _reent* const r, int const fd, struct stat* const st)
{
    FCBPBRec pb;
    Str63 name;
    CInfoPBRec cpb;
    Str63 n;

    (void)r;
    memset(st, 0, sizeof *st);
    if (fd < REF_OFFSET) {
        st->st_mode = S_IFCHR | 0666;
        return 0;
    }
    memset(&pb, 0, sizeof pb);
    pb.ioNamePtr = name;
    pb.ioRefNum = (short)(fd - REF_OFFSET);
    pb.ioFCBIndx = 0;
    if (PBGetFCBInfoSync(&pb) != noErr) {
        errno = EBADF;
        return -1;
    }
    if (cat_info(pb.ioFCBVRefNum, pb.ioFCBParID, name, &cpb, n) == noErr) {
        fill_stat(&cpb, st, 0);
    } else {
        st->st_mode = S_IFREG | 0644;
    }
    st->st_size = (off_t)pb.ioFCBEOF;
    return 0;
}

int ftruncate(int const fd, off_t const len)
{
    OSErr const err = SetEOF((short)(fd - REF_OFFSET), (long)len);
    return set_errno(err);
}

int _isatty_r(struct _reent* const r, int const fd)
{
    (void)r;
    return fd < REF_OFFSET;
}

int _fcntl_r(struct _reent* const r, int const fd, int const cmd, int const arg)
{
    (void)r;
    (void)fd;
    (void)cmd;
    (void)arg;
    return 0;
}

int fsync(int const fd)
{
    short vref;

    if (fd >= REF_OFFSET && GetVRefNum((short)(fd - REF_OFFSET), &vref) == noErr)
        FlushVol(NULL, vref);
    return 0;
}

/* --- folders and names ---------------------------------------------------- */

int _mkdir_r(struct _reent* const r, char const* const path, int const mode)
{
    Target t;
    OSErr err;
    long id;

    (void)r;
    (void)mode;
    if ((err = resolve(path, &t)) != noErr)
        return set_errno(err);
    if (t.kind == K_DIR || t.err == noErr) {
        errno = EEXIST;
        return -1;
    }
    return set_errno(FSpDirCreate(&t.spec, smSystemScript, &id));
}

static int remove_target(char const* const path, int const want_dir)
{
    Target t;
    OSErr err;
    FSSpec s;

    if ((err = resolve(path, &t)) != noErr)
        return set_errno(err);
    if (t.kind == K_DIR) {
        if (!want_dir) {
            errno = EISDIR;
            return -1;
        }
        if (is_vroot(t.dir) || t.dir.dir == fsRtDirID) {
            errno = EBUSY;
            return -1;
        }
        if ((err = FSMakeFSSpec(t.dir.vref, t.dir.dir, (ConstStr255Param) "\0", &s)) != noErr)
            return set_errno(err);
        err = FSpDelete(&s);
        if (err == fBsyErr)
            errno = ENOTEMPTY;
        else
            set_errno(err);
        return err ? -1 : 0;
    }
    if (t.err)
        return set_errno(t.err);
    if (want_dir) {
        errno = ENOTDIR;
        return -1;
    }
    return set_errno(FSpDelete(&t.spec));
}

int _unlink_r(struct _reent* const r, char const* const path)
{
    (void)r;
    return remove_target(path, 0);
}

int rmdir(char const* const path) { return remove_target(path, 1); }

int _rename_r(struct _reent* const r, char const* const from, char const* const to)
{
    Target a, b;
    OSErr err;
    FSSpec src;

    (void)r;
    if ((err = resolve(from, &a)) != noErr)
        return set_errno(err);
    if ((err = resolve(to, &b)) != noErr)
        return set_errno(err);
    if (a.kind == K_DIR) {
        if ((err = FSMakeFSSpec(a.dir.vref, a.dir.dir, (ConstStr255Param) "\0", &src)) != noErr)
            return set_errno(err);
    } else {
        if (a.err)
            return set_errno(a.err);
        src = a.spec;
    }
    if (b.kind == K_DIR) {
        errno = EISDIR;
        return -1;
    }
    if (src.vRefNum != b.spec.vRefNum) {
        errno = EXDEV;
        return -1;
    }
    if (b.err == noErr) { /* POSIX rename replaces the target */
        if (src.parID == b.spec.parID && EqualString(src.name, b.spec.name, false, true))
            return 0;
        if ((err = FSpDelete(&b.spec)) != noErr)
            return set_errno(err);
    }
    if (src.parID != b.spec.parID) {
        FSSpec dest_dir;

        if ((err = FSMakeFSSpec(b.spec.vRefNum, b.spec.parID, (ConstStr255Param) "\0", &dest_dir)) != noErr)
            return set_errno(err);
        if ((err = FSpCatMove(&src, &dest_dir)) != noErr)
            return set_errno(err);
        src.parID = b.spec.parID;
    }
    if (!EqualString(src.name, b.spec.name, true, true) && (err = FSpRename(&src, b.spec.name)) != noErr)
        return set_errno(err);
    return 0;
}

int _link_r(struct _reent* const r, char const* const a, char const* const b)
{
    (void)a;
    (void)b;
    r->_errno = EPERM;
    return -1;
}

char* getcwd(char* const buf, size_t const size)
{
    char* const out = buf ? buf : malloc(size ? size : PATH_MAX);

    if (!out)
        return NULL;
    if (loc_path(start_loc(), out, buf ? size : (size ? size : PATH_MAX))) {
        if (!buf)
            free(out);
        return NULL;
    }
    /* Unix getcwd has no trailing slash but on "/". */
    if (out[1]) {
        out[strlen(out) - 1] = 0;
    }
    return out;
}

int chdir(char const* const path)
{
    Target t;
    OSErr err;

    if ((err = resolve(path, &t)) != noErr)
        return set_errno(err);
    if (t.kind != K_DIR) {
        errno = t.err ? ENOENT : ENOTDIR;
        return -1;
    }
    start_loc();
    cwd = t.dir;
    return 0;
}

char* realpath(char const* const path, char* const resolved)
{
    Target t;
    OSErr err;
    char* const out = resolved ? resolved : malloc(PATH_MAX);
    char name[64];
    size_t len;

    if (!out)
        return NULL;
    if ((err = resolve(path, &t)) != noErr || (t.kind == K_FILE && t.err)) {
        set_errno(err ? err : t.err);
        goto fail;
    }
    if (t.kind == K_DIR) {
        if (loc_path(t.dir, out, PATH_MAX))
            goto fail;
        len = strlen(out);
        if (len > 1)
            out[len - 1] = 0;
        return out;
    }
    {
        Loc const l = { t.spec.vRefNum, t.spec.parID };
        if (loc_path(l, out, PATH_MAX))
            goto fail;
    }
    from_pname(t.spec.name, name);
    if (strlen(out) + strlen(name) >= PATH_MAX) {
        errno = ENAMETOOLONG;
        goto fail;
    }
    strcat(out, name);
    return out;
fail:
    if (!resolved)
        free(out);
    return NULL;
}

/* --- directories ---------------------------------------------------------- */

struct mac_dir {
    Loc loc;
    short index; /* next entry; 1 and 2 are "." and ".." */
    struct dirent ent;
};

#define MAX_DIRS 8
static struct mac_dir* open_dirs[MAX_DIRS];
#define DIR_FD_BASE 0x4000

DIR* opendir(char const* const path)
{
    Target t;
    OSErr err;
    struct mac_dir* d;
    int i;

    if ((err = resolve(path, &t)) != noErr) {
        set_errno(err);
        return NULL;
    }
    if (t.kind != K_DIR) {
        errno = t.err ? ENOENT : ENOTDIR;
        return NULL;
    }
    for (i = 0; i < MAX_DIRS && open_dirs[i]; i++)
        ;
    if (i == MAX_DIRS) {
        errno = EMFILE;
        return NULL;
    }
    if (!(d = calloc(1, sizeof *d)))
        return NULL;
    d->loc = t.dir;
    d->index = 1;
    open_dirs[i] = d;
    return (DIR*)d;
}

struct dirent* readdir(DIR* const dirp)
{
    struct mac_dir* const d = (struct mac_dir*)dirp;

    for (;;) {
        short const i = d->index++;

        if (i <= 2) {
            strcpy(d->ent.d_name, i == 1 ? "." : "..");
            d->ent.d_ino = (ino_t)i;
            return &d->ent;
        }
        if (is_vroot(d->loc)) {
            HParamBlockRec vp;
            Str255 vname;

            memset(&vp, 0, sizeof vp);
            vp.volumeParam.ioNamePtr = vname;
            vp.volumeParam.ioVolIndex = (short)(i - 2);
            if (PBHGetVInfoSync(&vp) != noErr)
                return NULL;
            from_pname(vname, d->ent.d_name);
            d->ent.d_ino = (ino_t)(unsigned short)vp.volumeParam.ioVRefNum;
            return &d->ent;
        } else {
            CInfoPBRec pb;
            Str63 n;

            memset(&pb, 0, sizeof pb);
            pb.hFileInfo.ioNamePtr = n;
            pb.hFileInfo.ioVRefNum = d->loc.vref;
            pb.hFileInfo.ioDirID = d->loc.dir;
            pb.hFileInfo.ioFDirIndex = (short)(i - 2);
            if (PBGetCatInfoSync(&pb) != noErr)
                return NULL;
            /* Invisible items (the custom icon file, the desktop database)
               stay invisible. */
            if (!is_dir(&pb) && (pb.hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible))
                continue;
            if (is_dir(&pb) && (pb.dirInfo.ioDrUsrWds.frFlags & kIsInvisible))
                continue;
            from_pname(n, d->ent.d_name);
            d->ent.d_ino = (ino_t)i;
            return &d->ent;
        }
    }
}

void rewinddir(DIR* const dirp) { ((struct mac_dir*)dirp)->index = 1; }

int closedir(DIR* const dirp)
{
    int i;

    for (i = 0; i < MAX_DIRS; i++)
        if (open_dirs[i] == (struct mac_dir*)dirp)
            open_dirs[i] = NULL;
    free(dirp);
    return 0;
}

int dirfd(DIR* const dirp)
{
    int i;

    for (i = 0; i < MAX_DIRS; i++)
        if (open_dirs[i] == (struct mac_dir*)dirp)
            return DIR_FD_BASE + i;
    errno = EBADF;
    return -1;
}

int fstatat(int const fd, char const* const name, struct stat* const st, int const flag)
{
    Target t;
    Loc saved;
    int r;

    (void)flag;
    if (fd < DIR_FD_BASE || fd >= DIR_FD_BASE + MAX_DIRS || !open_dirs[fd - DIR_FD_BASE]) {
        errno = EBADF;
        return -1;
    }
    /* Resolve the name from the directory's folder. */
    saved = start_loc();
    cwd = open_dirs[fd - DIR_FD_BASE]->loc;
    r = resolve(name, &t);
    cwd = saved;
    if (r)
        return set_errno((OSErr)r);
    return stat_target(&t, st);
}

/* --- time ----------------------------------------------------------------- */

/* Microseconds since start-up, from the time base the Time Manager keeps. */
uint64_t macos9_microseconds(void)
{
    UnsignedWide w;

    Microseconds(&w);
    return ((uint64_t)w.hi << 32) | w.lo;
}

int _gettimeofday_r(struct _reent* const r, struct timeval* const tp, void* const tz)
{
    static unsigned long base_secs;
    static uint64_t base_us;

    (void)r;
    (void)tz;
    if (!base_secs) {
        GetDateTime(&base_secs);
        base_us = macos9_microseconds();
    }
    if (tp) {
        uint64_t const us = macos9_microseconds() - base_us;

        tp->tv_sec = (time_t)(base_secs - MAC_EPOCH_OFFSET + us / 1000000u);
        tp->tv_usec = (suseconds_t)(us % 1000000u);
    }
    return 0;
}

int usleep(useconds_t const us)
{
    uint64_t const end = macos9_microseconds() + us;

    while (macos9_microseconds() < end)
        macos9_idle();
    return 0;
}

int nanosleep(struct timespec const* const req, struct timespec* const rem)
{
    if (rem)
        rem->tv_sec = rem->tv_nsec = 0;
    return usleep((useconds_t)(req->tv_sec * 1000000 + req->tv_nsec / 1000));
}

unsigned sleep(unsigned const s)
{
    usleep((useconds_t)s * 1000000u);
    return 0;
}

clock_t _times_r(struct _reent* const r, struct tms* const buf)
{
    clock_t const t = (clock_t)(macos9_microseconds() / (1000000u / CLOCKS_PER_SEC));

    (void)r;
    if (buf) {
        buf->tms_utime = t;
        buf->tms_stime = buf->tms_cutime = buf->tms_cstime = 0;
    }
    return t;
}

/* --- processes: there is one ---------------------------------------------- */

void* _sbrk_r(struct _reent* const r, ptrdiff_t const incr)
{
    (void)r;
    return NewPtrClear(incr);
}

void _exit(int const status)
{
    (void)status;
    macos9_shutdown();
    ExitToShell();
    for (;;)
        ;
}

int _kill_r(struct _reent* const r, pid_t const pid, int const sig)
{
    (void)sig;
    if (pid == 42)
        _exit(128 + sig);
    r->_errno = ESRCH;
    return -1;
}

pid_t _getpid_r(struct _reent* const r)
{
    (void)r;
    return 42;
}

int _fork_r(struct _reent* const r)
{
    r->_errno = ENOSYS;
    return -1;
}

int _execve_r(struct _reent* const r, char const* const fn, char* const* const argv, char* const* const envp)
{
    (void)fn;
    (void)argv;
    (void)envp;
    r->_errno = ENOSYS;
    return -1;
}

int _wait_r(struct _reent* const r, int* const status)
{
    (void)status;
    r->_errno = ECHILD;
    return -1;
}

int _getentropy_r(struct _reent* const r, void* const buf, size_t const len)
{
    (void)r;
    (void)buf;
    (void)len;
    errno = ENOSYS;
    return -1;
}

uid_t geteuid(void) { return 0; }
gid_t getegid(void) { return 0; }
uid_t getuid(void) { return 0; }
gid_t getgid(void) { return 0; }

/* --- the console: a log file next to the application ---------------------- */

static int log_fd = -2;

ssize_t _consolewrite(int const fd, void const* const buf, size_t const count)
{
    (void)fd;
    if (log_fd == -2) {
        Loc const saved = start_loc();

        log_fd = -1;
        cwd = app_loc;
        log_fd = open("ZSNES Log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        cwd = saved;
    }
    if (log_fd >= 0) {
        long n = (long)count;
        FSWrite((short)(log_fd - REF_OFFSET), &n, buf);
    }
    return (ssize_t)count;
}

ssize_t _consoleread(int const fd, void* const buf, size_t const count)
{
    (void)fd;
    (void)buf;
    (void)count;
    return 0;
}

void macos9_close_log(void)
{
    if (log_fd >= 0) {
        close(log_fd);
        log_fd = -1;
    }
}

int clock_gettime(clockid_t const id, struct timespec* const tp)
{
    if (id == CLOCK_REALTIME) {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        tp->tv_sec = tv.tv_sec;
        tp->tv_nsec = tv.tv_usec * 1000L;
    } else {
        uint64_t const us = macos9_microseconds();

        tp->tv_sec = (time_t)(us / 1000000u);
        tp->tv_nsec = (long)(us % 1000000u) * 1000L;
    }
    return 0;
}

/* Modification dates are the File Manager's to keep. */
int utime(char const* const path, struct utimbuf const* const times)
{
    (void)times;
    return access(path, F_OK);
}

int mkdir(char const* const path, mode_t const mode) { return _mkdir_r(_REENT, path, (int)mode); }

/* No users, no signals, no other programs. */
#include <pwd.h>
#include <signal.h>

struct passwd* getpwnam(char const* const name)
{
    (void)name;
    return NULL;
}

struct passwd* getpwuid(uid_t const uid)
{
    (void)uid;
    return NULL;
}

int sigaction(int const sig, struct sigaction const* const act, struct sigaction* const old)
{
    (void)sig;
    (void)act;
    if (old)
        memset(old, 0, sizeof *old);
    return 0;
}

int execvp(char const* const file, char* const argv[])
{
    (void)file;
    (void)argv;
    errno = ENOSYS;
    return -1;
}

/* Only the debug build's stdout and stderr redirection asks for these. */
int dup(int const fd)
{
    (void)fd;
    errno = ENOSYS;
    return -1;
}

int dup2(int const fd, int const fd2)
{
    (void)fd;
    (void)fd2;
    errno = ENOSYS;
    return -1;
}
