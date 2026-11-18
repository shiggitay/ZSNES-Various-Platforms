#define _ATFILE_SOURCE

#include <sys/stat.h>

#include "zdir.h"
#include "zpath.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#ifndef __UNIXSDL__

#define FIND_GOOD(handle) ((handle) != -1)
#define FIND_FAIL(handle) ((handle) == -1)
#define ff_name name
#define ff_fsize size
#define ff_attrib attrib
#define WILD_ALL "*"

// Note, these are faster than the built in DJGPP/MinGW ones
z_DIR* z_opendir(const char* path)
{
    z_DIR* dir = 0;
    if (path && *path) {
        char search[PATH_SIZE];
        strcpy(search, path);
        strcatslash(search);
        strcat(search, WILD_ALL);

        dir = malloc(sizeof(z_DIR));
        if (dir) {
            dir->find_first_handle = _findfirst(search, &dir->fileinfo);
            if (FIND_FAIL(dir->find_first_handle)) {
                // ENOENT set by findfirst already
                free(dir);
                dir = 0;
            }
        } else {
            errno = ENOMEM;
        }
    } else {
        errno = EINVAL;
    }

    return (dir);
}

struct z_dirent* z_readdir(z_DIR* dir)
{
    struct z_dirent* entry = 0;
    if (FIND_GOOD(dir->find_first_handle)) {
        entry = &dir->entry;
        strcpy(entry->d_name, dir->fileinfo.ff_name);
        if (FIND_FAIL(_findnext(dir->find_first_handle, &dir->fileinfo))) {
            _findclose(dir->find_first_handle);
            dir->find_first_handle = -1;
        }
    } else {
        errno = EBADF;
    }

    return (entry);
}

int z_closedir(z_DIR* dir)
{
    int result = 0;

    if (dir) {
        if (FIND_GOOD(dir->find_first_handle)) {
            _findclose(dir->find_first_handle);
        }
        free(dir);
    } else {
        result = -1;
        errno = EBADF;
    }

    return (result);
}

struct dirent_info* readdir_info(z_DIR* dir)
{
    static struct dirent_info info;
    struct dirent_info* infop = 0;

    if (FIND_GOOD(dir->find_first_handle)) {
        strcpy(dir->entry.d_name, dir->fileinfo.ff_name);

        info.name = dir->entry.d_name;
        info.size = dir->fileinfo.ff_fsize;
        info.mode = S_IREAD;
        if (!(dir->fileinfo.ff_attrib & _A_RDONLY)) {
            info.mode |= S_IWRITE;
        }
        if (dir->fileinfo.ff_attrib & _A_SUBDIR) {
            info.mode |= S_IFDIR;
        } else {
            info.mode |= S_IFREG;
        }
        infop = &info;

        if (FIND_FAIL(_findnext(dir->find_first_handle, &dir->fileinfo))) {
            _findclose(dir->find_first_handle);
            dir->find_first_handle = -1;
        }
    } else {
        errno = EBADF;
    }

    return (infop);
}

#else
#include <unistd.h>

#if defined(__GLIBC__) && (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 4))
#include <stdio.h>
/* No fstatat before glibc 2.4 (Yellow Dog Linux 5 and 6 have 2.3/2.5); stat
   the entry through the directory's descriptor in /proc instead. */
static int stat_in_dir(z_DIR* const dir, char const* const name, struct stat* const st)
{
    char path[PATH_MAX];

    if (snprintf(path, sizeof path, "/proc/self/fd/%d/%s", dirfd(dir), name) >= (int)sizeof path) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return stat(path, st);
}
#else
static int stat_in_dir(z_DIR* const dir, char const* const name, struct stat* const st)
{
    return fstatat(dirfd(dir), name, st, 0);
}
#endif

struct dirent_info* readdir_info(z_DIR* dir)
{
    static struct dirent_info info;
    struct dirent_info* infop = 0;

    /* A loop, not a recursion: a directory of entries that cannot be stat'ed -
       broken symlinks, say - used to nest one frame deep per entry. */
    struct dirent* entry;

    while (!infop && (entry = readdir(dir)) != NULL) {
        struct stat stat_buffer;
        if (!stat_in_dir(dir, entry->d_name, &stat_buffer)) {
            info.name = entry->d_name;
            info.size = stat_buffer.st_size;
            info.mode = stat_buffer.st_mode;
            info.uid = stat_buffer.st_uid;
            info.gid = stat_buffer.st_gid;
            infop = &info;
        }
    }
    return (infop);
}

int dirent_access(struct dirent_info* entry, int mode)
{
    int accessable = 0; // This is accessable, non access is -1

    if (!entry) {
        accessable = -1;
        errno = EACCES;
    } else if (mode) {
        uid_t uid = geteuid();
        gid_t gid = getegid();

        if (!(
                (!(mode & R_OK) || ((entry->mode & S_IROTH) || ((gid == entry->gid) && (entry->mode & S_IRGRP)) || ((uid == entry->uid) && (entry->mode & S_IRUSR)))) && (!(mode & W_OK) || ((entry->mode & S_IWOTH) || ((gid == entry->gid) && (entry->mode & S_IWGRP)) || ((uid == entry->uid) && (entry->mode & S_IWUSR)))) && (!(mode & X_OK) || ((entry->mode & S_IXOTH) || ((gid == entry->gid) && (entry->mode & S_IXGRP)) || ((uid == entry->uid) && (entry->mode & S_IXUSR)))))) {
            accessable = -1;
            errno = EACCES;
        }
    }

    return (accessable);
}

#endif
