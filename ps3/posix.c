/* posix.c - what the PS3 build's ZSNES calls and PSL1GHT's newlib does not
   have: the monotonic clock, nanosleep, and stat relative to an open
   directory (see ps3/compat.h). */
#include "compat.h"
/* The real calls here (compat.h is included ahead of this file too, so its
   renamings are already in force). */
#undef clock_gettime
#undef nanosleep
#undef opendir
#undef closedir
#undef dirfd
#undef fstatat

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

int ps3_clock_gettime(clockid_t const id, struct timespec* const tp)
{
    struct timeval tv;

    (void)id; /* one clock serves all three: it only goes forward here */
    if (gettimeofday(&tv, NULL) != 0)
        return -1;
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = (long)tv.tv_usec * 1000;
    return 0;
}

int ps3_nanosleep(struct timespec const* const req, struct timespec* const rem)
{
    unsigned long long const us = (unsigned long long)req->tv_sec * 1000000u + (unsigned long long)req->tv_nsec / 1000u;

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    usleep(us > 0xFFFFFFFFu ? 0xFFFFFFFFu : (unsigned)us);
    return 0;
}

#define MAX_DIRS 16
#define DIR_FD_BASE 1000

static struct {
    DIR* dir;
    char path[1024];
} dirs[MAX_DIRS];

DIR* ps3_opendir(char const* const path)
{
    DIR* const d = opendir(path);
    int i;

    if (!d)
        return NULL;
    for (i = 0; i < MAX_DIRS; i++) {
        if (!dirs[i].dir) {
            dirs[i].dir = d;
            if (path[0] == '/' || !getcwd(dirs[i].path, sizeof dirs[i].path))
                snprintf(dirs[i].path, sizeof dirs[i].path, "%s", path);
            else {
                size_t const n = strlen(dirs[i].path);

                snprintf(dirs[i].path + n, sizeof dirs[i].path - n, "/%s", path);
            }
            break;
        }
    }
    return d;
}

int ps3_closedir(DIR* const d)
{
    int i;

    for (i = 0; i < MAX_DIRS; i++)
        if (dirs[i].dir == d)
            dirs[i].dir = NULL;
    return closedir(d);
}

int ps3_dirfd(DIR* const d)
{
    int i;

    for (i = 0; i < MAX_DIRS; i++)
        if (dirs[i].dir == d)
            return DIR_FD_BASE + i;
    errno = EBADF;
    return -1;
}

int ps3_fstatat(int const fd, char const* const name, struct stat* const st, int const flag)
{
    char path[1280];
    int const i = fd - DIR_FD_BASE;

    (void)flag;
    if (name[0] == '/')
        return stat(name, st);
    if (i < 0 || i >= MAX_DIRS || !dirs[i].dir) {
        errno = EBADF;
        return -1;
    }
    snprintf(path, sizeof path, "%s/%s", dirs[i].path, name);
    return stat(path, st);
}

/* No other programs, and one user, on the PS3: the callers (opening a web
   page, the movie encoders, finding the home folder) fall back. */
int execvp(char const* const file, char* const argv[])
{
    (void)file;
    (void)argv;
    errno = ENOSYS;
    return -1;
}

uid_t getuid(void) { return 0; }

struct passwd* getpwuid(uid_t const uid)
{
    (void)uid;
    return NULL;
}

struct passwd* getpwnam(char const* const name)
{
    (void)name;
    return NULL;
}

/* macos9/net_stub.c's clock, which the PS3 build shares. */
uint64_t macos9_microseconds(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000u + (uint64_t)tv.tv_usec;
}
