/* Included ahead of every file of the PS3 build (-include): the POSIX
   pieces PSL1GHT's newlib lacks, which ps3/posix.c provides. */
#ifndef ZSNES_PS3_COMPAT_H
#define ZSNES_PS3_COMPAT_H

#include <dirent.h>
#include <sys/types.h>
#include <time.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME (clockid_t)1
#endif
#ifndef CLOCK_PROCESS_CPUTIME_ID
#define CLOCK_PROCESS_CPUTIME_ID (clockid_t)2
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC (clockid_t)4
#endif

struct stat;

int ps3_clock_gettime(clockid_t id, struct timespec* tp);
int ps3_nanosleep(struct timespec const* req, struct timespec* rem);
/* The PS3 has no descriptor-relative calls: directories remember the path
   they were opened with, and an entry is looked up by its whole path. */
DIR* ps3_opendir(char const* path);
int ps3_closedir(DIR* dir);
int ps3_dirfd(DIR* dir);
int ps3_fstatat(int fd, char const* name, struct stat* st, int flag);

#define clock_gettime ps3_clock_gettime
#define nanosleep ps3_nanosleep
#define opendir ps3_opendir
#define closedir ps3_closedir
#undef dirfd
#define dirfd ps3_dirfd
#define fstatat ps3_fstatat

#endif
