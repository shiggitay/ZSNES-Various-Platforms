#ifndef ZCLOCK_H
#define ZCLOCK_H

/* A clock that only goes forward, for frame timing.

   CLOCK_MONOTONIC came with Linux 2.6: on a 2.4 kernel (Yellow Dog Linux 3)
   clock_gettime refuses it, and ZSNES, which never looked at the result, saw
   time stand still - no frame ever fell due, the GUI's snow hung in the air
   and a game showed a black screen. Fall back to the wall clock then. */

#include <sys/time.h>
#include <time.h>

static inline void zclock_monotonic(struct timespec* const ts)
{
    static int monotonic = 1;

#ifdef CLOCK_MONOTONIC
    if (monotonic && clock_gettime(CLOCK_MONOTONIC, ts) == 0)
        return;
#endif
    monotonic = 0;
    {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        ts->tv_sec = tv.tv_sec;
        ts->tv_nsec = (long)tv.tv_usec * 1000L;
    }
}

#endif
