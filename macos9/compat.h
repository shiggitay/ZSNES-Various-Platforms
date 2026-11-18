/* Included ahead of every file of the Mac OS 9 build (-include): the POSIX
   pieces newlib leaves undeclared for this target, which macos9/posix.c
   provides. */
#ifndef ZSNES_MACOS9_COMPAT_H
#define ZSNES_MACOS9_COMPAT_H

#include <limits.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 1
#endif
#ifndef CLOCK_PROCESS_CPUTIME_ID
#define CLOCK_PROCESS_CPUTIME_ID 2
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 4
#endif

int clock_gettime(clockid_t clock_id, struct timespec* tp);
int nanosleep(const struct timespec* rqtp, struct timespec* rmtp);
int utime(const char* path, const struct utimbuf* times);

#endif
